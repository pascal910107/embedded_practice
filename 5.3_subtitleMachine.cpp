#include <iostream> // 包含標準輸入輸出流對象的頭文件
#include <sys/socket.h> // 包含socket函數和結構的頭文件
#include <arpa/inet.h> // 包含inet函數的頭文件
#include <unistd.h> // 提供通用的文件、目錄、程序及進程操作的函數
#include <string.h> // 包含字符串處理函數的頭文件
#include <iconv.h>
#include <vector>
#include <errno.h>
#include <iomanip>
#include <algorithm>
#include <sstream>

using namespace std;

#define CRC(crc, byte) (((crc) >> 8) ^ table[((crc) ^ (byte)) & 0xFF])

static const uint16_t table[256] = {
    0x0000, 0xC0C1, 0xC181, 0x0140, 0xC301, 0x03C0, 0x0280, 0xC241,
    0xC601, 0x06C0, 0x0780, 0xC741, 0x0500, 0xC5C1, 0xC481, 0x0440,
    0xCC01, 0x0CC0, 0x0D80, 0xCD41, 0x0F00, 0xCFC1, 0xCE81, 0x0E40,
    0x0A00, 0xCAC1, 0xCB81, 0x0B40, 0xC901, 0x09C0, 0x0880, 0xC841,
    0xD801, 0x18C0, 0x1980, 0xD941, 0x1B00, 0xDBC1, 0xDA81, 0x1A40,
    0x1E00, 0xDEC1, 0xDF81, 0x1F40, 0xDD01, 0x1DC0, 0x1C80, 0xDC41,
    0x1400, 0xD4C1, 0xD581, 0x1540, 0xD701, 0x17C0, 0x1680, 0xD641,
    0xD201, 0x12C0, 0x1380, 0xD341, 0x1100, 0xD1C1, 0xD081, 0x1040,
    0xF001, 0x30C0, 0x3180, 0xF141, 0x3300, 0xF3C1, 0xF281, 0x3240,
    0x3600, 0xF6C1, 0xF781, 0x3740, 0xF501, 0x35C0, 0x3480, 0xF441,
    0x3C00, 0xFCC1, 0xFD81, 0x3D40, 0xFF01, 0x3FC0, 0x3E80, 0xFE41,
    0xFA01, 0x3AC0, 0x3B80, 0xFB41, 0x3900, 0xF9C1, 0xF881, 0x3840,
    0x2800, 0xE8C1, 0xE981, 0x2940, 0xEB01, 0x2BC0, 0x2A80, 0xEA41,
    0xEE01, 0x2EC0, 0x2F80, 0xEF41, 0x2D00, 0xEDC1, 0xEC81, 0x2C40,
    0xE401, 0x24C0, 0x2580, 0xE541, 0x2700, 0xE7C1, 0xE681, 0x2640,
    0x2200, 0xE2C1, 0xE381, 0x2340, 0xE101, 0x21C0, 0x2080, 0xE041,
    0xA001, 0x60C0, 0x6180, 0xA141, 0x6300, 0xA3C1, 0xA281, 0x6240,
    0x6600, 0xA6C1, 0xA781, 0x6740, 0xA501, 0x65C0, 0x6480, 0xA441,
    0x6C00, 0xACC1, 0xAD81, 0x6D40, 0xAF01, 0x6FC0, 0x6E80, 0xAE41,
    0xAA01, 0x6AC0, 0x6B80, 0xAB41, 0x6900, 0xA9C1, 0xA881, 0x6840,
    0x7800, 0xB8C1, 0xB981, 0x7940, 0xBB01, 0x7BC0, 0x7A80, 0xBA41,
    0xBE01, 0x7EC0, 0x7F80, 0xBF41, 0x7D00, 0xBDC1, 0xBC81, 0x7C40,
    0xB401, 0x74C0, 0x7580, 0xB541, 0x7700, 0xB7C1, 0xB681, 0x7640,
    0x7200, 0xB2C1, 0xB381, 0x7340, 0xB101, 0x71C0, 0x7080, 0xB041,
    0x5000, 0x90C1, 0x9181, 0x5140, 0x9301, 0x53C0, 0x5280, 0x9241,
    0x9601, 0x56C0, 0x5780, 0x9741, 0x5500, 0x95C1, 0x9481, 0x5440,
    0x9C01, 0x5CC0, 0x5D80, 0x9D41, 0x5F00, 0x9FC1, 0x9E81, 0x5E40,
    0x5A00, 0x9AC1, 0x9B81, 0x5B40, 0x9901, 0x59C0, 0x5880, 0x9841,
    0x8801, 0x48C0, 0x4980, 0x8941, 0x4B00, 0x8BC1, 0x8A81, 0x4A40,
    0x4E00, 0x8EC1, 0x8F81, 0x4F40, 0x8D01, 0x4DC0, 0x4C80, 0x8C41,
    0x4400, 0x84C1, 0x8581, 0x4540, 0x8701, 0x47C0, 0x4680, 0x8641,
    0x8201, 0x42C0, 0x4380, 0x8341, 0x4100, 0x81C1, 0x8081, 0x4040,
};


