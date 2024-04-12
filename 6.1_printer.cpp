#include <fcntl.h> // 提供打開串行端口的功能
#include <unistd.h> // 提供close()函数
#include <termios.h> // 提供POSIX終端控制定義
#include <iostream>
#include <cstring> 
#include <vector>
#include <iomanip>
#include <sstream>
#include <bitset>
#include <iconv.h>
#include <chrono>
#include <fstream>
#include <cstdint>

using namespace std;


// ===================
// BMP檔案標頭結構
// ===================
#pragma pack(push, 1) // 禁用字節對齊，確保結構體的成員間不會有填充字節（緊密排列以符合BMP文件格式）
struct BMPHeader { // 檔案標頭
    uint16_t file_type{0x4D42}; // BMP檔案標記
    uint32_t file_size{0};      // 檔案大小
    uint16_t reserved1{0};      // 保留
    uint16_t reserved2{0};      // 保留
    uint32_t offset_data{0};    // 從檔案開頭到位圖數據的偏移
};

struct BMPInfoHeader { // 位圖資訊標頭
    uint32_t size{0};          // 本結構的大小
    int32_t width{0};          // 圖像寬度
    int32_t height{0};         // 圖像高度
    uint16_t planes{1};        // 色彩平面數，必須為1
    uint16_t bit_count{0};     // 每個像素位數
    uint32_t compression{0};   // 壓縮類型
    uint32_t size_image{0};    // 圖像大小，以字節為單位
    int32_t x_pixels_per_meter{0}; // 水平解析度
    int32_t y_pixels_per_meter{0}; // 垂直解析度
    uint32_t colors_used{0};       // 位圖實際使用的色彩數
    uint32_t colors_important{0};  // 重要色彩數
};
#pragma pack(pop) // 恢復字節對齊


// ===================
// 打開並配置串行端口
// ===================
int openAndConfigureSerialPort(const char* portName, int baudRate) {
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
    cfsetospeed(&tty, baudRate);
    cfsetispeed(&tty, baudRate);

    // 設置數據位為8
    tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS8;
    // CSIZE 定義為 0000060（八進制）-> 00110000（二進制），所以 ~CSIZE = 11001111（二進制），用作掩碼將 c_cflag 中的數據位設置清零
    // CS8 定義為 0000060（八進制）-> 00110000（二進制），將 c_cflag 中的數據位設置為8

    // 設置無奇偶校驗（取反關閉）
    tty.c_cflag &= ~PARENB;

    // 設置使用1個停止位（原始為2個）
    tty.c_cflag &= ~CSTOPB;

    // 設置RTS/CTS（硬件）流控制，Request To Send / Clear To Send，準備好發送 / 接收時會設置 RTS / CTS 信號成有效狀態
    tty.c_cflag |= CRTSCTS;

    // 設置為原始模式，輸入不經過處理
    tty.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG); // 禁用後，輸入不經過緩衝處理 | 不回顯輸入字符 | 不回顯擦除字符（backspace） | 不回顯控制字符（Ctrl+C）
    tty.c_iflag &= ~(IXON | IXOFF | IXANY); // 關閉軟件流控制（XON 『Ctrl+Q』 / XOFF 『Ctrl+S』），都被禁用時，發送和接收資料不會因為軟體訊號而暫停或恢復，資料流是連續的
    tty.c_oflag &= ~OPOST; // 原始輸出，例如換行符不會被轉換成回車換行符，禁用了輸出數據的後處理

    // 應用配置
    if (tcsetattr(serialFd, TCSANOW, &tty) != 0) { // 第二個參數 when 設置為 TCSANOW，立即更改串行端口的屬性
        cerr << "應用串行端口配置出錯" << endl;
        return -1;
    }

    return serialFd;
}


