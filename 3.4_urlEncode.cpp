#include <iostream>
#include <string>
#include <iomanip>

using namespace std;

string urlEncode(const string &value) {
    ostringstream escaped;
    escaped.fill('0');
    escaped << hex;

    // 遍歷輸入字串每個字元
    for (string::const_iterator i = value.begin(), n = value.end(); i != n; ++i) {
        string::value_type c = (*i);

        // 將字母、數字、-、_、.、~ 直接加到 escaped 流中
        if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            escaped << c;
            continue;
        }

        // 其他字元轉換成 %XX 格式
        escaped << uppercase; // 確保編碼後的十六進制數字使用大寫字母
        escaped << '%' << setw(2) << int((unsigned char) c); // ASCII 碼轉換成十六進制數字
        escaped << nouppercase; // 重置流的狀態
    }

    return escaped.str();
}

int main() {
    string data = "{\"device\":1,\"deviceNo\":\"9596719\",\"eventCode\":\"0A030B00\",\"eventMsg\":\"交易自檢電量(84%,80%)\",\"msgSn\":1,\"unixTime\":1707969481,\"version\":1,\"sign\":\"9e3bcaab50acfd4c61b7572a622a83a1b8118825b7c9a573858b5885f5e7846a\"}";
    string encodedData = urlEncode(data);
    cout << "Encoded Data: " << encodedData << endl;

    return 0;
}