// ===================
// CRC計算
// ===================
unsigned short calcCRC(const uint8_t *data, uint32_t size) {
    unsigned short crc = 0; // 初始化CRC
    for (uint32_t i = 0; i < size; ++i) {
        crc = CRC(crc, data[i]); // 對每個字節應用CRC宏計算新的CRC值
    }
    return crc;
}


// ===================
// UTF-8轉GBK
// ===================
vector<unsigned char> utf8ToGbkBytes(const string& utf8_str) {
    size_t inLen = utf8_str.size();
    size_t outLen = inLen * 2 + 1; // GBK編碼長度最大為UTF-8的2倍
    char* outBuf = new char[outLen];
    fill(outBuf, outBuf + outLen, 0);

    char* in = (char*)utf8_str.c_str();
    char* out = outBuf;
    iconv_t cd = iconv_open("GBK", "UTF-8");
    vector<unsigned char> gbkBytes;
    if (cd == (iconv_t)-1) {
        delete[] outBuf;
        return gbkBytes; // 返回空的vector
    }
    if (iconv(cd, &in, &inLen, &out, &outLen) == (size_t)-1) {
        iconv_close(cd);
        delete[] outBuf;
        return gbkBytes; // 返回空的vector
    }

    // 將轉換後的字節填充到vector中
    for (size_t i = 0; i < out - outBuf; ++i) {
        gbkBytes.push_back(static_cast<unsigned char>(outBuf[i]));
    }
    
    iconv_close(cd);
    delete[] outBuf;
    return gbkBytes;
}


