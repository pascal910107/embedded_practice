#include <iostream>
#include <string>
#include  <fstream>
#include  <sstream>
#include <nlohmann/json.hpp>
#include <bitset>
#include <chrono>
#include <iomanip>

using namespace std;

void stringCombine() {
    int unixTime = 1707969413;
    string deviceNo = "9596719";
    string ecc_ip = "172.16.11.20";
    int ecc_port = 8902;

    string unixTimeString = to_string(unixTime);
    string eccPortString = to_string(ecc_port);

    string combined = unixTimeString + " " + deviceNo + " " + ecc_ip + " " + eccPortString;
    cout << combined << endl;
}

string extractContent(const string& xml, const string& tagName) {
    string openTag = "<" + tagName + ">";
    string closeTag = "</" + tagName + ">";
    size_t startPos = xml.find(openTag) + openTag.length();
    size_t endPos = xml.find(closeTag);
    if (startPos != string::npos && endPos != string::npos) {
        return xml.substr(startPos, endPos - startPos);
    }
    return "";
}

void stringDismantle() {
    // string xml = "<TransXML><TRANS>"
    //                 "<T3901>0</T3901>"
    //                 "<T0100>0100</T0100>"
    //                 "<T0300>296060</T0300>"
    //                 "<T1100>115703</T1100>"
    //                 "<T1200>115703</T1200>"
    //                 "<T1300>20240215</T1300>"
    //                 "<T4109>222143782</T4109>"
    //                 "<T4110>0876000001404287</T4110>"
    //                 "<T1101>000342</T1101>"
    //                 "<T4213>08760136-00000-0008760136</T4213>"
    //                 "</TRANS></TransXML>";
    // string t4109Content = extractContent(xml, "T4109");
    // string t4110Content = extractContent(xml, "T4110");

    // cout << "T4109: " << t4109Content << endl;
    // cout << "T4110: " << t4110Content << endl;
    ifstream file("xml.txt");
    stringstream buffer;
    buffer << file.rdbuf();
    string xml = buffer.str();
    string t4109Content = extractContent(xml, "T4109");
    string t4110Content = extractContent(xml, "T4110");

    cout << "T4109: " << t4109Content << endl;
    cout << "T4110: " << t4110Content << endl;
}

void stringCompare() {
    string string1 = "Apple";
    string string2 = "Apple";
    string string3 = "apple";

    cout << "String1 and String2 are equal: " << boolalpha << (string1 == string2) << endl;
    cout << "String1 and String3 are equal: " << boolalpha << (string1 == string3) << endl;
}

void stringFuzzyCompare() {
    string A = "MP-1512R_Parking_V4_v1_00_r2";
    string B = "Parking_V4_v1_00_r2";
    bool result = (A.find(B) != string::npos);
    cout << "B exists in A: " << boolalpha << result << endl;
}

void stringSearch() {
    string jsonData = "{\"retCode\":\"0\",\"retMsg\":\"成功\",\"unixTime\":1707969419,\"version\":1,\"sign\":\"975403374f8e3f9a\"}";
    string searchString = "\"retCode\":\"0\"";
    bool found = (jsonData.find(searchString) != string::npos);
    cout << "Search string found: " << boolalpha << found << endl;
}

void stringProcess() {
    // 字串組合
    stringCombine();
    // 字串拆解
    stringDismantle();
    // 字串比對
    stringCompare();
    // 字串模糊比對
    stringFuzzyCompare();
    // 字串搜尋
    stringSearch();
}

void jsonProcess() {
    string filename = "config.txt";
    nlohmann::json object;
    {
        ifstream file(filename);
        if (!file.is_open()) {
            cout << "cannot open file for reading." << endl;
            return;
        }
        file >> object;
        file.close();
    }
    object["factoryPasswd"] = "89798198";
    {
        ofstream file(filename);
        if (!file.is_open()) {
            cout << "cannot open file for writing." << endl;
            return;
        }
        file << object.dump(4);
        file.close();
    }
    string searchString = "RS232MP1512";
    if (object.contains("Rs232MP1512")) {
        cout << "Found 'Rs232MP1512'." << endl;
    } else {
        cout << "'Rs232MP1512' not found." << endl;
    }
}

