#include <openssl/md5.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <string>
#include <curl/curl.h>
#include <vector>
#include <map>
#include <filesystem>
#include <mutex>
#include <random>
#include <chrono>
#include <thread>
#include <ctime>
#include <system_error>


using namespace std;
namespace fs = filesystem;


// ===================
// Logger 類別
// ===================
class Logger {
public:
    Logger(const string& fileName) : logFile(fileName, ios::app), stopCompress(false) {
        if (!logFile.is_open()) {
            cerr << "無法開啟 log 檔案: " << fileName << endl;
        }
        compressThread = thread(&Logger::compressLog, this);
    }

    ~Logger() {
        if (logFile.is_open()) {
            logFile.close();
        }
        stopCompress = true;
        if (compressThread.joinable()) {
            compressThread.join(); // 等待壓縮執行緒結束
        }
    }

    void info(const string& message) {
        writeLog("Info", message);
    }
    
    void debug(const string& message) {
        writeLog("Debug", message);
    }

    void warning(const string& message) {
        writeLog("Warning", message);
    }

    void error(const string& message) {
        writeLog("Error", message);
    }

private:
    ofstream logFile;
    mutex logMutex;
    thread compressThread;
    bool stopCompress;
    size_t maxFileSize = 10 * 1024 * 1024; // 10MB
    // int compressInterval = 10; // 每隔 n 秒壓縮一次

    // 寫入 log
    void writeLog(const string& level, const string& message) {
        lock_guard<mutex> guard(logMutex); // guard 會在生命週期結束時被銷毀，並自動釋放鎖
        auto now = chrono::system_clock::now();
        auto timeNow = chrono::system_clock::to_time_t(now);

        tm tm{};
        localtime_r(&timeNow, &tm);

        logFile << "[" << level << "]\t"
                << put_time(&tm, "%Y/%m/%d %H:%M:%S") << " "
                << message << endl;
    }


    // 壓縮 log 檔
    void compressLog() {
        const string sourceDir = "/home/pascal/projects/helloworld/upgrade-client/upgrade/logs";
        const string targetDir = "/home/pascal/projects/helloworld/upgrade-client/upgrade/tarlogs";

        while (!stopCompress) {
            // this_thread::sleep_for(chrono::seconds(compressInterval)); // 每隔 compressInterval 秒執行一次
            
            fs::create_directories(targetDir); // 確保目標目錄存在
            
            auto now = chrono::system_clock::now();
            auto timeNow = chrono::system_clock::to_time_t(now);

            string sourceFile = sourceDir + "/upgrade.log";
            if (fs::exists(sourceFile) && fs::file_size(sourceFile) > maxFileSize) {
                // 壓縮 log 檔
                tm tmForFileName{};
                localtime_r(&timeNow, &tmForFileName);
                char buffer[80] ={0};
                strftime(buffer, sizeof(buffer), "%Y%m%d%H%M%S", &tmForFileName);
                string targetFileName = string(buffer) + ".tar.gz";
                
                string cmd = "tar -czf " + targetDir + "/" + targetFileName + " -C " + sourceDir + " upgrade.log";
                system(cmd.c_str());
                cout << "壓縮 log 檔完成: " << sourceFile << " -> " << targetDir << "/" << targetFileName << endl;

                // 清理 log 檔
                ofstream file(sourceFile, ios::out | ios::trunc);
                if (!file.is_open()) {
                    cerr << "無法開啟檔案以清除內容: " << sourceFile << endl;
                } else {
                    file.close();
                }
            }


            // 清理超過14天的壓縮檔
            try {
                for (const auto& entry : fs::directory_iterator(targetDir)) {
                    string fileName = entry.path().filename().string();
        
                    if (fileName.length() >= 14) { // YYYYMMDDHHMMSS.tar.gz
                        tm fileDate{};
                        string timeStr = fileName.substr(0, 14); // YYYYMMDDHHMMSS
                        strptime(timeStr.c_str(), "%Y%m%d%H%M%S", &fileDate);

                        auto fileTime = chrono::system_clock::from_time_t(mktime(&fileDate)); // tm 轉 time_t，再轉 chrono::system_clock::time_point
                        auto duration = chrono::duration_cast<chrono::hours>(now - fileTime);

                        if (duration.count() > 24 * 14) { // 檢查是否超過14天
                            fs::remove(entry.path()); // 刪除檔案
                            cout << "刪除超過14天的壓縮檔: " << fileName << endl;
                        }

                    }
                }
            } catch (const exception& e) {
                cerr << "清理壓縮檔時發生錯誤: " << e.what() << endl;
            }
        }
    }

};