// ===================
// 解析 Response
// ===================
int parseResponse(const vector<uint8_t>& data) {
    cout << "Receive" << endl;
    for (auto byte : data) {
        cout << hex << setw(2) << setfill('0') << (int)byte << " ";
    }
    cout << endl;

    auto it = data.begin();
    // 跳過幀頭的A5
    while (it != data.end() && *it == 0xA5) ++it;
    
    //跳過包頭數據的14字節（前進14個uint8_t元素）
    advance(it, 14);
    if (it >= data.end()) {
        cerr << "Invalid response: Package header or data field is incomplete." << endl;
        return -1;
    }

    // 找幀尾0x5A位置
    auto frameEndIt = find(it, data.end(), 0x5A);
    if (frameEndIt == data.end()) {
        cerr << "Invalid response: Frame tail (0x5A) not found." << endl;
        return -1;
    }

    // 數據域結束於幀尾前2字節的包校驗
    auto dataFieldEnd = frameEndIt - 2;
    vector<uint8_t> dataField(it, dataFieldEnd); // vector 範圍是左閉右開的，所以不包含 dataFieldEnd

    cout << "Cmd Error = " << setw(2) << setfill('0') << hex << (int)dataField[2] << ", ";
    switch (dataField[2]) {
        case 0x00:
            cout << "No Error" << endl;
            break;
        case 0x01:
            cout << "Command Group Error" << endl;
            break;
        case 0x02:
            cout << "Command Not Found" << endl;
            break;
        case 0x03:
            cout << "The Controller is busy now" << endl;
            break;
        case 0x04:
            cout << "Out of the Memory Volume" << endl;
            break;
        case 0x05:
            cout << "CRC16 Checksum Error" << endl;
            break;
        case 0x06:
            cout << "File Not Exist" << endl;
            break;
        case 0x07:
            cout << "Flash Access Error" << endl;
            break;
        case 0x08:
            cout << "File Download Error" << endl;
            break;
        case 0x09:
            cout << "Filename Error" << endl;
            break;
        case 0x0A:
            cout << "File type Error" << endl;
            break;
        case 0x0B:
            cout << "File CRC16 Error" << endl;
            break;
        case 0x0C:
            cout << "FontLibrary Not Exist" << endl;
            break;
        case 0x0D:
            cout << "Firmware Type Error(Check the controller type)" << endl;
            break;
        case 0x0E:
            cout << "DateTime format Error" << endl;
            break;
        case 0x0F:
            cout << "File Exist for File overwrite" << endl;
            break;
        case 0x10:
            cout << "File block number error" << endl;
            break;
        default:
            break;
    }

    // 判斷 ACK 或 NACK
    if (dataField.size() == 5 && dataField[0] == 0xA0) {
        cout << (dataField[1] == 0x00 ? "ACK" : "NACK") << endl;
        return (dataField[1] == 0x00 ? 0 : 1);
    } else if (dataField[2] != 0x00) {
        return -1;
    }
    return 0;
}

// ===================
// 字符轉義
// ===================
vector<unsigned char> escapeBytes(const vector<unsigned char>& input) {
    vector<unsigned char> output;
    for (auto byte : input) {
        switch (byte) {
            case 0xA5:
                // 0xA5 -> 0xA6, 0x02
                output.push_back(0xA6);
                output.push_back(0x02);
                break;
            case 0xA6:
                // 0xA6 -> 0xA6, 0x01
                output.push_back(0xA6);
                output.push_back(0x01);
                break;
            case 0x5A:
                // 0x5A -> 0x5B, 0x02
                output.push_back(0x5B);
                output.push_back(0x02);
                break;
            case 0x5B:
                // 0x5B -> 0x5B, 0x01
                output.push_back(0x5B);
                output.push_back(0x01);
                break;
            default:
                output.push_back(byte);
                break;
        }
    }
    return output;
}


// ===================
// 建立並連接Socket
// ===================
bool createAndConnectSocket(int& sock, const char* server_ip, int port) {
    // 創建socket文件描述符
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        cerr << "Socket creation error" << endl;
        return false;
    }

    struct sockaddr_in serv_addr; // 宣告服務器地址結構
    serv_addr.sin_family = AF_INET; // 指定地址：IPv4
    serv_addr.sin_port = htons(port); // 指定端口
    // 將IP地址轉換為網絡地址（文本轉二進制）
    if (inet_pton(AF_INET, server_ip, &serv_addr.sin_addr) <= 0) {
        cerr << "Invalid address/ Address not supported" << endl;
        return false;
    }
    // 連接服務器
    if (connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        cerr << "Connection Failed" << endl;
        return false;
    }
    return true;
}


