#include "logger.h"
#include "config.h"

#include <iostream>
#include <fstream>
#include <map>
#include <string>
#include <filesystem>
#include <thread>
#include <mutex>
#include <system_error>
#include <sstream>
#include <openssl/md5.h>
#include <curl/curl.h>
#include <vector>
#include <random>
#include <stdexcept>


using namespace std;
namespace fs = filesystem;



Logger logger = Logger("/upgrade/logs/upgrade.log");


// ===================
// 用於 Base64 編碼的 64 個字符
// ===================
static const string base64_chars = 
             "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
             "abcdefghijklmnopqrstuvwxyz"
             "0123456789+/";


// ===================
// 計算 MD5 雜湊值
// ===================
string md5(const string &input) {
    unsigned char digest[MD5_DIGEST_LENGTH]; // 存計算出的 MD5 雜湊值 (16 bytes)
    MD5((unsigned char*)input.c_str(), input.length(), (unsigned char*)&digest);
    stringstream ss;
    for(int i = 0; i < MD5_DIGEST_LENGTH; i++)
        ss << hex << setw(2) << setfill('0') << (int)digest[i]; // 每個 byte（16 個） 轉成 16 進位並補 0
    return ss.str();
}


// ===================
// Base64 解碼
// ===================
string base64_decode(string const& encoded_string) {
    int in_len = encoded_string.size();
    int i = 0, j = 0, in_ = 0;
    unsigned char char_array_4[4], char_array_3[3];
    string ret;

    // while 條件確保只有有效的 Base64 字符才會進行解碼（字母數字（isalnum）、+、/）
    while (in_len-- && ( encoded_string[in_] != '=') && isalnum(encoded_string[in_]) || (encoded_string[in_] == '+') || (encoded_string[in_] == '/')) {
        char_array_4[i++] = encoded_string[in_]; in_++; // 將待解碼的 Base64 字符存入 char_array_4
        if (i ==4) {
            for (i = 0; i <4; i++)
                char_array_4[i] = base64_chars.find(char_array_4[i]); // 將 Base64 字符轉換為對應的 6-bit 數據，ex: A -> 000000 (0), B -> 000001 (1), ...

            char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4); // 取出第一個 6-bit 字節的前 6 位和第二個 6-bit 字節的前 2 位，即第一個 8-bit 字節
            char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2); // 取出第二個 6-bit 字節的後 4 位和第三個 6-bit 字節的前 4 位，即第二個 8-bit 字節
            char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3]; // 取出第三個 6-bit 字節的後 2 位和第四個 6-bit 字節的全部，即第三個 8-bit 字節

            for (i = 0; (i < 3); i++)
                ret += char_array_3[i];
            i = 0;
        }
    }

    // 處理最後不足 4 字節的情況，補 0
    if (i) {
        for (j = i; j <4; j++)
            char_array_4[j] = 0;

        for (j = 0; j <4; j++)
            char_array_4[j] = base64_chars.find(char_array_4[j]);

        char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
        char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
        char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];

        for (j = 0; (j < i - 1); j++) ret += char_array_3[j];
    }

    logger.info("Base64 decode success");
    return ret;
}


// ===================
// Response 處理函數
// ===================
static size_t WriteCallback(void *contents, size_t size, size_t nmemb, string *userp) { // userp 是指向緩衝區的指針
    userp->append((char*)contents, size * nmemb); // 將收到的數據附加到 userp 指向的緩衝區
    return size * nmemb; // 每個數據塊的大小 * 數據塊數量 = 總數據大小
}


// ===================
// 傳送 POST 請求
// ===================
string sendPostRequest(const string& url, const string& postFields) {

    CURL *curl;
    CURLcode res;
    string readBuffer;

    curl = curl_easy_init();
    if(curl) {
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, postFields.c_str());

        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L); // 不驗證證書
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L); // 不檢查證書主機

        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);

        logger.info("Sending POST request to " + url + " with fields: \"" + postFields + "\"");
        res = curl_easy_perform(curl);
        if(res != CURLE_OK) {
            stringstream ss;
            ss << "curl_easy_perform() failed: " << curl_easy_strerror(res);
            logger.error(ss.str());
            return "";
        }

        curl_easy_cleanup(curl);
        return readBuffer;
    }
    logger.error("curl_easy_init() failed");
    return "";
}


