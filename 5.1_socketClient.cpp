#include <iostream> // 包含標準輸入輸出流對象的頭文件
#include <sys/socket.h> // 包含socket函數和結構的頭文件
#include <arpa/inet.h> // 包含inet函數的頭文件
#include <unistd.h> // 提供通用的文件、目錄、程序及進程操作的函數
#include <string.h> // 包含字符串處理函數的頭文件

int main() {
    int sock = 0, valread; // 宣告socket文件描述符和讀取值變量
    struct sockaddr_in serv_addr; // 宣告服務器地址結構
    char buffer[1024] = {0}; // 宣告緩衝區，用於接收服務器的消息
    
    // 創建socket文件描述符
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("\n Socket creation error \n"); // 創建失敗，打印錯誤信息
        return -1;
    }
    
    serv_addr.sin_family = AF_INET; // 指定地址‘：IPv4
    serv_addr.sin_port = htons(8080); // 指定端口號
    
    // 將IP地址從文本轉換為二進制形式
    if(inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr)<=0) {
        printf("\nInvalid address/ Address not supported \n"); // 地址轉換失敗
        return -1;
    }
    
    // 連接到服務器
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        printf("\nConnection Failed \n"); // 連接失敗，打印錯誤信息
        return -1;
    }
    // send(sock, text, strlen(text), 0); // 向服務器發送消息
    // printf("Hello message sent\n"); // 打印發送消息的提示
    // valread = read(sock, buffer, 1024); // 從服務器讀取數據
    // printf("%s\n", buffer); // 打印接收到的數據

    while(true) {
        printf("Enter message (type 'exit' to quit): ");
        char text[1024];
        std::cin.getline(text, 1024); // 從標準輸入讀取一行

        if(strcmp(text, "exit") == 0) break; // 如果輸入為"exit"，則退出循環

        send(sock, text, strlen(text), 0); // 向服務器發送消息
        printf("Message sent\n"); // 打印發送消息的提示
        valread = read(sock, buffer, 1024); // 從服務器讀取數據
        printf("%s\n", buffer); // 打印接收到的數據
        memset(buffer, 0, sizeof(buffer)); // 清空接收緩衝區
    }

    close(sock);
    return 0;
}