// ===================
// 組合 PHY1（包頭數據 + 數據域 + 包校驗）
// ===================
vector<unsigned char> combinePHY1(vector<unsigned char> dataField) {
    vector<unsigned char> phy1;

    // 包頭數據
    vector<unsigned char> packageHeader = {
        0x01, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFE, 0x02 // 包頭數據後兩位為數據域長度
    };
    packageHeader.push_back(dataField.size() & 0xFF);
    packageHeader.push_back((dataField.size() >> 8) & 0xFF);

    // 組合 包頭數據 和 數據域
    phy1.insert(phy1.end(), packageHeader.begin(), packageHeader.end());
    phy1.insert(phy1.end(), dataField.begin(), dataField.end());

    // 計算 CRC-16 包校驗
    uint16_t ph1CRC = calcCRC(phy1.data(), phy1.size());
    phy1.push_back(ph1CRC & 0xFF);
    phy1.push_back((ph1CRC >> 8) & 0xFF);

    return phy1;
}


// ===================
// 組合 PHY0（幀頭 + PHY1 層數據 + 幀尾）
// ===================
vector<unsigned char> combinePHY0(vector<unsigned char> phy1) {
    vector<unsigned char> phy0;
    
    // 幀頭
    vector<unsigned char> frameHeader = {
        0xA5, 0xA5, 0xA5, 0xA5, 0xA5, 0xA5, 0xA5, 0xA5
    };

    // ph1 轉義
    vector<unsigned char> escapedPHY1 = escapeBytes(phy1);

    
    phy0.insert(phy0.end(), frameHeader.begin(), frameHeader.end());
    phy0.insert(phy0.end(), escapedPHY1.begin(), escapedPHY1.end());
    phy0.push_back(0x5A); // 幀尾

    return phy0;
}


// ===================
// 發送開始寫文件命令
// ===================
void startWriteFile(int sock, vector<unsigned char> fileData) {
    // 數據域
    vector<unsigned char> dataField = {
        0xA1, // 命令分組編號
        0x05, // 命令編號
        0x01, // 是否要求 Response
        0x00, 0x00, // 保留
        0x01, // 文件覆蓋方式，0x00：不覆蓋，0x01：覆蓋
        'L', 'O', 'G', 'O', // 文件名
        // 文件長度（四位）
    };

    for (int i = 0; i < 4; ++i) {
        dataField.push_back((fileData.size() >> (i * 8)) & 0xFF);
    }

    vector<unsigned char> phy1 = combinePHY1(dataField);
    vector<unsigned char> phy0 = combinePHY0(phy1);

    send(sock, phy0.data(), phy0.size(), 0);
    cout << "Message sent" << endl;
    for (auto byte : phy0) {
        cout << hex << setw(2) << setfill('0') << (int)byte << " ";
    }
    cout << endl;
}


// ===================
// 發送寫文件命令
// ===================
void writeFile(int sock, vector<unsigned char> fileData) {
    // 數據域
    vector<unsigned char> dataField = {
        0xA1, // 命令分組編號
        0x06, // 命令編號
        0x01, // 是否要求 Response
        0x00, 0x00, // 保留
        'L', 'O', 'G', 'O', // 文件名
        0x01, // 標志是否為最後一包數據，0x00：不是，0x01：是
        0x00, 0x00, // 包號
        0x39, 0x00, // 包長（單包發送即文件長度）
        0x00, 0x00, 0x00, 0x00, // 本包數據在文件中的起始位置
        // 文件數據
    };

    // 更新文件數據長度
    int offsetForFileDataLen = 12; // 相對於 dataField 開頭的偏移量
    dataField[offsetForFileDataLen] = static_cast<unsigned char>(fileData.size() & 0xFF);
    dataField[offsetForFileDataLen + 1] = static_cast<unsigned char>((fileData.size() >> 8) & 0xFF);

    // 將 fileData 加到 dataField 的末尾
    dataField.insert(dataField.end(), fileData.begin(), fileData.end());

    vector<unsigned char> phy1 = combinePHY1(dataField);
    vector<unsigned char> phy0 = combinePHY0(phy1);

    send(sock, phy0.data(), phy0.size(), 0);
    cout << "Message sent" << endl;
    for (auto byte : phy0) {
        cout << hex << setw(2) << setfill('0') << (int)byte << " ";
    }
    cout << endl;
}



