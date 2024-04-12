#include <fcntl.h> // 提供打開串行端口的功能
#include <unistd.h> // 提供close()函数
#include <termios.h> // 提供POSIX終端控制定義
#include <iostream>
#include <cstring> 
#include <vector>
#include <iomanip>
#include <sstream>
#include <bitset>
#include <sys/select.h>
#include <chrono>
#include <sys/time.h> // for struct timeval

using namespace std;


// ===================
// 打開並配置串行端口
// ===================
int openAndConfigureSerialPort(const char* portName) {
    int serialFd = open(portName, O_RDWR | O_NOCTTY | O_SYNC | O_NONBLOCK); // 讀寫模式 | 不將此設備作為控制終端 | 同步操作(寫入到串行端口) | 非阻塞模式
    if (serialFd < 0) {
        cerr << "無法打開串行端口" << endl;
        return -1;
    }
    struct termios tty; // termios 結構體用於控制和配置串行端口（tty 設備）
    memset(&tty, 0, sizeof tty);
    if (tcgetattr(serialFd, &tty) != 0) { //獲取當前串行端口配置
        cerr << "獲取串行端口配置時出錯" << endl;
        return -1;
    }

    //設置波特率 baud rate
    cfsetospeed(&tty, B57600);
    cfsetispeed(&tty, B57600);

    // 設置數據位為8
    tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS8;     // 8-bit chars
    // 無校驗位，一個停止位
    tty.c_iflag &= ~IGNBRK;         // disable break processing
    tty.c_lflag = 0;                // 不啟用信號字符，不允許字符和文件格式轉換
    tty.c_oflag = 0;                // 不需要任何輸出處理
    tty.c_cc[VMIN]  = 0;            // 讀取不阻塞
    tty.c_cc[VTIME] = 5;            // 0.5秒讀取超時

    tty.c_iflag &= ~(IXON | IXOFF | IXANY); // 關閉軟件流控制
    tty.c_cflag |= (CLOCAL | CREAD); // 忽略調製解調器控制線，啟用接收器
    tty.c_cflag &= ~(PARENB | PARODD);      // 關閉奇偶校驗
    tty.c_cflag |= 0; // 禁用了所有輸出處理選項
    tty.c_cflag &= ~CSTOPB; // 1個停止位
    tty.c_cflag &= ~CRTSCTS; // 關閉硬件流控制

    if (tcsetattr(serialFd, TCSANOW, &tty) != 0) {
        cerr << "應用串行端口配置出錯" << endl;
        return -1;
    }

    return serialFd;
}


// ===================
// 取得當前時間（16進制 ASCII）
// ===================
string getCurrentTimeAsHexString() {
    auto now = chrono::system_clock::now();
    auto now_c = chrono::system_clock::to_time_t(now);

    stringstream ss;
    ss << put_time(localtime(&now_c), "%Y%m%d%H%M%S");

    string timeStr = ss.str();
    string hexStr;
    for (char c : timeStr) {
        int temp = c - '0'; 
        stringstream hexstream;
        hexstream << hex << temp;
        hexStr += hexstream.str();
    }

    return hexStr;
}


// ===================
// 取得當前時間（UnixDateTime）
// ===================
string getUnixDateTimeLSB() {
    auto now = chrono::system_clock::now();
    auto duration = now.time_since_epoch();
    auto seconds = chrono::duration_cast<chrono::seconds>(duration).count();

    string lsbStr;
    for (int i = 0; i < 4; ++i) {
        char byte = (seconds >> (i * 8)) & 0xFF;
        lsbStr += byte;
    }

    return lsbStr;
}


// ===================
// 計算 LRC
// ===================
unsigned char calculateLRC(vector<unsigned char> &command) {
    unsigned char lrc = 0;
    for (auto byte : command) {
        lrc ^= byte;
    }
    return lrc;
}


// ===================
// 組合要發送的命令
// ===================
vector<unsigned char> combineCommand(int serialFd, int cla, int ins, int p1, int p2, int lc, vector<unsigned char> data, int le) {

    int infoLength = 6 + data.size();
    vector<unsigned char> command = {
        0x00, 0x00,
        static_cast<unsigned char>(infoLength),
        static_cast<unsigned char>(cla),
        static_cast<unsigned char>(ins),
        static_cast<unsigned char>(p1),
        static_cast<unsigned char>(p2),
        static_cast<unsigned char>(lc)
    };
    command.insert(command.end(), data.begin(), data.end());
    command.push_back(static_cast<unsigned char>(le));

    unsigned char lrc = calculateLRC(command);
    command.push_back(lrc);

    return command;
}


