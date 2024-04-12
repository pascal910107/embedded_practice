#include <iostream> // 包含標準輸入輸出流對象的頭文件
#include <cstring> // 包含字符串處理函數的頭文件
#include <sys/socket.h> // 包含socket函數和結構的頭文件
#include <netinet/in.h> // 包含網絡地址結構的頭文件
#include <unistd.h> // 提供通用的文件、目錄、程序及進程操作的函數
#include <thread>

void handle_connection(int sock) {
    char buffer[1024] = {0};
    long valread = read(sock, buffer, 1024);
    if (valread > 0) {
        std::cout << buffer << std::endl;
        write(sock, "Hello from server", strlen("Hello from server"));
        std::cout << "Message sent to client" << std::endl;
    }
    close(sock);
}

int main() {
    int server_fd, new_socket; // 宣告socket文件描述符
    long valread;
    struct sockaddr_in address; // 宣告地址結構
    int addrlen = sizeof(address); // 獲取地址結構的長度
    
    // 創建socket文件描述符
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) { // AF_INET 表示IPv4地址，SOCK_STREAM 表示TCP協議
        perror("socket failed"); // 如果socket創建失敗，打印錯誤信息
        exit(EXIT_FAILURE); // 退出程序
    }
    
    address.sin_family = AF_INET; // 指定地址：IPv4
    address.sin_addr.s_addr = INADDR_ANY; // 自動獲取IP地址，INADDR_ANY 為通配地址（0.0.0.0），即任意地址，接收任意IP地址發來的數據
    address.sin_port = htons(8080); // 指定端口號，htons函數確保數據格式符合網絡協議
    
    memset(address.sin_zero, '\0', sizeof address.sin_zero); // 將結構餘下的部分清零
    
    // 綁定socket到指定IP地址和端口
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address))<0) {
        perror("bind failed"); // 如果綁定失敗，打印錯誤信息
        exit(EXIT_FAILURE); // 退出程序
    }
    // 在指定端口開始監聽是否有客戶端連接，參數指定了socket可以排隊的最大連接數目
    if (listen(server_fd, 10) < 0) {
        perror("listen"); // 監聽失敗，打印錯誤信息
        exit(EXIT_FAILURE); // 退出程序
    }
    
    // while(1) { // 進入無限循環等待客戶端連接
    //     printf("\n+++++++ Waiting for new connection ++++++++\n\n");
    //     // 接受客戶端連接，產生一個新的socket，專門用於與該客戶端的通信
    //     if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen))<0) { // accept是一個阻塞函數，會一直等到有客戶端連接
    //         perror("accept"); // 接受連接失敗，打印錯誤信息
    //         exit(EXIT_FAILURE); // 退出程序
    //     }
        
    //     char buffer[30000] = {0}; // 宣告一個大型緩衝區，用於接收消息
    //     valread = read(new_socket, buffer, 30000); // 從客戶端讀取數據，read函數也是一個阻塞函數
    //     printf("%s\n", buffer); // 打印接收到的數據
    //     write(new_socket, "Hello from server", strlen("Hello from server")); // 向客戶端發送數據
    //     close(new_socket); // 關閉與客戶端的連接
    // }

    printf("\n+++++++ Waiting for new connection ++++++++\n\n");
    while(true) {
        new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen);
        if (new_socket < 0) {
            perror("accept");
            continue;
        }
        printf("\nHave new connection\n\n");
        std::thread t(handle_connection, new_socket); // 創建新線程，處理與客戶端的通信
        t.detach(); // 分離線程，讓線程獨立運行
    }
    return 0;
}
