#include "personmanager.h"
#include <cstdlib>

int main() {
    // 设置控制台为 UTF-8，保证中文正常显示
#ifdef _WIN32
    std::system("chcp 65001 > nul");
#endif

    PersonManager manager;
    manager.menu();
    return 0;
}