// 整數轉字串
string intToString(int num) {
    return to_string(num);
}

// 字串轉整數
int stringToInt(const string& str) {
    int num;
    istringstream(str) >> num;
    return num;
}

// 十六進制轉十六進制字串
string hexToHexString(int num) {
    stringstream ss;
    ss << hex << num;
    return ss.str();
}

// 十六進制字串轉十六進制
void hexStringToHex(const string& hexStr) {
    int num;
    istringstream(hexStr) >> hex >> num;
    cout << " 0x" << setfill('0') << setw(2) << uppercase << hex << num << endl;

    // unsigned int num;
    // stringstream ss;
    // ss << hex << hexStr; // 將流的格式設置為十六進制模式，代表接下來向流中寫入的整數數據將被視為十六進制數字，或從流中讀取的字串將被解析為十六進制數字
    // ss >> num;
    // cout << hex << num << endl;
}

// 整數轉十六進制
string intToHex(int num) {
    stringstream ss;
    if (num <= 0xFF) {
        ss << "0x" << setfill('0') << setw(2) << hex << num;
    } else {
        int high = (num >> 8) & 0xFF; // num右移8位，取低8位，舉例 num=9999 (1001 1000 0111 1111)，右移8位後為 0000 0000 1001 1000
        int low = num & 0xFF;
        ss << "0x" << setfill('0') << setw(2) << uppercase << hex << high << " 0x" << setfill('0') << setw(2) << uppercase << hex << low;
    }
    return ss.str();
}

// 十六進制轉整數
int hexToInt(int num) {
    stringstream ss;
    ss << hex << num;
    ss >> num;
    return num;
}

// 整數轉為二進制字符串
string intToBinary(int num) {
    return bitset<8>(num).to_string();
}

// 格式轉換
void typeConvert() {
    cout << "int to string: " << intToString(123) << endl;
    cout << "string to int: " << stringToInt("456") << endl;

    cout << "hexstring to hex: ";
    hexStringToHex("0D");
    cout << "hex to hexstring: " << hexToHexString(0xC5) << endl;

    cout << "int to hex: " << intToHex(10) << endl;
    cout << "int to hex: " << intToHex(9999) << endl;
    // cout << "hex to int: " << hexToInt(0x64) << endl;
    cout << "hex to int: " << dec << 0x64 << endl;

    cout << "int to binary: " << intToBinary(18) << endl;
}

// 時間處理
void timeProcess() {
    // 1. 獲取當前Unix時間戳
    auto now = chrono::system_clock::now();
    auto now_c = chrono::system_clock::to_time_t(now);
    cout << "取出設備當下unixtime時間格式：" << now_c << endl;

    // 2. 獲取當前ISO 8601日期時間格式
    cout << "取出設備當下IOSdate時間格式：" << put_time(localtime(&now_c), "%Y/%m/%d %H:%M:%S") << endl;

    // 3. Unix時間戳轉換為自定義格式
    cout << "unixtime轉換時間格式：" << put_time(localtime(&now_c), "%Y%m%d%H%M%S") << endl;

    // 4. Unix時間戳轉換為ISO 8601格式
    long long unixTime = 1707969427;
    time_t t = unixTime;
    cout << "unixtime轉換IOSdate格式：" << unixTime << "=" << put_time(localtime(&t), "%Y/%m/%d %H:%M:%S") << endl;

    // 5. ISO 8601格式轉換為自定義格式
    string isoDate = "2024/02/15 11:57:07";
    istringstream ss(isoDate);
    tm dt;
    ss >> get_time(&dt, "%Y/%m/%d %H:%M:%S");
    stringstream result;
    result << put_time(&dt, "%Y%m%d%H%M%S");
    cout << "IOSdate格式轉換：" << isoDate << "=" << result.str() << endl;
}


int main() {
    // stringProcess();
    // jsonProcess();
    typeConvert();
    // timeProcess();
    return 0;
}
