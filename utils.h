#ifndef UTILS_H
#define UTILS_H

#include <string>
#include <vector>
#include <sstream>
#include <iostream>
#include <limits>

using namespace std;

// 读取一个整数（带输入校验，非法输入会提示并重试）
inline int readInt(const string& prompt) {
    int value;
    while (true) {
        cout << prompt;
        if (cin >> value) {
            cin.ignore(numeric_limits<streamsize>::max(), '\n');
            return value;
        }
        cout << "【输入无效】请输入一个整数！\n";
        cin.clear();
        cin.ignore(numeric_limits<streamsize>::max(), '\n');
    }
}

// 读取一个双精度浮点数（带输入校验）
inline double readDouble(const string& prompt) {
    double value;
    while (true) {
        cout << prompt;
        if (cin >> value) {
            cin.ignore(numeric_limits<streamsize>::max(), '\n');
            return value;
        }
        cout << "【输入无效】请输入一个数字！\n";
        cin.clear();
        cin.ignore(numeric_limits<streamsize>::max(), '\n');
    }
}

// 按分隔符拆分字符串（用于文件解析）
inline vector<string> split(const string& s, char delim) {
    vector<string> tokens;
    string token;
    stringstream ss(s);
    while (getline(ss, token, delim)) {
        tokens.push_back(token);
    }
    return tokens;
}

#endif // UTILS_H