// ===================
// 發送實時顯示信息命令
// ===================
void realTimeDisplay(int sock, const string& message, unsigned char displayMode) {
    // 數據域
    vector<unsigned char> dataField = {
        0xA3, 0x06, 0x01, 0x2D, 0x00,
        0x00, // 要刪除的區域個數
        0x01, // 更新的區域個數
        // 區域數據長度（兩位）
    };

    // 區域數據格式
    vector<unsigned char> areaData = {
        0x00,
        0x00, 0x00, // X座標
        0x00, 0x00, // Y座標
        0x18, 0x00, // 寬度
        0x20, 0x00, // 高度
        0x00,
        0x00, // 行間距
        0x00, // 動態區運行模式
        0x02, 0x00, // 動態區數據超時時間
        0x00, // 語音
        0x00, // 拓展位個數
        0x00, // 字體對齊
        0x02, // 多行顯示
        0x02, // 自動換行
        displayMode, // 顯示方式
        0x00, // 退出方式
        0x04, // 顯示速度
        0x00, // 顯示延時
        // 數據長度（四位）
        // 數據
    };

    vector<unsigned char> gbkBytes = utf8ToGbkBytes(message);
    unsigned int dataLength = gbkBytes.size();

    // areaData 更新數據長度（小端格式）
    for (int i = 0; i < 4; ++i) {
        areaData.push_back((dataLength >> (i * 8)) & 0xFF);
    }

    // dataField 更新區域數據長度
    unsigned int areaDataLength = areaData.size() + dataLength;
    for (int i = 0; i < 2; ++i) {
        dataField.push_back((areaDataLength >> (i * 8)) & 0xFF);
    }

    // areaData 更新數據
    areaData.insert(areaData.end(), gbkBytes.begin(), gbkBytes.end());

    // dataField 更新區域數據
    dataField.insert(dataField.end(), areaData.begin(), areaData.end());

    vector<unsigned char> phy1 = combinePHY1(dataField);
    vector<unsigned char> phy0 = combinePHY0(phy1);

    send(sock, phy0.data(), phy0.size(), 0);
    cout << "Message sent" << endl;
    for (auto byte : phy0) {
        cout << hex << setw(2) << setfill('0') << (int)byte << " ";
    }
    cout << endl;
}


// ===================
// 接收 Response
// ===================
int receiveResponse(int sock) {
    fd_set readfds;
    struct timeval tv;
    int retval, valread;
    char buffer[1024];
    vector<uint8_t> recvData;

    // 設定超時時間
    tv.tv_sec = 3;  // 等待3秒
    tv.tv_usec = 0;

    // 清除fd集合
    FD_ZERO(&readfds);
    // 將sock加入fd集合
    FD_SET(sock, &readfds);

    cout << "Waiting for response..." << endl;

    do {
        retval = select(sock + 1, &readfds, NULL, NULL, &tv);

        if (retval == -1) {
            cerr << "select() error: " << strerror(errno) << endl;
            close(sock);
            return -1;
        } else if (retval) {
            // 資料可讀
            valread = read(sock, buffer, sizeof(buffer));
            if (valread > 0) {
                recvData.insert(recvData.end(), buffer, buffer + valread); // 將接收到的資料追加到vector中
                // for (int i = 0; i < valread; ++i) {
                //     cout << hex << (buffer[i] & 0xFF) << " ";
                // }
            } else if (valread == 0) {
                // 沒有更多的資料，退出迴圈
                break;
            } else {
                // read錯誤
                cerr << "read() error" << endl;
                close(sock);
                return -1;
            }
        } else {
            // 超時，沒有資料可讀
            cout << "Timeout, no more data." << endl;
            break;
        }
    } while (true);

    int result = parseResponse(recvData);
    return result;
}


