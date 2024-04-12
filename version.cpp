#include <iostream>

int main() {
    #if defined(__cplusplus)
    std::cout << "C++ 標準版本: " << __cplusplus << std::endl;
    #else
    std::cout << "不是在C++環境中編譯。" << std::endl;
    #endif

    return 0;
}