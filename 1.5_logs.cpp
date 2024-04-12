#include <iostream>
#include <chrono>
#include <filesystem>
#include <sstream>
#include <iomanip>
#include <fstream>
#include <cstdlib>
#include <vector>
#include <mutex>

using namespace std;
namespace fs = filesystem;

class Logger {
public:
    Logger() {
        // 檢查並壓縮超過1天的檔案，刪除超過7天的壓縮檔案
        compressOrDeleteOldLogs();
        openLogFile();
    }

    ~Logger() {
        if (logStream.is_open()) {
            logStream.close();
        }
    }

    void Info(const string& message) {
        // 每次寫入前先檢查當前時間與 log 檔日期是否相同
        checkAndUpdateLogFile();
        // 每次寫入前檢查並切換超過大小的檔案
        checkAndRotateLogBySize();
        writeLog("Info", message);
    }
    
    void Debug(const string& message) {
        checkAndUpdateLogFile();
        checkAndRotateLogBySize();
        writeLog("Debug", message);
    }

    void Warning(const string& message) {
        checkAndUpdateLogFile();
        checkAndRotateLogBySize();
        writeLog("Warning", message);
    }

    void Error(const string& message) {
        checkAndUpdateLogFile();
        checkAndRotateLogBySize();
        writeLog("Error", message);
    }

private:
    ofstream logStream;
    string logFilePath;
    size_t maxSize = 10 * 1024 * 1024; // 10MB
    string currentDate;
    mutex log_mutex;

    void openLogFile() {
        // 獲取當前時間
        auto now = chrono::system_clock::now();
        auto timeNow = chrono::system_clock::to_time_t(now); // system_clock 轉 time_t（Unix 时间戳）

        // 轉換時間為 tm 結構以便於格式化
        tm tm{};
        localtime_r(&timeNow, &tm); // time_t 轉 tm（struct，年、月、日、小時、分、秒等），使用 localtime_r 為線程安全（要自己提供 tm 結構的指針）

        // 格式化時間
        ostringstream oss;
        oss << put_time(&tm, "%Y/%m"); // tm 轉 string，第二個參數是要轉的格式
        string datePath = oss.str();

        // 創建目錄路徑
        string logPath = "logs/" + datePath;
        if (!fs::exists(logPath)) {
            fs::create_directories(logPath);
        }

        ostringstream fileNameStream;
        fileNameStream << put_time(&tm, "%Y-%m-%d") << ".log";
        string fileName = fileNameStream.str();

        logFilePath = logPath + "/" + fileName;
        logStream.open(logFilePath, ios::app); //  以追加模式打開 log
    }

    // 檢查當前時間與 log 檔日期是否相同
    void checkAndUpdateLogFile() {
        auto now = chrono::system_clock::now();
        auto timeNow = chrono::system_clock::to_time_t(now);
        tm tm{};
        localtime_r(&timeNow, &tm);
        ostringstream oss;
        oss << put_time(&tm, "%Y-%m-%d");
        string newDate = oss.str();

        // 如果不同就開啟新日期的 log 檔
        if (currentDate == newDate) {
            return;
        }
        currentDate = newDate;
        if (logStream.is_open()) {
            logStream.close();
        }
        openLogFile();
    }



    void writeLog(const string& level, const string& message) {
        lock_guard<mutex> guard(log_mutex); // guard 會在生命週期結束時被銷毀，並自動釋放鎖
        auto now = chrono::system_clock::now();
        auto timeNow = chrono::system_clock::to_time_t(now);

        tm tm{};
        localtime_r(&timeNow, &tm);

        logStream << "[" << level << "]\t"
                   << put_time(&tm, "%Y/%m/%d %H:%M:%S") << " "
                   << message << endl;
    }

    void compressOrDeleteOldLogs() {
        auto now = chrono::system_clock::now();
        auto compressThreshold = now - chrono::days(1);
        auto deleteThreshold = now - chrono::days(7);

        for (const auto& entry : fs::recursive_directory_iterator("logs")) {
            if (entry.is_regular_file()) {
                if (entry.path().extension() == ".gz") {
                    fs::file_time_type lastWriteTime = fs::last_write_time(entry);
                    auto lastWriteTimePoint = chrono::time_point_cast<chrono::system_clock::duration>(
                        lastWriteTime - fs::file_time_type::clock::now() + now
                    );

                    // 檢查是否超過設定天數
                    if (lastWriteTimePoint < deleteThreshold) {
                        fs::remove(entry);
                        cout << "Deleted: " << entry.path().string() << endl;
                    }
                } else {
                     fs::file_time_type lastWriteTime = fs::last_write_time(entry);
                    auto lastWriteTimePoint = chrono::time_point_cast<chrono::system_clock::duration>(
                        lastWriteTime - fs::file_time_type::clock::now() + now
                    ); // 將 duration（先計算 fs::file_time_type::clock 的差異，再加到 now 上） 轉換成 time_point

                    // 印出最後修改時間
                    //    time_t cftime =  chrono::system_clock::to_time_t(lastWriteTimePoint);
                    //     cout << ctime(&cftime);

                    // 檢查是否超過設定天數
                    if (lastWriteTimePoint < compressThreshold) {
                        string filePath = entry.path().string();
                        string command = "gzip " + filePath;

                        // 執行壓縮命令
                        system(command.c_str());
                        cout << "Compressed: " << filePath << endl;
                    }
                }
            }
        }
    }

    void checkAndRotateLogBySize() {
        if (logFilePath.empty() || !fs::exists(logFilePath)) return;
        if (fs::file_size(logFilePath) <= maxSize) return;

        // 取時間戳
        auto now = chrono::system_clock::now();
        auto timeNow = chrono::system_clock::to_time_t(now);

        // 重新命名檔案
        string baseName = logFilePath.substr(0, logFilePath.find_last_of('.'));
        string extension = logFilePath.substr(logFilePath.find_last_of('.'));
        string newFileName = baseName + "_" + to_string(timeNow) + extension;
        fs::rename(logFilePath, newFileName);
        logStream.close();
        openLogFile();
    }
};

void setLastWriteTime() {
    fs::path goalFilePath{"logs/2024/02/2024-02-29_1709192808.log"};
    auto now = chrono::system_clock::now();
    auto days = chrono::hours(24 * 2); // 計算70天的時長
    auto newTime = chrono::file_clock::now() + 
                (now - days - chrono::system_clock::now()); // 計算70天前的時間

    try {
        fs::last_write_time(goalFilePath, newTime); // 更新文件的最後寫入時間
        cout << "Successfully set the last write time of " << goalFilePath << " to 2 days ago.\n";
    } catch (const fs::filesystem_error& e) {
        cerr << "Error: " << e.what() << '\n';
    }
}

int main() {
    Logger logger;
    logger.Info("***************************************************");
    logger.Info("[初始化] Initial start");
    logger.Info("***************************************************");
    logger.Info("This is an info message.");
    logger.Debug("This is a debug message.");
    logger.Warning("This is a warning message.");
    logger.Error("This is an error message.");
    // setLastWriteTime();
    return 0;
}