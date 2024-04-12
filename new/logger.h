#ifndef LOGGER_H
#define LOGGER_H

#include <iostream>
#include <fstream>
#include <string>
#include <mutex>
#include <thread>
#include <chrono>
#include <filesystem>
#include <iomanip>
#include <ctime>
#include <system_error>
#include <condition_variable>



// ===================
// Logger 類別
// ===================
class Logger {
public:
    Logger(const std::string& fileName);

    ~Logger();

    void info(const std::string& message);
    
    void debug(const std::string& message);

    void warning(const std::string& message);

    void error(const std::string& message);

private:
    std::ofstream logFile;
    std::mutex logMutex;
    std::thread compressThread;
    bool stopCompress;
    size_t maxFileSize;


    // 寫入 log
    void writeLog(const std::string& level, const std::string& message);

    // 壓縮 log 檔
    void compressLog();

};


#endif // LOGGER_H