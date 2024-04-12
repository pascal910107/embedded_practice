// 檔案處理
#include  <fstream>
#include <map>
#include <sstream>
#include <iostream>
#include <string>
#include <cstdio>
#include <filesystem>
#include <nlohmann/json.hpp>

using namespace std;


// 檔案文件開、讀、寫 操控
void controlFile() {
    string filename = "config.txt";
    // nlohmann::json object;
    // 讀
    {
        ifstream file(filename);
        if (!file.is_open()) {
            cout << "cannot open file for reading." << endl;
            return;
        }
        // file >> object;
        cout << "file content: " << endl << file.rdbuf() << endl;
        file.close();
    }

    // object["sid"] = "0211e144d9650b3a24b933e11f8cdec384f8d806";
    // cout << object.dump(4) << endl;
    // 寫
    {
        ofstream file(filename, ios::app); // 追加内容
        if (!file.is_open()) {
            cout << "cannot open file for writing." << endl;
            return;
        }
        // file << object.dump(4);
        string sid = "\"sid\":\"0211e144d9650b3a24b933e11f8cdec384f8d806\"";
        file << sid;
        file.close();
    }

    // // 重置文件讀取指針到開頭
    // file.clear();  // 清除任何存在的結束標誌等
    // file.seekg(0, ios::beg);  // 移動指針到文件開頭
}

// 檔案內容讀取解析、修改（Json、formdata）
void parseFile() {
    // 1.
    // 讀取 config.txt 內容並解析出 Rs232Reader 參數的值
    {
        nlohmann::json object;
        ifstream file("config.txt");
        if (!file.is_open()) {
            cout << "cannot open file for reading." << endl;
            return;
        }
        file >> object;
        file.close();

        string rs232 = object["Rs232Reader"];
        cout << "Rs232Reader" << rs232 << endl;
    }
    
    // 2.
    string filename = "test.txt";
    // 建立並寫入檔案
    {
        ofstream file(filename);
        if (!file.is_open()) {
            cout << "cannot open file for writing." << endl;
            return;
        }
        file << "deviceNo=89798198&eccStatus=1&sdUseSpace=99&eccSamId=41fa621b260744ce165&mainVersion=V1.0.0.0";
        file.close();
    }
    // 讀取檔案並處理
    map<string, string> params;
    {
        ifstream file(filename);
        if (!file.is_open()) {
            cout << "cannot open file for reading." << endl;
            return;
        }
        string line;
        while (getline(file, line)) {
            stringstream ss(line); // 將文件內容的字符串賦值給 stringstream 對象 ss，使字串成為可操作的字符串流
            string item;
            while (getline(ss, item, '&')) {
                size_t pos = item.find("=");
                if (pos != string::npos) {
                    string key = item.substr(0, pos);
                    string value = item.substr(pos + 1);
                    params[key] = value; // 如果同一鍵多次出現，後值覆蓋前值
                }
            }
        }
        file.close();
    }
    // 輸出參數值
    for (const auto& param : params) {
        cout << param.first << " = " << param.second << endl;
    }
}

// 檔案刪除、更名、搬移、檔案大小
void changeFileState() {
    // 創建兩個空檔案
    {
        ofstream outFile1("file1.txt");
        ofstream outFile2("file2.txt");
        if (!outFile1 || !outFile2) {
            cerr << "無法創建一個或多個檔案" << endl;
            return;
        }
        outFile1.close();
        outFile2.close();
    }
    // 刪除檔案
    if (remove("file1.txt") != 0) {
        perror("Error deleting file"); // 自定義錯誤消息，顯示在原本錯誤消息前
    }
    // 更名
    if (rename("file2.txt", "file2_new.txt") != 0) {
        perror("Error renaming file");
    }
    // 搬移檔案
    if (rename("file2_new.txt", "temp/file2_new.txt") != 0) {
        perror("Error moving file");
    }
    // 獲取檔案空間大小 xxxx bytes/K
    {
        ifstream file("config.txt", ios::ate | ios::binary); // 文件打開後定位到文件尾
        if (!file.is_open()) {
            cout << "cannot open file for reading." << endl;
            return;
        }
        auto fileSize = file.tellg(); // 返回從文件開始到當前文件指標的偏移量，也就是文件大小
        cout << "File size: " << fileSize << " bytes" << endl;

        // 轉換為 KB
        // double fileSizeKB = fileSize / 1024.0; // 二進制標準
        double fileSizeKB = fileSize / 1000.0; // SI標準
        cout << "File size: " << fileSizeKB << " KB" << endl;

        file.close();
    }
}

// 資料夾建立、空間大小
void createFile(const string& path, size_t size) {
    ofstream file(path, ios::binary);
    if (!file.is_open()) {
        cerr << "無法建立檔案：" << path << endl;
        return;
    }
    
    // 生成1MB以上的內容
    string content(size, '0'); // 創建長度為size的字符串，字符串中的每個字都是'0'
    file.write(content.c_str(), content.size());
    file.close();
}

void createFolder() {
    string dirPath = "testDir";
    // 建立資料夾
    if (!filesystem::create_directory(dirPath)) {
        cout << "資料夾已存在或建立失敗：" << dirPath << endl;
        return;
    }

    // 在資料夾中建立數個檔案
    createFile(dirPath + "/file1.txt", 1024 * 1024 + 1); // 1MB + 1byte
    createFile(dirPath + "/file2.txt", 1024 * 1024 + 2); // 1MB + 2bytes

    // 計算資料夾大小
    uintmax_t size = 0;
    for (const auto& entry : filesystem::recursive_directory_iterator(dirPath)) {
        if (entry.is_regular_file()) {
            size += filesystem::file_size(entry.path());
        }
    }

    cout << "資料夾大小：" << size << " bytes" << endl;
    return;
}

// log檔案機制

int main() {

    // controlFile();
    // parseFile();
    // changeFileState();
    createFolder();
    return 0;

}