// ===================
// 組合文件數據
// ===================
vector<unsigned char> combineFileData(const string& message, unsigned char displayMode) {
    // 節目文件數據格式
    vector<unsigned char> fileData = {
        0x00, // 文件類型
        'L', 'O', 'G', 'O', // 文件名
        0x00, 0x00, 0x00, 0x00, // 文件長度（四位）
        0x00, // 節目播放優先級
        0x00, 0x00, // 節目播放方式
        0x01, // 節目重複播放次數
        0xFF, 0xFF, 0x01, 0x01, 0xFF, 0xFF, 0x01, 0x01,  // 節目生命週期，0xFFFF：永久有效
        0x11, // 節目的星期屬性
        0x00, // 定時節目位
        0x00, // 節目的播放時段組數
        0x01, // 區域個數
        // 區域數據長度（四位）
        // 區域數據
    };

    // 區域數據格式
    vector<unsigned char> areaData = {
        0x00,
        0x00, 0x00, // X座標
        0x00, 0x00, // Y座標
        0x18, 0x00, // 寬度
        0x20, 0x00, // 高度
        0x00,
        0x00, // 行間距
        0x00, // 動態區運行模式
        0x02, 0x00, // 動態區數據超時時間
        0x00, // 語音
        0x00, // 拓展位個數
        0x00, // 字體對齊
        0x02, // 多行顯示
        0x02, // 自動換行
        displayMode, // 顯示方式
        0x00, // 退出方式
        0x04, // 顯示速度
        0x00, // 顯示延時
        // 數據長度（四位）
        // 數據
    };

    vector<unsigned char> gbkBytes = utf8ToGbkBytes(message);
    unsigned int dataLength = gbkBytes.size();

    // areaData 更新數據長度（小端格式）
    for (int i = 0; i < 4; ++i) {
        areaData.push_back((dataLength >> (i * 8)) & 0xFF);
    }

    // fileData 更新區域數據長度
    unsigned int areaDataLength = areaData.size() + dataLength;
    for (int i = 0; i < 4; ++i) {
        fileData.push_back((areaDataLength >> (i * 8)) & 0xFF);
    }

    // areaData 更新數據
    areaData.insert(areaData.end(), gbkBytes.begin(), gbkBytes.end());

    // fileData 更新區域數據
    fileData.insert(fileData.end(), areaData.begin(), areaData.end());

    // 更新文件數據長度
    unsigned int fileDataLength = fileData.size() + 2; // 加上 file crc16 的2個字節
    int offsetForFileDataLen = 5;
    for (int i = 0; i < 4; ++i) {
        fileData[offsetForFileDataLen + i] = static_cast<unsigned char>((fileDataLength >> (i * 8)) & 0xFF);
    }

    // 計算 file CRC-16
    unsigned short fileDataCRC = calcCRC(fileData.data(), fileData.size());
    fileData.push_back(fileDataCRC & 0xFF);
    fileData.push_back((fileDataCRC >> 8) & 0xFF);

    return fileData;
}


// ===================
// 初始化字幕機文件
// ===================
void initSubtitleMachineFile(int sock, const string& message, unsigned char displayMode) {
    vector<unsigned char> fileData = combineFileData(message, displayMode);

    // 開始寫文件
    startWriteFile(sock, fileData);
    int result = receiveResponse(sock);

    if (result == -1) {
        return;
    } else if (result == 1) {
        cout << "文件系統剩餘容量不足" << endl;
        return;
    }
    cout << endl << endl;

    // 寫文件
    writeFile(sock, fileData);
}



int main() {
    const char* server_ip = "172.16.65.115";
    int port = 5005;
    string input = "asdABCZ為程式資訊"; // UTF-8
    unsigned char displayMode = 0x03; // 0x01 靜止顯示 0x02 快速打出 0x03 向左移動


    int sock = 0; // socket文件描述符
    char buffer[10240] = {0}; // 用於接收server的消息
    
    if (!createAndConnectSocket(sock, server_ip, port)) {
        return -1;
    }

    // initSubtitleMachineFile(sock, input, displayMode);
    realTimeDisplay(sock, input, displayMode);

    receiveResponse(sock);
    close(sock);
    
    return 0;
}