Logger logger = Logger("/home/pascal/projects/helloworld/upgrade-client/upgrade/logs/upgrade.log");


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
    random_device rd; // 隨機數
    mt19937 gen(rd()); // 以隨機數為種子，產生 0-2^32-1 的隨機數
    uniform_int_distribution<> distrib(100, 2000); // 生成均勻分佈的隨機數
    int sleepTime = distrib(gen); // 隨機 100-2000 毫秒
    logger.info("Waiting for " + to_string(sleepTime) + " milliseconds");
    this_thread::sleep_for(chrono::milliseconds(sleepTime)); // 等待隨機 100-2000 毫秒


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
    
    string filePath = "/home/pascal/projects/helloworld/upgrade-client/upgrade/config/upgrade.cfg";
    ifstream configFile(filePath);
    string line;
    map<string, string> configValues;


    // 1. 讀取 cfg 設定檔
    cout << "[Step 1] 讀取 cfg 設定檔 " << endl;
    if (configFile.is_open()) {
        cout << "讀取 " << filePath << endl << "==================================================" << endl;
        while (getline(configFile, line)) {
            size_t delimiterPos = line.find("=");
            if (delimiterPos != string::npos) {
                string key = line.substr(0, delimiterPos);
                string value = line.substr(delimiterPos + 1);
                configValues[key] = value;
                cout << key << " = " << value << endl;
            }
        }
        cout << "==================================================" << endl;
        configFile.close();
    } else {
        cout << "無法打開配置文件: " << filePath << endl;
        return 1;
    }

    string upgradeServerUrl = configValues["Upgrade_Server_Url"];
    string upgradeServerPath = configValues["Upgrade_Server_Path"];
    string appName = configValues["App_Name"];
    string version = configValues["Version"];
    string appDir = configValues["App_Dir"];
    string downloadDir = configValues["Download_Dir"];
    string beforeUpgradeScript = configValues["Before_Upgrade_Script"];
    string afterUpgradeScript = configValues["After_Upgrade_Script"];
    string needBackup = configValues["Need_Backup"];



    // 2. 執行 before_upgrade.sh
    logger.info("");
    logger.info("[Step 2] 執行更新前腳本");
    if (fs::exists(beforeUpgradeScript)) {
        logger.info("執行 " + beforeUpgradeScript);
        system(beforeUpgradeScript.c_str());
    } else {
        logger.error("腳本不存在: " + beforeUpgradeScript);
    }


    // 3. 清空並重新建立暫存區與備份區
    logger.info("");
    logger.info("[Step 3] 清空並重新建立暫存區與備份區");
    if (fs::exists(downloadDir)) {
        logger.info("刪除暫存區目錄 " + downloadDir);
        fs::remove_all(downloadDir);
    } else {
        logger.info("暫存區目錄不存在: " + downloadDir);
    }
    logger.info("創建暫存區目錄 " + downloadDir);
    fs::create_directory(downloadDir);

    string backupDir = appDir + "/backup";
    if (fs::exists(backupDir)) {
        logger.info("刪除備份區目錄 " + backupDir);
        fs::remove_all(backupDir);
    } else {
        logger.info("備份區目錄不存在: " + backupDir);
    }
    logger.info("創建備份區目錄 " + backupDir);
    fs::create_directory(backupDir);


    // 4. 發送 POST 請求取得檔案簽章
    logger.info("");
    logger.info("[Step 4] 向 Server 取得檔案簽章");
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
    string remoteSignature = extractXMLContent(response, "signature");
    map<string, string> remoteFileSignature = parseFileSignature(base64_decode(remoteSignature));
    logger.info("檢查更新數量: " + to_string(remoteFileSignature.size()));


    // 5. 比對本地檔案簽章與遠端檔案簽章，若不存在或不一致則紀錄
    logger.info("");
    logger.info("[Step 5] 比對本地檔案簽章與遠端檔案簽章");

    vector<string> needDownloadFiles;
    for (int i = 1;const auto& [filePath, md5Signature] : remoteFileSignature) {
        fs::path localFilePath = fs::path(appDir + filePath);
        // 檢查文件是否存在
        if (fs::exists(localFilePath)) {
            string localSignature = getFileSignature(localFilePath.string());
            if (localSignature != md5Signature) {
                logger.info("File [" + to_string(i) + "]: " + localFilePath.string() + "..................Updated(" + localSignature + " -> " + md5Signature + ")");
                needDownloadFiles.push_back(filePath); // 紀錄在 vector 中，之後一次性下載
            }
        } else {
            logger.info("File [" + to_string(i) + "]: " + localFilePath.string() + ".................New");
            needDownloadFiles.push_back(filePath);
        }
        i++;
    }
    logger.info("不需更新數量: " + to_string(remoteFileSignature.size() - needDownloadFiles.size()));
    logger.info("需要更新數量: " + to_string(needDownloadFiles.size()));


    // 6. 下載需要更新的檔案
    logger.info("");
    logger.info("[Step 6] 下載需要更新的檔案");
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
        string remoteContent = extractXMLContent(response, "content");

        string downloadFilePath = downloadDir + filePath;
        fs::path downloadFileDir = fs::path(downloadFilePath).parent_path();
        if (!fs::exists(downloadFileDir)) {
            logger.info("創建目錄 " + downloadFileDir.string());
            fs::create_directories(downloadFileDir);
        }
        ofstream downloadFile(downloadFilePath, ios::binary);
        downloadFile << base64_decode(remoteContent);
        logger.info("Downloaded file " + downloadFilePath);
        downloadFile.close();

        // 檢查檔案簽章是否一致
        string localSignature = getFileSignature(downloadFilePath);
        string remoteSignature = remoteFileSignature[filePath];
        if (localSignature != remoteSignature) {
            logger.error("Signature is not equal, [Local signature]: " + localSignature + " [Remote signature]: " + remoteSignature);
            logger.error("Download failed, remove all files in " + downloadDir);
            fs::remove_all(downloadDir);
            // [TODO] 可能需要重新下載
            return 1;
        }
        logger.info("Signature is equal, [Local signature]: " + localSignature + " [Remote signature]: " + remoteSignature);
    }
    logger.info("==================================================");
    logger.info("下載完成數量: " + to_string(needDownloadFiles.size()));


    // 7. 移動下載完成的檔案至 appDir，並設定檔案權限為755
    logger.info("");
    logger.info("[Step 7] 移動下載完成的檔案至 appDir, 並設定檔案權限為755");
    bool moveResult = true;
    for (const auto& filePath : needDownloadFiles) {
        logger.info("=======================================[" + filePath + "]");
        string appFilePath = appDir + filePath;

        // 檔案存在才需備份
        if (fs::exists(appFilePath)) {
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

    if (!moveResult) {
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


    // 8. 移動完成後刪除暫存區與備份區
    logger.info("");
    logger.info("[Step 8] 移動完成後刪除暫存區與備份區");
    fs::remove_all(downloadDir);
    logger.info("刪除暫存區 " + downloadDir);
    fs::remove_all(backupDir);
    logger.info("刪除備份區 " + backupDir);


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