// ===================
// 發送命令
// ===================
void sendCommand(int serialFd, string commandType,  string currentTime, string unixDateTimeLSB, int lcdControl = 0, int txnAmount = 0) {
    vector<unsigned char> command = {};
    if (commandType == "PPR_Reset_Offline") {
        // 組合 Information -> Body -> Data
        vector<unsigned char> data = {
            0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x31, // TM 店號
            0x30, 0x31, // TM 機號
        };
        data.insert(data.end(), currentTime.begin(), currentTime.end()); // TM 交易日期時間
        data.insert(data.end(), {
            0x30, 0x30, 0x30, 0x30, 0x30, 0x31, // TM 交易序號
            0x30, 0x30, 0x30, 0x31, // TM 收銀員代號
        });
        data.insert(data.end(), unixDateTimeLSB.begin(), unixDateTimeLSB.end()); // 交易日期時間（UnixDateTime）
        data.insert(data.end(), {
            0x00, // 舊場站代碼
            0x01, 0x00, // 新場站代碼
            0x00, // 服務業者代碼
            0x00, 0x00, 0x00, // 新服務業者代碼
            0x73, //小額設定旗標
            0xB8, 0x0B, // 小額消費日限額額度
            0xE8, 0x03, // 小額消費次限額次數
            0x11, // SAM 卡位置控制旗標
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 保留
        });
        command = combineCommand(serialFd, 0x80, 0x01, 0x00, 0x01, 0x40, data, 0xFA);
    } else if (commandType == "PPR_EDCARead") {
        vector<unsigned char> data = {
            static_cast<unsigned char>(lcdControl), // 控制交易完成後之 LCD 顯示，0x00:【票卡餘額 XXXX】，0x01:【(請勿移動票卡)】
            0x30, 0x30, 0x30, 0x30, 0x30, 0x31, // TM 交易序號
        };
        data.insert(data.end(), unixDateTimeLSB.begin(), unixDateTimeLSB.end()); // 交易日期時間（UnixDateTime）
        data.insert(data.end(), {
            0x00, 0x00, 0x00, 0x00, 0x00 // 保留
        });
        command = combineCommand(serialFd, 0x80, 0x01, 0x01, 0x00, 0x10, data, 0xA0);
    } else if (commandType == "PPR_EDCADeduct") {
         vector<unsigned char> data = {
            0x01, // 扣款
            0x00,
            0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x31, // TM 店號
            0x30, 0x31, // TM 機號
        };
        data.insert(data.end(), currentTime.begin(), currentTime.end()); // TM 交易日期時間
        data.insert(data.end(), {
            0x30, 0x30, 0x30, 0x30, 0x30, 0x31, // TM 交易序號
            0x30, 0x30, 0x30, 0x31, // TM 收銀員代號
        });
        data.insert(data.end(), unixDateTimeLSB.begin(), unixDateTimeLSB.end()); // 交易日期時間（UnixDateTime）
        data.push_back(txnAmount & 0xFF); // 交易金額
        data.push_back((txnAmount >> 8) & 0xFF);
        data.push_back((txnAmount >> 16) & 0xFF);
        data.insert(data.end(), {
            0x00, // 是否進行自動加值
            0x20, // 交易方式，0x20:【小額扣款】
            0x06, // 本運具之轉乘群組代碼
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 保留
            static_cast<unsigned char>(lcdControl) // 控制交易完成後之 LCD 顯示，0x00:【交易完成 請取卡】，0x01:【(請勿移動票卡)】
        });
        command = combineCommand(serialFd, 0x80, 0x01, 0x02, 0x00, 0x40, data, 0x7A);
    }

    cout << endl << endl << "發送命令 " << commandType << ": " << endl;
    for (int i = 0; i < command.size(); i++) {
        cout << hex << setw(2) << setfill('0') << (int)command[i] << " ";
    }
    cout << endl << endl;
    write(serialFd, command.data(), command.size());
}


