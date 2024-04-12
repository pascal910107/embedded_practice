#include <iostream>
#include <string>
#include <iconv.h>
#include <iomanip>
#include <algorithm>
#include <sstream>

// 使用iconv進行編碼轉換
std::string utf8_to_gbk(const std::string& utf8_str) {
    size_t in_len = utf8_str.size();
    size_t out_len = in_len * 2 + 1; // GBK編碼長度最大為UTF-8的2倍
    char* out_buf = new char[out_len];
    std::fill(out_buf, out_buf + out_len, 0);

    char* in = (char*)utf8_str.c_str();
    char* out = out_buf;
    iconv_t cd = iconv_open("GBK", "UTF-8");
    if (cd == (iconv_t)-1) {
        delete[] out_buf;
        return "";
    }
    if (iconv(cd, &in, &in_len, &out, &out_len) == (size_t)-1) {
        iconv_close(cd);
        delete[] out_buf;
        return "";
    }
    std::string gbk_str(out_buf);
    iconv_close(cd);
    delete[] out_buf;
    return gbk_str;
}


std::string removeSpaces(const std::string& input) {
    std::string output;
    std::copy_if(input.begin(), input.end(), std::back_inserter(output),
                 [](char c) { return !isspace(static_cast<unsigned char>(c)); });
    return output;
}

std::string formatHexStringToCppArray(const std::string& hexString) {
    std::ostringstream formattedString;
    formattedString << "unsigned char command[] = {\n";
    int lineLength = 8;

    std::string noSpaceString = removeSpaces(hexString);

    for (size_t i = 0; i < noSpaceString.length(); i += 2) {
        if (i % (lineLength * 2) == 0 && i != 0) {
            formattedString << ",\n    ";
        } else {
            formattedString << (i != 0 ? ", " : "    ");
        }
        formattedString << "0x" << noSpaceString.substr(i, 2);
    }

    formattedString << "\n};";
    return formattedString.str();
}

int main() {
    // std::string input = "012345QRSTTV'B"; // UTF-8
    std::string input = "哈囉大家好"; // UTF-8
    std::string gbk_str = utf8_to_gbk(input);

    std::stringstream ss;

    for (size_t i = 0; i < gbk_str.size(); i++) {
        ss << std::hex << std::setw(2) << std::setfill('0') << (int)(unsigned char)gbk_str[i];
    }

    std::string hexString = ss.str();
    std::string formattedCppArray = formatHexStringToCppArray(hexString);
    std::cout << formattedCppArray << std::endl;

    return 0;
}