#include <iostream>
#include <fstream>
#include <thread>
#include <sys/epoll.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <vector>
#include <map>

using namespace std;


map<int, int> gpioFdMap;

// ===================
// 設定GPIO引腳的值
// ===================
void setGPIOValue(const string& pin, const string& value) {
    string path = "/sys/class/gpio/gpio" + pin + "/value";
    ofstream file(path);
    if (file.is_open()) {
        file << value;
        file.close();
        cout << "GPIO" << pin << " value 被設定為 " << value << endl;
    } else {
        cerr << "無法開啟 " << path << " 檔案" << endl;
    }
}

// ===================
// 初始設置 GPIO
// ===================
void setupGpio(int gpio, bool isInput) {
    char buffer[64];
    int fd = open("/sys/class/gpio/export", O_WRONLY);
    sprintf(buffer, "%d", gpio);
    write(fd, buffer, strlen(buffer));
    close(fd);

    sprintf(buffer, "/sys/class/gpio/gpio%d/direction", gpio);
    fd = open(buffer, O_WRONLY);
    if (isInput) {
        write(fd, "in", 2);
    } else {
        write(fd, "out", 3);
    }

    sprintf(buffer, "/sys/class/gpio/gpio%d/edge", gpio);
    fd = open(buffer, O_WRONLY);
    if (isInput) {
        write(fd, "both", 4);
    } else {
        write(fd, "none", 4);
    }
    close(fd);
}


// ===================
// 監聽 GPIO 變化
// ===================
void monitorGpio(int gpio, int epollFd) {
    char buffer[64];
    sprintf(buffer, "/sys/class/gpio/gpio%d/value", gpio);
    int fd = open(buffer, O_RDONLY | O_NONBLOCK);

    gpioFdMap[gpio] = fd;
    cout << "開始監聽 GPIO" << gpio << "，fd 為" << fd << endl ;

    struct epoll_event event;
    event.events = EPOLLPRI | EPOLLERR;
    event.data.fd = fd;
    epoll_ctl(epollFd, EPOLL_CTL_ADD, fd, &event);

    read(fd, buffer, sizeof(buffer)); // 預先讀取一次，預防之前的未讀取事件被 epoll_wait 誤判
}


int main() {
    vector<int> inputGpios = {18, 19, 20, 21};
    vector<int> outputGpios = {0, 1};

    map<int, int> fdGpioValueMap;
    for (int gpio : inputGpios) {
        fdGpioValueMap[gpioFdMap[gpio]] = 1;
    }

    // for (int gpio : outputGpios) {
    //     setupGpio(gpio, false);
    // }
    // for (int gpio : inputGpios) {
    //     setupGpio(gpio, true);
    // }

    int epollFd = epoll_create(1);
    cout << "創建 epoll_fd=" << epollFd << endl << endl << endl;
    for (int gpio : inputGpios) {
        thread(monitorGpio, gpio, epollFd).detach();
    }

    struct epoll_event events[4];
    char value;
    while (true) {
        cout << "等待事件" << endl;
        int n = epoll_wait(epollFd, events, 4, -1);
        cout << "監聽到 " << n << " 個事件，";
        for (int i = 0; i < n; ++i) {
            int fd = events[i].data.fd;
            cout << "事件發生在 fd = " << fd << endl;
            lseek(fd, 0, SEEK_SET); // 將檔案指標移回開頭
            if (read(fd, &value, 1) > 0) {
                cout << "讀取到值: " << value << endl;
                if (value == '0') {
                    fdGpioValueMap[fd] = 0;
                    if (fd == gpioFdMap[18]) {
                        cout << "B 線圈觸發" << endl;
                        if (fdGpioValueMap[gpioFdMap[21]] == 1) {
                            setGPIOValue(to_string(outputGpios[0]), "0"); // 關閘門
                            cout << "關閘門" << endl;
                            setGPIOValue(to_string(outputGpios[1]), "1"); // 滿車
                            cout << "滿車" << endl;
                        }
                    } else if (fd == gpioFdMap[21]) {
                        cout << "A 線圈觸發" << endl;
                        setGPIOValue(to_string(outputGpios[0]), "1"); // 開閘門
                        cout << "開閘門" << endl;
                    }
                } else {
                    fdGpioValueMap[fd] = 1;
                    if (fd == gpioFdMap[18]) {
                        cout << "B 線圈離開" << endl;
                    } else if (fd == gpioFdMap[21]) {
                        cout << "A 線圈離開" << endl;
                        if (fdGpioValueMap[gpioFdMap[18]] == 0) {
                            setGPIOValue(to_string(outputGpios[0]), "0"); // 關閘門
                            cout << "關閘門" << endl;
                            setGPIOValue(to_string(outputGpios[1]), "1"); // 滿車
                            cout << "滿車" << endl;
                        }
                    }
                }
            }
        }
    }
    return 0;
}