// ===================
// 查詢印表機狀態
// ===================
int queryStatus(int serialFd) {
    int error = 0;
    for (int i = 1; i <= 4; i++) {
        vector<unsigned char> command = {0x10, 0x04, static_cast<unsigned char>(i)};
        write(serialFd, command.data(), command.size());
        unsigned char response[8];
        read(serialFd, response, 8);
        cout << "狀態 " << i << ": " << bitset<8>(response[0]) << endl;
        for (int j = 0; j < 8; ++j) {
            switch (i) {
                case 1:
                    if (j == 7) {
                        cout << ((response[0] & (1 << j)) ? "紙未撕走" : "紙已撕走") << endl;
                    }
                    break;
                case 2:
                    if (j == 2) {
                        error += response[0] & (1 << j);
                        cout << ((response[0] & (1 << j)) ? "上蓋開" : "上蓋關") << endl;
                    } else if (j == 5) {
                        error += response[0] & (1 << j);
                        cout << ((response[0] & (1 << j)) ? "印表機缺紙" : "印表機不缺紙") << endl;
                    } else if (j == 6) {
                        error += response[0] & (1 << j);
                        cout << ((response[0] & (1 << j)) ? "有錯誤情況" : "沒有出錯情況") << endl;
                    }
                    break;
                case 3:
                    if (j == 3) {
                        error += response[0] & (1 << j);
                        cout << ((response[0] & (1 << j)) ? "切刀有錯誤" : "切刀無錯誤") << endl;
                    } else if (j == 5) {
                        error += response[0] & (1 << j);
                        cout << ((response[0] & (1 << j)) ? "有不可恢復錯誤" : "無不可恢復錯誤") << endl;
                    } else if (j == 6) {
                        error += response[0] & (1 << j);
                        cout << ((response[0] & (1 << j)) ? "列印頭溫度或電壓超出範圍" : "列印頭溫度和電壓正常") << endl;
                    }
                    break;
                case 4:
                    if (j == 2) {
                        cout << ((response[0] & (1 << j)) ? "紙將近" : "有紙") << endl;
                    } else if (j == 5) {
                        error += response[0] & (1 << j);
                        cout << ((response[0] & (1 << j)) ? "紙盡" : "有紙") << endl;
                    }
                    break;
                default:
                    break;
            }
        }
    }
    return error;
}


// ===================
// utf8 轉 big5
// ===================
vector<unsigned char> utf8ToBig5(const string& utf8str) {
    iconv_t cd = iconv_open("BIG5", "UTF-8");
    if (cd == (iconv_t)-1) {
        return {};
    }

    vector<unsigned char> big5str(utf8str.length() * 2);
    char* inbuf = const_cast<char*>(utf8str.data());
    char* outbuf = reinterpret_cast<char*>(big5str.data());
    size_t inbytesleft = utf8str.length();
    size_t outbytesleft = big5str.size();

    while (0 < inbytesleft) {
        size_t result = iconv(cd, &inbuf, &inbytesleft, &outbuf, &outbytesleft);
        if (result == (size_t)-1) {
            iconv_close(cd);
            return {};
        }
    }

    big5str.resize(big5str.size() - outbytesleft);
    iconv_close(cd);
    return big5str;
}


// ===================
// 列印文字
// ===================
void printText(int serialFd, const string &text, int align = 0x00, int size = 0x00, int weight = 0x00, int spacing = 0x21, bool print = true, int location = 0) {
    vector<unsigned char> big5str = utf8ToBig5(text);
    // for (auto &c : big5str) {
    //     cout << hex << (int)c << " ";
    // }
    vector<unsigned char> command = {
        0x1B, 0x33, static_cast<unsigned char>(spacing), // 行間距
        0x1B, 0x21, static_cast<unsigned char>(weight),
        0x1B, 0x61, static_cast<unsigned char>(align), // 0: 靠左對齊, 1: 居中對齊, 2: 靠右對齊
        0x1D, 0x21, static_cast<unsigned char>(size), // 字體寬高
    };
    if (location > 0) {
        command.insert(command.end(), {0x1B, 0x24});
        command.push_back(location & 0xFF);
        command.push_back((location >> 8) & 0xFF);
    }
    command.insert(command.end(), big5str.begin(), big5str.end());
    if (print) {
        command.insert(command.end(), {0x0D, 0x0A});
    }
    write(serialFd, command.data(), command.size());
}