// ===================
// 印出對齊的字段
// ===================
void printAligned(const string& fieldName, vector<unsigned char>& dataBuffer, int fieldLength) {
    cout << left << setw(48) << setfill(' ') << fieldName;
    for (int i = 0; i < fieldLength; i++) {
        cout << right << hex << setw(2) << setfill('0') << (int)dataBuffer[2 + i] << " ";
    }
    cout << endl;
    dataBuffer.erase(dataBuffer.begin(), dataBuffer.begin() + fieldLength);
}


// ===================
// 處理 Response
// ===================
void processResponse(vector<unsigned char> dataBuffer, string commandType) {
    if (commandType == "PPR_Reset_Offline") {
        printAligned("[Information Section Length]:", dataBuffer, 1);
        printAligned("[Spec. Version Num]:", dataBuffer, 1);
        printAligned("[Reader ID]:", dataBuffer, 4);
        printAligned("[Reader FW Version]:", dataBuffer, 6);
        printAligned("[SAM ID]:", dataBuffer, 8);
        printAligned("[SAM SN]:", dataBuffer, 4);
        printAligned("[SAM CRN]:", dataBuffer, 8);
        printAligned("[Device ID]:", dataBuffer, 4);
        printAligned("[SAM Key Version]:", dataBuffer, 1);
        printAligned("[S-TAC]:", dataBuffer, 8);
        printAligned("[SAM Version Num]:", dataBuffer, 1);
        printAligned("[SID]:", dataBuffer, 8);
        printAligned("[SAM Usage Control]:", dataBuffer, 3);
        printAligned("[SAM Admin KVN]:", dataBuffer, 1);
        printAligned("[SAM Issuer KVN]:", dataBuffer, 1);
        printAligned("[Auth Credit Limit]:", dataBuffer, 3);
        printAligned("[Single Credit TXN AMT Limit]:", dataBuffer, 3);
        printAligned("[Auth Credit Balance]:", dataBuffer, 3);
        printAligned("[Auth Credit Cumulative]:", dataBuffer, 3);
        printAligned("[Auth Cancel Credit Cumulative]:", dataBuffer, 3);
        printAligned("[New Device ID]:", dataBuffer, 6);
        printAligned("[Tag List Table]:", dataBuffer, 40);
        printAligned("[SAM Issuer Specific Data]:", dataBuffer, 32);
        printAligned("[STC]:", dataBuffer, 4);
        printAligned("[RSAM]:", dataBuffer, 8);
        printAligned("[RHOST]:", dataBuffer, 8);
        printAligned("[SATOKEN]:", dataBuffer, 16);
        printAligned("[CPD Read Flag]:", dataBuffer, 1);
        printAligned("[One Day Quota Flag For Micro Payment]:", dataBuffer, 1);
        printAligned("[One Day Quota For Micro Payment]:", dataBuffer, 2);
        printAligned("[Once Quota For Micro Payment]:", dataBuffer, 2);
        printAligned("[Check Debit Value]:", dataBuffer, 2);
        printAligned("[Add Quota Flag]:", dataBuffer, 1);
        printAligned("[Add Quota]:", dataBuffer, 3);
        printAligned("[The Remainder of Add Quota]:", dataBuffer, 3);
        printAligned("[Cancel Credit Quota]:", dataBuffer, 3);
        printAligned("[deMAC Parameter]:", dataBuffer, 8);
        printAligned("[Last TXN Date Time]:", dataBuffer, 4);
        printAligned("[Prev New Device ID]:", dataBuffer, 6);
        printAligned("[Prev STC]:", dataBuffer, 4);
        printAligned("[Prev TXN Date Time]:", dataBuffer, 4);
        printAligned("[Prev Credit Balance Change Flag]:", dataBuffer, 1);
        printAligned("[Prev Confirm Code]:", dataBuffer, 2);
        printAligned("[Prev CACrypto]:", dataBuffer, 16);
    } else if (commandType == "PPR_EDCARead") {
        printAligned("[Information Section Length]:", dataBuffer, 1);
        printAligned("[Purse Version Number]:", dataBuffer, 1);
        printAligned("[Purse Usage Control]:", dataBuffer, 1);
        printAligned("[Single Auto-Load Transaction Card Amount]:", dataBuffer, 3);
        printAligned("[PID(Purse ID)]:", dataBuffer, 8);
        printAligned("[Sub Area Code]:", dataBuffer, 2);
        printAligned("[Purse Exp. Date]:", dataBuffer, 4);
        printAligned("[Purse Balance Before TXN]:", dataBuffer, 3);
        printAligned("[TXN SN. Before TXN]:", dataBuffer, 3);
        printAligned("[Card Type]:", dataBuffer, 1);
        printAligned("[Personal Profile]:", dataBuffer, 1);
        printAligned("[Profile Exp. Date]:", dataBuffer, 4);
        printAligned("[Area Code]:", dataBuffer, 1);
        printAligned("[Card Physical ID]:", dataBuffer, 7);
        printAligned("[Card Physical ID Length]:", dataBuffer, 1);
        printAligned("[Device ID]:", dataBuffer, 4);
        printAligned("[New Device ID]:", dataBuffer, 6);
        printAligned("[Service Provider ID]:", dataBuffer, 1);
        printAligned("[New Service Provider ID]:", dataBuffer, 3);
        printAligned("[Location ID]:", dataBuffer, 1);
        printAligned("[New Location ID]:", dataBuffer, 2);
        printAligned("[Issuer Code]:", dataBuffer, 1);
        printAligned("[Bank Code]:", dataBuffer, 1);
        printAligned("[Loyalty Counter]:", dataBuffer, 2);
        printAligned("[Last Credit TXN Log]:", dataBuffer, 33);
        printAligned("[Autoload Counter]:", dataBuffer, 1);
        printAligned("[Autoload Date]:", dataBuffer, 2);
        printAligned("[Card One Day Quota]:", dataBuffer, 3);
        printAligned("[Card One Day Quota Date]:", dataBuffer, 2);
        printAligned("[TSQN of URT]:", dataBuffer, 3);
        printAligned("[TXN Date Time of URT]:", dataBuffer, 4);
        printAligned("[TXN Type of URT]:", dataBuffer, 1);
        printAligned("[TXN AMT of URT]:", dataBuffer, 3);
        printAligned("[EV of URT]:", dataBuffer, 3);
        printAligned("[Transfer Group Code]:", dataBuffer, 2);
        printAligned("[Location ID of URT]:", dataBuffer, 2);
        printAligned("[Device ID of URT]:", dataBuffer, 6);
        printAligned("[Identification Num]:", dataBuffer, 16);
        printAligned("[The Remainder of Add Quota]:", dataBuffer, 3);
        printAligned("[RFU]:", dataBuffer, 15);
    } else if (commandType == "PPR_EDCADeduct") {
        printAligned("[Information Section Length]:", dataBuffer, 1);
        printAligned("[Signature key KVN]:", dataBuffer, 1);
        printAligned("[SID]:", dataBuffer, 8);
        printAligned("[Hash Type]:", dataBuffer, 1);
        printAligned("[Host Admin Key KVN]:", dataBuffer, 1);
        printAligned("[Msg Type]:", dataBuffer, 1);
        printAligned("[Sub Type]:", dataBuffer, 1);
        printAligned("[CTC]:", dataBuffer, 3);
        printAligned("[TM(TXN Mode)]:", dataBuffer, 1);
        printAligned("[TQ(TXN Qualifier)]:", dataBuffer, 1);
        printAligned("[SIGN]:", dataBuffer, 16);
        printAligned("[TSQN]:", dataBuffer, 3);
        printAligned("[TXN AMT]:", dataBuffer, 3);
        printAligned("[Purse Balance]:", dataBuffer, 3);
        printAligned("[MAC/HCrypto]:", dataBuffer, 16);
        printAligned("[TXN Date Time]:", dataBuffer, 4);
        printAligned("[Confirm Code]:", dataBuffer, 2);
        printAligned("[Msg Type]:", dataBuffer, 1);
        printAligned("[Sub Type]:", dataBuffer, 1);
        printAligned("[Transfer Group Code]:", dataBuffer, 1);
        printAligned("[New Transfer Group Code]:", dataBuffer, 2);
        printAligned("[CTC]:", dataBuffer, 3);
        printAligned("[TM(TXN Mode)]:", dataBuffer, 1);
        printAligned("[TQ(TXN Qualifier)]:", dataBuffer, 1);
        printAligned("[SIGN]:", dataBuffer, 16);
        printAligned("[TSQN]:", dataBuffer, 3);
        printAligned("[TXN AMT]:", dataBuffer, 3);
        printAligned("[Purse Balance]:", dataBuffer, 3);
        printAligned("[MAC/HCrypto]:", dataBuffer, 16);
        printAligned("[TXN Date Time]:", dataBuffer, 4);
        printAligned("[Confirm Code]:", dataBuffer, 2);
    }
}




