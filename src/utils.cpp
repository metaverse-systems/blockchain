#include "utils.hpp"
#include <iomanip>
#include <openssl/evp.h>
#include <chrono>
#include <ctime>
#include <fstream>
#include <iostream>
#include <algorithm>
#include <sstream>
#include <random>

std::string bytesToHexString(const unsigned char *bytes, size_t length)
{
    std::stringstream hex_stream;
    hex_stream << std::hex << std::setfill('0');
    for (size_t i = 0; i < length; ++i)
    {
        hex_stream << std::setw(2) << static_cast<int>(bytes[i]);
    }
    return hex_stream.str();
}

std::string sha256(const std::string &str) {
    unsigned char hash[EVP_MAX_MD_SIZE];
    unsigned int lengthOfHash = 0;

    EVP_MD_CTX *mdctx;
    if((mdctx = EVP_MD_CTX_new()) == nullptr)
    {
        throw std::runtime_error("Failed to create EVP_MD_CTX");
    }

    if(EVP_DigestInit_ex(mdctx, EVP_sha256(), nullptr) != 1)
    {
        EVP_MD_CTX_free(mdctx);
        throw std::runtime_error("Failed to initialize SHA256 digest");
    }

    if(EVP_DigestUpdate(mdctx, str.c_str(), str.size()) != 1)
    {
        EVP_MD_CTX_free(mdctx);
        throw std::runtime_error("Failed to update SHA256 digest");
    }

    if(EVP_DigestFinal_ex(mdctx, hash, &lengthOfHash) != 1)
    {
        EVP_MD_CTX_free(mdctx);
        throw std::runtime_error("Failed to finalize SHA256 digest");
    }

    EVP_MD_CTX_free(mdctx);

    return bytesToHexString(hash, lengthOfHash);
}

void logMessage(const std::string &level, const std::string &msg)
{
    const char *color = "\033[0m";
    if (level == "ERROR") color = "\033[1;31m";      // bold red
    else if (level == "WARN") color = "\033[1;33m";   // bold yellow
    else if (level == "INFO") color = "\033[1;36m";   // bold cyan

    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    std::tm tm_buf;
    localtime_r(&time_t_now, &tm_buf);
    std::cerr << "[" << std::put_time(&tm_buf, "%Y-%m-%d %H:%M:%S") << "] "
              << color << "[" << level << "]\033[0m " << msg << "\n";
}

std::string generate_uuid_v4() {
    std::random_device rd;
    std::mt19937_64 gen(rd());
    std::uniform_int_distribution<uint64_t> dist;

    uint64_t hi = dist(gen);
    uint64_t lo = dist(gen);

    // Set version 4 bits: hi bits 12-15 = 0100
    hi = (hi & 0xFFFFFFFFFFFF0FFFULL) | 0x0000000000004000ULL;
    // Set variant bits: lo bits 62-63 = 10
    lo = (lo & 0x3FFFFFFFFFFFFFFFULL) | 0x8000000000000000ULL;

    // Format as 8-4-4-4-12
    char buf[37];
    std::snprintf(buf, sizeof(buf),
        "%08x-%04x-%04x-%04x-%012llx",
        static_cast<uint32_t>((hi >> 32) & 0xFFFFFFFF),
        static_cast<uint16_t>((hi >> 16) & 0xFFFF),
        static_cast<uint16_t>(hi & 0xFFFF),
        static_cast<uint16_t>((lo >> 48) & 0xFFFF),
        static_cast<unsigned long long>(lo & 0x0000FFFFFFFFFFFFULL));
    return std::string(buf);
}

bool checkLeadingZeroBits(const std::string &hashStr, uint32_t bitsNeeded)
{
    if (bitsNeeded == 0) return true;

    // Lookup table: hex digit -> number of leading zero bits
    // '0'->4, '1'->3, '2'->'3'->2, '4'-'7'->1, '8'-'f'->0
    static const int lzTable[16] = {4, 3, 2, 2, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0};

    uint32_t zeroBits = 0;
    for (char c : hashStr) {
        int nibble;
        if (c >= '0' && c <= '9') nibble = c - '0';
        else if (c >= 'a' && c <= 'f') nibble = 10 + (c - 'a');
        else if (c >= 'A' && c <= 'F') nibble = 10 + (c - 'A');
        else return false;

        if (nibble == 0) {
            zeroBits += 4;
        } else {
            zeroBits += lzTable[nibble];
            break;
        }

        if (zeroBits >= bitsNeeded) return true;
    }
    return zeroBits >= bitsNeeded;
}