#ifndef SCHOOL_CRYPTO_H
#define SCHOOL_CRYPTO_H
#include <string>
namespace schoolcrypto {
constexpr int PasswordIterations = 600000;
std::string randomHex(int bytes = 32);
std::string digest(const std::string &text);
std::string derive(const std::string &password, const std::string &saltHex,
                   int iterations = PasswordIterations);
bool equal(const std::string &a, const std::string &b);
bool validPassword(const std::string &password);
} // namespace schoolcrypto
#endif
