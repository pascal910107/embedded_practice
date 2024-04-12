#include <iostream>
#include <cstdio>
#include <memory>
#include <stdexcept>
#include <string>
#include <array>

using namespace std;

string exec(const char* cmd) {
    array<char, 128> buffer;
    string result;
    // 負責管理 FILE 類型的資源，並指定 pclose() 作為資源釋放函數
    // popen() 會開啟一個子程序執行命令，並返回一個指向它的 FILE* 指針，FILE* 被傳遞給 unique_ptr 構造函數管理，當 unique_ptr 被銷毀時自動調用 pclose()
    unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd, "r"), pclose);
    if (!pipe) {
        throw runtime_error("popen() failed!");
    }
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) { // fgets() 讀取一行文字到 buffer 中
        result += buffer.data();
    }
    return result;
}

int main() {
    cout << "Command ls:\n" << exec("ls -al");
    // cout << "Command cat:\n" << exec("cat config.txt");
    // cout << "Command zcat:\n" << exec("zcat logs/2024/02/2024-02-21.log.gz");
    // cout << "Command vim:\n" << exec("vim temp_file.txt");
    // cout << "Command sudo:\n" << exec("sudo ls");
    // cout << "Command chmod:\n" << exec("chmod 777 config.txt"); // user, group, others; read(4), write(2), execute(1)
    // cout << "Command git:\n" << exec("git --version");
    // cout << "Command top:\n" << exec("top"); // 系統資源監控，按 q 離開
    // cout << "Command pwd:\n" << exec("pwd");
    // cout << "Command reboot:\n" << exec("reboot"); // 重啟
    // cout << "Command zip:\n" << exec("zip -r output.zip config.txt"); // -r: recursive
    // cout << "Command unzip:\n" << exec("unzip output.zip");
    // cout << "Command tar:\n" << exec("tar -czvf output.tar.gz config.txt"); // -c: create, -z: gzip, -v: verbose 詳細輸出訊息, -f: file
    // cout << "Command tar:\n" << exec("tar -xzvf output.tar.gz");
    return 0;
}
