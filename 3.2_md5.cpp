#include <openssl/md5.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>

using namespace std;

string md5FromString(const string &input) {
    unsigned char digest[MD5_DIGEST_LENGTH]; // 存計算出的 MD5 雜湊值 (16 bytes)
    MD5((unsigned char*)input.c_str(), input.length(), (unsigned char*)&digest);

    stringstream ss;
    for(int i = 0; i < MD5_DIGEST_LENGTH; i++)
        ss << hex << setw(2) << setfill('0') << (int)digest[i]; // 每個 byte（16 個） 轉成 16 進位並補 0

    return ss.str();
}

string md5FromFile(const string &filename) {
    ifstream file(filename, ifstream::binary);
    if (!file) {
        cerr << "Cannot open file: " << filename << endl;
        return "";
    }

    MD5_CTX md5Context;
    MD5_Init(&md5Context); // 初始化 MD5 上下文用於計算 MD5 雜湊值

    char buffer[1024];

    // 逐段讀取檔案內容並更新 MD5 雜湊值
    while (file.read(buffer, sizeof(buffer))) {
        MD5_Update(&md5Context, buffer, file.gcount()); // file.gcount() 返回實際讀取的字節數
    }
    MD5_Update(&md5Context, buffer, file.gcount()); // 最後一次讀取的部分

    unsigned char digest[MD5_DIGEST_LENGTH];
    MD5_Final(digest, &md5Context); // 完成 MD5 計算並將最終的散列值存儲在 digest 數組中，並清除 MD5 上下文

    stringstream ss;
    for(int i = 0; i < MD5_DIGEST_LENGTH; i++)
        ss << hex << setw(2) << setfill('0') << (int)digest[i]; // 每個 byte（16 個） 轉成 16 進位並補 0

    return ss.str();
}

int main() {
    string filePath = "./md5_sample.txt";
    string text = "微程式具有多年資通訊專案開發經驗，三大業務包括電子支付端末設備、共享智慧單車服務、半導體感控監測（智慧製造）。以電子支付領域來看，提供全台包含公車、捷運、公共自行車、停車場、計程車隊等數十萬個端末電子支付設備，打造國內數位化支付應用的基礎，也在TPASS行政院通勤月票計畫中扮演關鍵角色。";

    cout << "MD5 of file: " << md5FromFile(filePath) << endl;
    cout << "MD5 of string: " << md5FromString(text) << endl;

    return 0;
}
