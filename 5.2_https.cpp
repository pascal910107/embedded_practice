#include <iostream>
#include <string>
#include <curl/curl.h>

static size_t WriteCallback(void *contents, size_t size, size_t nmemb, std::string *userp) { // userp 是指向緩衝區的指針
    userp->append((char*)contents, size * nmemb); // 將收到的數據附加到 userp 指向的緩衝區
    return size * nmemb; // 每個數據塊的大小 * 數據塊數量 = 總數據大小
}

int main() {
    CURL *curl;
    CURLcode res;
    std::string readBuffer;

    curl = curl_easy_init();
    if(curl) {
        // char *encoded_sid = curl_easy_escape(curl, "0211e144d9650b3a24b933e11f8cdec384f8d806", 0);
        // char *encoded_device_id = curl_easy_escape(curl, "99000001", 0);
        std::string postFields = "sid=0211e144d9650b3a24b933e11f8cdec384f8d806&device_id=99000001";

        curl_easy_setopt(curl, CURLOPT_URL, "https://dev-mg.program.com.tw:443/api/bk/gettime");
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, postFields.c_str());

        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L); // 不驗證證書
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L); // 不檢查證書主機

        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);

        res = curl_easy_perform(curl);
        if(res != CURLE_OK) {
            std::cerr << "curl_easy_perform() failed: " << curl_easy_strerror(res) << std::endl;
        } else {
            std::cout << readBuffer << std::endl;
        }

        curl_easy_cleanup(curl);
    }
    return 0;
}
