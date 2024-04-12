#include <iostream>
#include <string>
#include <fstream>
#include <vector>

using namespace std;

// 用於 Base64 編碼的 64 個字符
static const string base64_chars = 
             "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
             "abcdefghijklmnopqrstuvwxyz"
             "0123456789+/";

string base64_encode(unsigned char const* bytes_to_encode, unsigned int in_len) {
    string ret; // 用於存最終的 Base64 編碼結果
    int i = 0, j = 0;
    unsigned char char_array_3[3], char_array_4[4]; // 存原始的 8-bit 字節數據 (8*3) 和Base64編碼後的 6-bit 字節數據 (6*4)

    // while 遍歷所有待編碼的字節，每 3 個一組
    while (in_len--) {
        char_array_3[i++] = *(bytes_to_encode++); // 將待編碼的字節存入 char_array_3
        if (i == 3) {
            char_array_4[0] = (char_array_3[0] & 0xfc) >> 2; // 取出第一個字節的前 6 位，即第一個 6-bit 字節
            char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4); // 取出第一個字節的後 2 位和第二個字節的前 4 位，即第二個 6-bit 字節
            char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6); // 取出第二個字節的後 4 位和第三個字節的前 2 位，即第三個 6-bit 字節
            char_array_4[3] = char_array_3[2] & 0x3f; // 取出第三個字節的後 6 位，即第四個 6-bit 字節

            // 將 6-bit 字節轉換為對應的 Base64 字符
            for(i = 0; (i <4) ; i++)
                ret += base64_chars[char_array_4[i]];
            i = 0;
        }
    }

    // 處理最後不足 3 字節的情況，補 0
    if (i) {
        for(j = i; j < 3; j++)
            char_array_3[j] = '\0';

        char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
        char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
        char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);

        for (j = 0; (j < i + 1); j++)
            ret += base64_chars[char_array_4[j]];

        while((i++ < 3))
            ret += '=';
    }

    return ret;
}

string base64_decode(string const& encoded_string) {
    int in_len = encoded_string.size();
    int i = 0, j = 0, in_ = 0;
    unsigned char char_array_4[4], char_array_3[3];
    string ret;

    // while 條件確保只有有效的 Base64 字符才會進行解碼（字母數字（isalnum）、+、/）
    while (in_len-- && ( encoded_string[in_] != '=') && isalnum(encoded_string[in_]) || (encoded_string[in_] == '+') || (encoded_string[in_] == '/')) {
        char_array_4[i++] = encoded_string[in_]; in_++; // 將待解碼的 Base64 字符存入 char_array_4
        if (i ==4) {
            for (i = 0; i <4; i++)
                char_array_4[i] = base64_chars.find(char_array_4[i]); // 將 Base64 字符轉換為對應的 6-bit 數據，ex: A -> 000000 (0), B -> 000001 (1), ...

            char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4); // 取出第一個 6-bit 字節的前 6 位和第二個 6-bit 字節的前 2 位，即第一個 8-bit 字節
            char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2); // 取出第二個 6-bit 字節的後 4 位和第三個 6-bit 字節的前 4 位，即第二個 8-bit 字節
            char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3]; // 取出第三個 6-bit 字節的後 2 位和第四個 6-bit 字節的全部，即第三個 8-bit 字節

            for (i = 0; (i < 3); i++)
                ret += char_array_3[i];
            i = 0;
        }
    }

    // 處理最後不足 4 字節的情況，補 0
    if (i) {
        for (j = i; j <4; j++)
            char_array_4[j] = 0;

        for (j = 0; j <4; j++)
            char_array_4[j] = base64_chars.find(char_array_4[j]);

        char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
        char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
        char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];

        for (j = 0; (j < i - 1); j++) ret += char_array_3[j];
    }

    return ret;
}

vector<unsigned char> read_file(const string& filename) {
    ifstream file(filename, ios::binary); // 以二進制模式打開文件
    if (!file.is_open()) {
        throw runtime_error("Could not open file: " + filename);
    }

    // 尋找文件結尾，獲取文件大小
    file.seekg(0, ios::end);
    streamsize size = file.tellg();
    file.seekg(0, ios::beg);

    // 讀取文件內容到字節數組
    vector<unsigned char> buffer(size);
    if (!file.read(reinterpret_cast<char*>(buffer.data()), size)) {
        throw runtime_error("Failed to read file: " + filename);
    }

    return buffer;
}

void write_file(const string& filename, const vector<unsigned char>& data) {
    ofstream file(filename, ios::binary);
    if (!file.is_open()) {
        throw runtime_error("Could not open file for writing: " + filename);
    }

    file.write(reinterpret_cast<const char*>(data.data()), data.size());
}

int main() {
    // const string original_string = "test";
    // string encoded_string = base64_encode(reinterpret_cast<const unsigned char*>(original_string.c_str()), original_string.length());
    // string decoded_string = base64_decode(encoded_string);

    // cout << "Original: " << original_string << endl;
    // cout << "Encoded: " << encoded_string << endl;
    // cout << "Decoded: " << decoded_string << endl;

    try {
        string filename = "img/logo.svg";
        string new_filename = "img/new_logo.svg";

        vector<unsigned char> image_data = read_file(filename);
        string encoded_data = base64_encode(image_data.data(), image_data.size());
        cout << "Encoded Image: " << encoded_data << endl;

        string decoded_data = base64_decode(encoded_data);
        vector<unsigned char> byte_data(decoded_data.begin(), decoded_data.end()); // 將解碼後的字符串轉換為字節數組
        write_file(new_filename, byte_data);
        cout << "File saved to: " << new_filename << endl;

    } catch (const exception& e) {
        cerr << "Error: " << e.what() << endl;
    }

    return 0;
}