// ===================
// 列印一維條碼
// ===================
void printBarcode(int serialFd, const string &text, int type, int width = 0x02, int height = 0x40, int hri = 0x00) {
    vector<unsigned char> command = {
        0x1B, 0x61, 0x01,
        0x1D, 0x48, static_cast<unsigned char>(hri), // 可讀字元 HRI 列印位置
        0x1D, 0x77, static_cast<unsigned char>(width), // 寬度
        0x1D, 0x68, static_cast<unsigned char>(height), // 高度
        0x1D, 0x6B, static_cast<unsigned char>(type) // 編碼方式
    };
    command.insert(command.end(), text.begin(), text.end());
    command.push_back(0x00);
    write(serialFd, command.data(), command.size());

}


// ===================
// 列印雙 QRCode
// ===================
void printDoubleQRCode(int serialFd, const string &text1, const string &text2) {
    vector<unsigned char> command = {
        0x1F, 0x51, 0x02, 0x04,
        0x00, 0x05, // QR1 位置
    };
    command.push_back((text1.size() >> 8) & 0xFF); // QR1 數據長度
    command.push_back(text1.size() & 0xFF);
    command.insert(command.end(), {
        0x00, 0x06 // 錯誤校正水準、QR1 大小
    });
    command.insert(command.end(), text1.begin(), text1.end()); // QR1 數據

    command.insert(command.end(), {
        0x00, 0xD0, // QR2 位置
    });
    command.push_back((text2.size() >> 8) & 0xFF); // QR2 數據長度
    command.push_back(text2.size() & 0xFF);
    command.insert(command.end(), {
        0x00, 0x06, // 錯誤校正水準、QR2 大小
    });
    command.insert(command.end(), text2.begin(), text2.end());
    write(serialFd, command.data(), command.size());
}        


// ===================
// 進紙
// ===================
void feedPaper(int serialFd, int points, int lines) {
    if (points > 0) {
        vector<unsigned char> command = {0x1B, 0x4A, static_cast<unsigned char>(points)};
        write(serialFd, command.data(), command.size());
    } else if (lines > 0) {
        vector<unsigned char> command = {0x1B, 0x64, static_cast<unsigned char>(lines)};
        write(serialFd, command.data(), command.size());
    }
}


// ===================
// 取得當前時間
// ===================
string getCurrentTimeAsString() {
    auto now = chrono::system_clock::now();
    auto in_time_t = chrono::system_clock::to_time_t(now);

    tm bt{};
    localtime_r(&in_time_t, &bt);

    stringstream ss;
    ss << put_time(&bt, "%Y-%m-%d %H:%M:%S");
    return ss.str();
}


// ===================
// 顏色反轉
// ===================
void invertColors(uint8_t* buffer, size_t size) {
    for (size_t i = 0; i < size; i++) {
        buffer[i] = 255 - buffer[i];
    }
}


// ===================
// 垂直翻轉圖片
// ===================
void flipVertical(uint8_t* buffer, int width, int height) {
    vector<uint8_t> temp(width); // 暫存一行像素
    for (int i = 0; i < height / 2; i++) { // 上半部和下半部像素進行交換
        memcpy(temp.data(), buffer + i * width, width);
        memcpy(buffer + i * width, buffer + (height - i - 1) * width, width); // 倒數第i行存進第i行
        memcpy(buffer + (height - i - 1) * width, temp.data(), width);
    }
}


// ===================
// 列印圖片
// ===================
void printImage(int serialFd, const string &filename) {
    ifstream bmpFile(filename, ios::binary);
    BMPHeader header;
    BMPInfoHeader infoHeader;

    if (!bmpFile) {
        throw runtime_error("無法開啟檔案");
    }
    bmpFile.read((char*)&header, sizeof(header));
    bmpFile.read((char*)&infoHeader, sizeof(infoHeader));

    // 確認是否為BMP檔案
    if (header.file_type != 0x4D42) {
        throw runtime_error("非有效的BMP檔案");
    }
    // cout << "檔案大小: " << header.file_size << endl;
    // cout << "圖像大小: " << infoHeader.size_image << endl;
    // cout << "圖像寬度: " << infoHeader.width << endl;
    // cout << "圖像高度: " << infoHeader.height << endl;
    // cout << "數據偏移: " << header.offset_data << endl;

    vector<uint8_t> buffer(infoHeader.size_image);
    bmpFile.seekg(header.offset_data, ios::beg);
    bmpFile.read(reinterpret_cast<char*>(buffer.data()), infoHeader.size_image); // reinterpret_cast 類型轉換，不檢查安全性; static_cast 類型轉換，檢查安全性
    bmpFile.close();

    // 顏色反轉
    invertColors(buffer.data(), buffer.size());
    // 垂直翻轉
    flipVertical(buffer.data(), infoHeader.width / 8, infoHeader.height);

    vector<unsigned char> command = {
        0x1D, 0x76, 0x30, 0x00, 0x2C, 0x00, 0x40, 0x00
    };    
    command.insert(command.end(), buffer.begin(), buffer.end());
    command.insert(command.end(), {0x0D, 0x0A});
    write(serialFd, command.data(), command.size());
}


