#include <iostream>
#include <openssl/sha.h>
#include <sstream>
#include <iomanip>

using namespace std;

string sha256(const string str) {
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256_CTX sha256;
    SHA256_Init(&sha256);
    SHA256_Update(&sha256, str.c_str(), str.size());
    SHA256_Final(hash, &sha256);
    stringstream ss;
    for(int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
        ss << hex << setw(2) << setfill('0') << (int)hash[i];
    }
    return ss.str();
}

int main() {
    string text = "微程式具有多年資通訊專案開發經驗，三大業務包括電子支付端末設備、共享智慧單車服務、半導體感控監測（智慧製造）。以電子支付領域來看，提供全台包含公車、捷運、公共自行車、停車場、計程車隊等數十萬個端末電子支付設備，打造國內數位化支付應用的基礎，也在TPASS行政院通勤月票計畫中扮演關鍵角色。";
    string output = sha256(text);
    cout << "SHA-256: " << output << endl;
    return 0;
}