// ===================
// 拆解 XML 內容
// ===================
string extractXMLContent(const string& xml, const string& tagName) {
    string openTag = "<" + tagName + ">";
    string closeTag = "</" + tagName + ">";
    size_t startPos = xml.find(openTag) + openTag.length();
    size_t endPos = xml.find(closeTag);
    if (startPos != string::npos && endPos != string::npos) {
        // logger.info("Extracted <" + tagName + "> from XML: " + xml.substr(startPos, endPos - startPos));
        cout << "Extracted <" << tagName << "> from XML: " << xml.substr(startPos, endPos - startPos) << endl;
        return xml.substr(startPos, endPos - startPos);
    }
    logger.error("Failed to extract " + tagName + " from XML: " + xml);
    return "";
}


// ===================
// 拆解檔案與簽章
// ===================
map<string, string> parseFileSignature(const string& signature) {
    map<string, string> fileSignature;
    istringstream stream(signature);
    string line;
    
    while (getline(stream, line)) {
        string delimiters = " \t";
        size_t firstDelimiterPos = line.find_first_of(delimiters);
        if (firstDelimiterPos == string::npos) {
            continue;
        }
        size_t secondWordStart = line.find_first_not_of(delimiters, firstDelimiterPos);
        if (secondWordStart == string::npos) {
            continue;
        }

        string filePath = line.substr(0, firstDelimiterPos);
        string md5Signature = line.substr(secondWordStart);
        fileSignature[filePath] = md5Signature;
    }

    logger.info("Parsed file signature success");
    for (int i = 1; auto& it : fileSignature) {
        logger.info("File [" + to_string(i) + "]: " + it.first + "................." + it.second);
        i++;
    }
    return fileSignature;
}

// ===================
// 執行系統命令
// ===================
string exec(const char* cmd) {
    array<char, 128> buffer;
    string result;
    unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd, "r"), pclose);
    if (!pipe) {
        throw runtime_error("popen() failed!");
    }
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) { // fgets() 讀取一行文字到 buffer 中
        result += buffer.data();
    }
    return result;
}


// ===================
// 取得檔案簽章
// ===================
string getFileSignature(const string& filePath) {
    string command = "md5sum " + filePath;
    string result = exec(command.c_str());
    // 去掉雜湊值後面的檔名
    size_t delimiterPos = result.find(" ");
    if (delimiterPos != string::npos) {
        result = result.substr(0, delimiterPos);
    }
    return result;
}