// ===================
// 發送列印命令
// ===================
void sendCommand(int serialFd) {
    printImage(serialFd, "img/invoice_logo.bmp");
    printText(serialFd, "電子發票證明聯\n113年03-04月\nYP-10001173", 0x01, 0x11, 0x38);
    string currentTime = getCurrentTimeAsString();
    printText(serialFd, currentTime);
    printText(serialFd, "隨機碼:9480", 0x00, 0x00, 0x00, 0x21, false);
    printText(serialFd, "總計:100", 0x00, 0x00, 0x00, 0x21, true, 275);
    printText(serialFd, "賣方:89798198");
    printBarcode(serialFd, "11303YP100011739480", 0x04, 0x01, 0x32);
    printDoubleQRCode(serialFd, "YP100011731130301948000000000000000010000000089798198qWg/UN5+uVECCpw3zFQ7hw==:**********:1:1:1:停車費:1:1:", "**");
    printText(serialFd, "卡號:1512536303（悠遊卡）");
    printText(serialFd, "退貨憑電子發票證明聯正本辦理");
    printText(serialFd, "-------------------------------");
    feedPaper(serialFd, 0x00, 0x02);

    vector<unsigned char> command = {
        // 0x1B, 0x69, // 全切
        // 0x1B, 0x6D, // 半切
        0x1D, 0x56, 0x01
    };    
    write(serialFd, command.data(), command.size());

    printText(serialFd, "交易明細", 0x01, 0x11, 0x03);
    printText(serialFd, "場站:三代6F114測試站");
    printText(serialFd, "聯絡電話:12345678");
    printText(serialFd, "停車費:$100 TX");
    printText(serialFd, "銷售額:$100");
    printText(serialFd, "總計金額:$100 TX");
    printText(serialFd, "端末設備編號:000BABE79AF0");
    printText(serialFd, "卡號:1512536303（悠遊卡）");
    printText(serialFd, "外觀卡號:0");
    printText(serialFd, "車牌號碼:AAA1111");
    printText(serialFd, "進場時間:" + getCurrentTimeAsString());
    printText(serialFd, "出場時間:" + getCurrentTimeAsString());
    printText(serialFd, "扣款前:$9156  自動加值:0");
    printText(serialFd, "扣款:$100  扣款後:$9056");

    feedPaper(serialFd, 0xA8, 0x00);
    command = {
        // 0x1B, 0x69, // 全切
        // 0x1B, 0x6D, // 半切
        0x1D, 0x56, 0x00
    };    
    write(serialFd, command.data(), command.size());
}
// void sendCommand(int serialFd) {
//     vector<unsigned char> command = {
//         // 0x12, 0x54 // selftest
//         0x1B, 0x40,
//         0x1B, 0x33, 0x40, // 設置行間距 0 ~ 255 點
//     };
//     write(serialFd, command.data(), command.size());
//     printText(serialFd, "Selftest", 0x01, 0x11); // 置中，2倍寬高
//     printText(serialFd, "System KP-247", 0x00, 0x00); // 居左，標準大小
//     setLineSpacing(serialFd, 0x21); // 設置行間距為默認值（33點）
//     printText(serialFd, "Version:V1.15.00 20180904ip", 0x00, 0x00);
//     setLineSpacing(serialFd, 0x40);
//     printText(serialFd, "[Barcode Samples]", 0x00, 0x00);
//     setLineSpacing(serialFd, 0x21);
//     printText(serialFd, "CODE 39:", 0x00, 0x00);
//     feedPaper(serialFd, 0x20, 0x00);
//     // 打印一維條碼
//     command = {
//         0x1D, 0x48, 0x02, // 設置條碼可讀字元在條碼下方
//         0x1D, 0x68, 0x50, // 設置條碼高度
//         0x1D, 0x77, 0x02, // 設置條碼寬度
//         0x1D, 0x6B, 0x04, '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 0x00, // 打印條碼
//     };
//     write(serialFd, command.data(), command.size());
//     setLineSpacing(serialFd, 0x20);
//     printText(serialFd, "QR CODE:", 0x00, 0x00);
//     feedPaper(serialFd, 0x20, 0x00);
//     // 打印雙 QR 碼
//     command = {
//         0x1F, 0x51, 0x02, 0x03,
//         0x00, 0x20, // QR1 位置
//         0x00, 0x0A, // QR1 數據長度
//         0x01, 0x06, // 錯誤校正水準、QR1 大小
//         0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, // QR1 數據
//         0x00, 0xC0, // QR2 位置
//         0x00, 0x0A, // QR2 數據長度
//         0x02, 0x06, // 錯誤校正水準、QR2 大小
//         0x39, 0x38, 0x37, 0x36, 0x35, 0x34, 0x33, 0x32, 0x31, 0x30, // QR2 數據
//     };
//     write(serialFd, command.data(), command.size());
//     feedPaper(serialFd, 0x20, 0x00);
//     printText(serialFd, "[ASCII Samples]", 0x00, 0x00);
//     // ASCII
//     for (int i = 32; i <= 127; ++i) {
//         write(serialFd, &i, 1);
//     }
//     write(serialFd, "\x0A", 1);
//     feedPaper(serialFd, 0x20, 0x00);

