#include "crypto.h"
#include <stdexcept>
#include <vector>
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
// MinGW 的 BCrypt 声明依赖 windows.h 中的类型，必须保持此包含顺序。
#include <windows.h>
#include <bcrypt.h>
#else
#include <openssl/evp.h>
#include <openssl/rand.h>
#endif
namespace schoolcrypto {
static std::string hex(const unsigned char *bytes, size_t size) {
    const char digits[] = "0123456789abcdef";
    std::string result;
    result.reserve(size * 2);
    for (size_t i = 0; i < size; ++i) {
        result += digits[bytes[i] >> 4];
        result += digits[bytes[i] & 15];
    }
    return result;
}
static std::vector<unsigned char> unhex(const std::string &value) {
    if (value.size() % 2)
        throw std::runtime_error("Invalid password salt");
    std::vector<unsigned char> result;
    auto digit = [](char c) {
        if (c >= '0' && c <= '9')
            return c - '0';
        if (c >= 'a' && c <= 'f')
            return c - 'a' + 10;
        throw std::runtime_error("Invalid password salt");
    };
    for (size_t i = 0; i < value.size(); i += 2)
        result.push_back(static_cast<unsigned char>((digit(value[i]) << 4) | digit(value[i + 1])));
    return result;
}
std::string randomHex(int bytes) {
    std::vector<unsigned char> data(static_cast<size_t>(bytes));
#ifdef _WIN32
    if (BCryptGenRandom(nullptr, data.data(), static_cast<ULONG>(data.size()),
                        BCRYPT_USE_SYSTEM_PREFERRED_RNG) < 0)
#else
    if (RAND_bytes(data.data(), bytes) != 1)
#endif
        throw std::runtime_error("Secure random generator failed");
    return hex(data.data(), data.size());
}
std::string derive(const std::string &password, const std::string &saltHex, int iterations) {
    if (iterations < 100000 || iterations > 1000000)
        throw std::runtime_error("Invalid password work factor");
    auto salt = unhex(saltHex);
    unsigned char out[32];
#ifdef _WIN32
    BCRYPT_ALG_HANDLE handle = nullptr;
    if (BCryptOpenAlgorithmProvider(&handle, BCRYPT_SHA256_ALGORITHM, nullptr,
                                    BCRYPT_ALG_HANDLE_HMAC_FLAG) < 0)
        throw std::runtime_error("Cannot initialize password hashing");
    auto status = BCryptDeriveKeyPBKDF2(
        handle, reinterpret_cast<PUCHAR>(const_cast<char *>(password.data())),
        static_cast<ULONG>(password.size()), salt.data(), static_cast<ULONG>(salt.size()),
        static_cast<ULONGLONG>(iterations), out, sizeof(out), 0);
    BCryptCloseAlgorithmProvider(handle, 0);
    if (status < 0)
#else
    if (PKCS5_PBKDF2_HMAC(password.data(), static_cast<int>(password.size()), salt.data(),
                          static_cast<int>(salt.size()), iterations, EVP_sha256(), sizeof(out),
                          out) != 1)
#endif
        throw std::runtime_error("Password hashing failed");
    return hex(out, sizeof(out));
}
std::string digest(const std::string &text) {
    unsigned char out[32];
#ifdef _WIN32
    BCRYPT_ALG_HANDLE algorithm = nullptr;
    BCRYPT_HASH_HANDLE hash = nullptr;
    if (BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0) < 0)
        throw std::runtime_error("Cannot initialize session hash");
    auto status = BCryptCreateHash(algorithm, &hash, nullptr, 0, nullptr, 0, 0);
    if (status >= 0)
        status = BCryptHashData(hash, reinterpret_cast<PUCHAR>(const_cast<char *>(text.data())),
                                static_cast<ULONG>(text.size()), 0);
    if (status >= 0)
        status = BCryptFinishHash(hash, out, sizeof(out), 0);
    if (hash)
        BCryptDestroyHash(hash);
    BCryptCloseAlgorithmProvider(algorithm, 0);
    if (status < 0)
#else
    unsigned int length = 0;
    if (EVP_Digest(text.data(), text.size(), out, &length, EVP_sha256(), nullptr) != 1 ||
        length != 32)
#endif
        throw std::runtime_error("Session hashing failed");
    return hex(out, sizeof(out));
}
bool equal(const std::string &a, const std::string &b) {
    if (a.size() != b.size())
        return false;
    unsigned char difference = 0;
    for (size_t i = 0; i < a.size(); ++i)
        difference |= static_cast<unsigned char>(a[i] ^ b[i]);
    return difference == 0;
}
bool validPassword(const std::string &password) {
    size_t characters = 0;
    for (unsigned char c : password) {
        if (c < 32 || c == 127)
            return false;
        if ((c & 192) != 128)
            ++characters;
    }
    return characters >= 8 && characters <= 128 && password.size() <= 512;
}
} // namespace schoolcrypto