int main() {
    try {
        readConfig();
    } catch (const exception& e) {
        cerr << "[錯誤] " << e.what() << endl;
        return EXIT_FAILURE;
    }

    random_device rd; // 隨機數
    mt19937 gen(rd()); // 以隨機數為種子，產生 0-2^32-1 的隨機數
    uniform_int_distribution<> distrib(300000, 600000); // 生成均勻分佈的隨機數
    int sleepTime = distrib(gen); // 隨機 300-600 秒
    logger.info("Waiting for " + to_string(sleepTime) + " milliseconds");
    this_thread::sleep_for(chrono::milliseconds(sleepTime)); // 等待隨機 300-600 秒


    // 1. 執行 before_upgrade.sh
    logger.info("");
    logger.info("[Step 1] 執行更新前腳本");
    if (fs::exists(beforeUpgradeScript)) {
        logger.info("執行 " + beforeUpgradeScript);
        system(beforeUpgradeScript.c_str());
    } else {
        logger.error("腳本不存在: " + beforeUpgradeScript);
    }


    // 2. 清空並重新建立暫存區與備份區
    logger.info("");
    logger.info("[Step 2] 清空並重新建立暫存區與備份區");
    if (fs::exists(downloadDir)) {
        logger.info("刪除暫存區目錄 " + downloadDir);
        fs::remove_all(downloadDir);
    } else {
        logger.info("暫存區目錄不存在: " + downloadDir);
    }
    logger.info("創建暫存區目錄 " + downloadDir);
    fs::create_directory(downloadDir);

    string backupDir = appDir + "/backup";
    if (needBackup == "true") {
        if (fs::exists(backupDir)) {
            logger.info("刪除備份區目錄 " + backupDir);
            fs::remove_all(backupDir);
        } else {
            logger.info("備份區目錄不存在: " + backupDir);
        }
        logger.info("創建備份區目錄 " + backupDir);
        fs::create_directory(backupDir);
    } else {
        logger.info("不需要備份");
    }


    // 3. 發送 POST 請求取得檔案簽章
    logger.info("");
    logger.info("[Step 3] 向 Server 取得檔案簽章");
    string url = upgradeServerUrl + upgradeServerPath;
    string postFields = "func=get_signature&appname=" + appName + "&version=" + version;
    string response = sendPostRequest(url, postFields);
    
    string retcode = extractXMLContent(response, "retcode");
    if (retcode != "1") {
        logger.error("Retcode is " + retcode + ", API request failed");
        // [TODO] 可能要重發請求
        return 1;
    }
    logger.info("Retcode is 1, API request succeeded");
    string serverSignature = extractXMLContent(response, "signature");
    map<string, string> serverFileSignature = parseFileSignature(base64_decode(serverSignature));
    logger.info("檢查更新數量: " + to_string(serverFileSignature.size()));


    // 4. 比對本地檔案簽章與伺服器端檔案簽章，若不存在或不一致則紀錄
    logger.info("");
    logger.info("[Step 4] 比對本地檔案簽章與伺服器端檔案簽章");

    vector<string> needDownloadFiles;
    for (int i = 1;const auto& [filePath, md5Signature] : serverFileSignature) {
        fs::path clientFilePath = fs::path(appDir + filePath);
        // 檢查文件是否存在
        if (fs::exists(clientFilePath)) {
            string clientSignature = getFileSignature(clientFilePath.string());
            if (clientSignature != md5Signature) {
                logger.info("File [" + to_string(i) + "]: " + clientFilePath.string() + "..................Updated(" + clientSignature + " -> " + md5Signature + ")");
                needDownloadFiles.push_back(filePath); // 紀錄在 vector 中，之後一次性下載
            }
        } else {
            logger.info("File [" + to_string(i) + "]: " + clientFilePath.string() + ".................New");
            needDownloadFiles.push_back(filePath);
        }
        i++;
    }
    logger.info("不需更新數量: " + to_string(serverFileSignature.size() - needDownloadFiles.size()));
    logger.info("需要更新數量: " + to_string(needDownloadFiles.size()));


    // 5. 下載需要更新的檔案
    logger.info("");
    logger.info("[Step 5] 下載需要更新的檔案");
    for (const auto& filePath : needDownloadFiles) {
        logger.info("=======================================[" + filePath + "]");
        postFields = "func=get_file&appname=" + appName + "&version=" + version + "&fn=" + filePath;
        response = sendPostRequest(url, postFields);

        string retcode = extractXMLContent(response, "retcode");
        if (retcode != "1") {
            logger.error("Retcode is " + retcode + ", API request failed");
            // [TODO] 可能要重發請求
            return 1;
        }
        logger.info("Retcode is 1, API request succeeded");
        string serverontent = extractXMLContent(response, "content");

        string downloadFilePath = downloadDir + filePath;
        fs::path downloadFileDir = fs::path(downloadFilePath).parent_path();
        if (!fs::exists(downloadFileDir)) {
            logger.info("創建目錄 " + downloadFileDir.string());
            fs::create_directories(downloadFileDir);
        }
        ofstream downloadFile(downloadFilePath, ios::binary);
        downloadFile << base64_decode(serverontent);
        logger.info("Downloaded file " + downloadFilePath);
        downloadFile.close();

        // 檢查檔案簽章是否一致
        string clientSignature = getFileSignature(downloadFilePath);
        string serverSignature = serverFileSignature[filePath];
        if (clientSignature != serverSignature) {
            logger.error("Signature is not equal, [Client signature]: " + clientSignature + " [Server signature]: " + serverSignature);
            logger.error("Download failed, remove all files in " + downloadDir);
            fs::remove_all(downloadDir);
            // [TODO] 可能需要重新下載
            return 1;
        }
        logger.info("Signature is equal, [Client signature]: " + clientSignature + " [Server signature]: " + serverSignature);
    }
    logger.info("==================================================");
    logger.info("下載完成數量: " + to_string(needDownloadFiles.size()));


    // 6. 移動下載完成的檔案至 appDir，並設定檔案權限為755
    logger.info("");
    logger.info("[Step 6] 移動下載完成的檔案至 appDir, 並設定檔案權限為755");
    bool moveResult = true;
    for (const auto& filePath : needDownloadFiles) {
        logger.info("=======================================[" + filePath + "]");
        string appFilePath = appDir + filePath;

        // 檔案存在才需備份
        if (fs::exists(appFilePath) && needBackup == "true") {
            // 備份需要更新的檔案
            string backupFilePath = backupDir + filePath;
            fs::path backupFileDir = fs::path(backupFilePath).parent_path();
            if (!fs::exists(backupFileDir)) {
                logger.info("創建備份目錄 " + backupFileDir.string());
                fs::create_directories(backupFileDir);
            }
            fs::copy_file(appFilePath, backupFilePath, fs::copy_options::overwrite_existing); // 因為是空的所以可以不用錯誤處理
            logger.info("Backup file\t" + backupFilePath);
        }

        // 移動下載完成的檔案至 appDir（更新）
        string downloadFilePath = downloadDir + filePath;
        fs::path appFileDir = fs::path(appFilePath).parent_path();
        if (!fs::exists(appFileDir)) {
            logger.info("創建目錄 " + appFileDir.string());
            fs::create_directories(appFileDir);
        }
        try {
            bool copyResult = fs::copy_file(downloadFilePath, appFilePath, fs::copy_options::overwrite_existing);
            if (copyResult) {
                logger.info("Moved file\t" + downloadFilePath + "\tto\t" + appFilePath);
            }
        } catch (const fs::filesystem_error& e) {
            logger.error("Error: " + string(e.what()));
            moveResult = false;
            break;
        }
        system(("chmod 755 " + appFilePath).c_str());
        logger.info("Set file permission to 755");
    }

    if (!moveResult && needBackup == "true") {
        // 回滾備份區的檔案
        logger.error("移動檔案失敗, 回滾備份區的檔案");
        for (const auto& filePath : needDownloadFiles) {
            string appFilePath = appDir + filePath;
            string backupFilePath = backupDir + filePath;
            if (fs::exists(backupFilePath)) {
                fs::copy_file(backupFilePath, appFilePath, fs::copy_options::overwrite_existing);
                logger.info("Rollback file " + backupFilePath);
            }
        }
        return 1;
        
    }
    logger.info("==================================================");
    logger.info("移動完成數量: " + to_string(needDownloadFiles.size()));


    // 7. 移動完成後刪除暫存區與備份區
    logger.info("");
    logger.info("[Step 7] 移動完成後刪除暫存區與備份區");
    fs::remove_all(downloadDir);
    logger.info("刪除暫存區 " + downloadDir);
    if (needBackup == "true") {
        fs::remove_all(backupDir);
        logger.info("刪除備份區 " + backupDir);
    }


    // 9. 執行 after_upgrade.sh
    logger.info("");
    logger.info("[Step 9] 執行更新後腳本");
    if (fs::exists(afterUpgradeScript)) {
        logger.info("執行 " + afterUpgradeScript);
        system(afterUpgradeScript.c_str());
    } else {
        logger.error("腳本不存在: " + afterUpgradeScript);
    }
    logger.info("");

    return 0;
}