//     // 打印圖片
//     command = {
//         0x1D, 0x76, 0x30, 0x00, 0x30, 0x00, 0x10, 0x00, // 圖片水平取模列印
//         0xFF, 0xFF, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00,
//         0xFF, 0xFF, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00,
//         0xFF, 0xFF, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00,
//         0xFF, 0xFF, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00,
//         0xFF, 0xFF, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00,
//         0xFF, 0xFF, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00,
//         0xFF, 0xFF, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00,
//         0xFF, 0xFF, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00,
//         0xFF, 0xFF, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00,
//         0xFF, 0xFF, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00,
//         0xFF, 0xFF, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00,
//         0xFF, 0xFF, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00,
//         0xFF, 0xFF, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00,
//         0xFF, 0xFF, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00,
//         0xFF, 0xFF, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00,
//         0xFF, 0xFF, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00,

//         0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0xFF, 0xFF,
//         0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0xFF, 0xFF,
//         0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0xFF, 0xFF,
//         0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0xFF, 0xFF,
//         0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0xFF, 0xFF,
//         0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0xFF, 0xFF,
//         0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0xFF, 0xFF,
//         0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0xFF, 0xFF,
//         0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0xFF, 0xFF,
//         0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0xFF, 0xFF,
//         0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0xFF, 0xFF,
//         0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0xFF, 0xFF,
//         0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0xFF, 0xFF,
//         0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0xFF, 0xFF,
//         0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0xFF, 0xFF,
//         0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0xFF, 0xFF,

//         0x0A
//     };
//     write(serialFd, command.data(), command.size());
//     printText(serialFd, "SelfTest Finished", 0x01, 0x00);
//     feedPaper(serialFd, 0xF4, 0x00);

//     command = {
//         0x1B, 0x69, // 全切
//         // 0x1B, 0x6D, // 半切
//     };    
//     write(serialFd, command.data(), command.size());
// }

int main() {
    const char* portName = "/dev/ttyUSB0";
    int baudRate = B115200;
    int serialFd = openAndConfigureSerialPort(portName, baudRate);
    if (serialFd < 0) {
        return 1;
    }
    
    if (!queryStatus(serialFd)) {
        cout << endl << "打印機狀態正常，發送數據" << endl;
        sendCommand(serialFd);
    } else {
        cout << "打印機狀態異常" << endl;
    }
    close(serialFd);
    return 0;
}