// ===================
// 接收 Response
// ===================
int recieveResponse(int serialFd, string commandType) {
    struct timeval timeout;
    fd_set readfds;
    vector<unsigned char> dataBuffer; // 用於存儲接收到的數據
    bool isStartOfMessage = true; // 是否是消息的開始

    while (true) {
        FD_ZERO(&readfds); // 清空文件描述符集合
        FD_SET(serialFd, &readfds); // 將串行端口文件描述符加入文件描述符集合

        timeout.tv_sec = 3; // 超時為3秒
        timeout.tv_usec = 0; // 0微秒

        int ret = select(serialFd + 1, &readfds, NULL, NULL, &timeout); // 監視串行端口是否有數據到來（等待文件描述符集合中的文件描述符變為可讀、可寫或有錯誤，直到超時）
        if (ret < 0) {
            cout << "select() 出錯" << endl;
            return -1;
        }  else if (ret == 0) {
            // cout << "3秒內無讀取到數據" << endl << endl;
            break;
        } else if (FD_ISSET(serialFd, &readfds)) { // 串行端口的文件描述符可讀（有數據到來）
            unsigned char buf[256] = {0};
            int n = read(serialFd, buf, sizeof(buf));
            if (n > 0) {
                if (isStartOfMessage && buf[0] == 0x00 && buf[1] == 0x00 && buf[2] > 0) { // 處理 Prolog
                    isStartOfMessage = false;
                    dataBuffer.insert(dataBuffer.end(), buf, buf + n);
                } else if (!isStartOfMessage) { // 繼續接收信息
                    dataBuffer.insert(dataBuffer.end(), buf, buf + n);
                } else { // 錯誤 Response
                    cout << "錯誤 Response 數據" << endl;
                    return -1;
                }
            }
        }
    }
    if (dataBuffer.size() > 0) {
        if (dataBuffer[dataBuffer.size() - 3] == 0x90 && dataBuffer[dataBuffer.size() - 2] == 0x00) {
            cout << commandType << " 成功" << endl;
            processResponse(dataBuffer, commandType);
        } else {
            cout << commandType << " 失敗" << endl;
            cout << hex << setfill('0') << setw(2) << (int)dataBuffer[dataBuffer.size() - 3] << hex << setfill('0') << setw(2) << (int)dataBuffer[dataBuffer.size() - 2] << endl;
            if (dataBuffer[dataBuffer.size() - 3] == 0x62 && dataBuffer[dataBuffer.size() - 2] == 0x02) {
                cout << "讀卡失敗" << endl;
            } else if (dataBuffer[dataBuffer.size() - 3] == 0x61 && dataBuffer[dataBuffer.size() - 2] == 0x11) {
                cout << "未先正確執行 CPU 卡上一步驟或CPU 卡驗證失敗" << endl;
            }
            return -2;
        }
    }
    return 0;
}




int main() {
    const char* portName = "/dev/ttyUSB0";
    int serialFd = openAndConfigureSerialPort(portName);
    if (serialFd < 0) {
        return 1;
    }

    int lcdControl = 0x00;
    int txnAmount = 99;

    string currentTime = getCurrentTimeAsHexString();
    string unixDateTimeLSB = getUnixDateTimeLSB();
    
    // sendCommand(serialFd, "PPR_Reset_Offline", currentTime, unixDateTimeLSB);
    // if (recieveResponse(serialFd, "PPR_Reset_Offline") < 0) {
    //     close(serialFd);
    //     return 1;
    // }

    sendCommand(serialFd, "PPR_EDCARead", currentTime, unixDateTimeLSB, lcdControl);
    if (recieveResponse(serialFd, "PPR_EDCARead") < 0) {
        close(serialFd);
        return 1;
    }

    sendCommand(serialFd, "PPR_EDCADeduct", currentTime, unixDateTimeLSB, lcdControl, txnAmount);
    if (recieveResponse(serialFd, "PPR_EDCADeduct") < 0) {
        close(serialFd);
        return 1;
    }

    

    close(serialFd);
    return 0;
}
