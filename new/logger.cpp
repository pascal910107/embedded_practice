#include "logger.h"
#include "config.h"


using namespace std;
namespace fs = filesystem;


// ===================
// Logger 類別
// ===================

Logger::Logger(const string& fileName) : stopCompress(false) {
    try {
        readConfig();
    } catch (const exception& e) {
        cerr << "[錯誤] " << e.what() << endl;
        throw;
    }
    logFile.open(appDir + fileName, ios::app);
    if (!logFile.is_open()) {
        cerr << "無法開啟 log 檔案: " << fileName << endl;
        throw system_error(errno, system_category(), "無法開啟 log 檔案");
    }

    maxFileSize = 10 * 1024 * 1024; // 10MB

    compressThread = thread(&Logger::compressLog, this);
}

Logger::~Logger() {
    if (logFile.is_open()) {
        logFile.close();
    }
    stopCompress = true;
    if (compressThread.joinable()) {
        compressThread.join(); // 等待壓縮執行緒結束
    }
}

void Logger::info(const string& message) {
    writeLog("Info", message);
}

void Logger::debug(const string& message) {
    writeLog("Debug", message);
}

void Logger::warning(const string& message) {
    writeLog("Warning", message);
}

void Logger::error(const string& message) {
    writeLog("Error", message);
}


// 寫入 log
void Logger::writeLog(const string& level, const string& message) {
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
void Logger::compressLog() {
    try {
        readConfig(); // 因為 compressLog 是在另一個執行緒中執行，所以需要重新讀取設定
    } catch (const exception& e) {
        cerr << "[錯誤] " << e.what() << endl;
        throw;
    }
    const string sourceDir = appDir + "/upgrade/logs";
    const string targetDir = appDir + "/upgrade/tarlogs";


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


