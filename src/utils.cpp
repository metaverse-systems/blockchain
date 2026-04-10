#include "utils.hpp"
#include <iomanip>
#include <openssl/evp.h>
#include <chrono>
#include <ctime>
#include <fstream>
#include <iostream>
#include <algorithm>

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

void loadDotEnv(const std::filesystem::path &path)
{
    std::ifstream ifs(path);
    if (!ifs.is_open()) {
        return; // Silently skip if file doesn't exist
    }

    std::string line;
    while (std::getline(ifs, line)) {
        // Trim leading whitespace
        auto start = line.find_first_not_of(" \t");
        if (start == std::string::npos) continue;
        line = line.substr(start);

        // Skip comments and empty lines
        if (line.empty() || line[0] == '#') continue;

        auto eq_pos = line.find('=');
        if (eq_pos == std::string::npos) continue;

        std::string key = line.substr(0, eq_pos);
        std::string value = line.substr(eq_pos + 1);

        // Trim whitespace from key
        auto key_end = key.find_last_not_of(" \t");
        if (key_end != std::string::npos)
            key = key.substr(0, key_end + 1);

        // Trim whitespace from value
        auto val_start = value.find_first_not_of(" \t");
        if (val_start != std::string::npos)
            value = value.substr(val_start);
        else
            value = "";

        auto val_end = value.find_last_not_of(" \t\r\n");
        if (val_end != std::string::npos)
            value = value.substr(0, val_end + 1);
        else
            value = "";

        // Strip surrounding quotes
        if (value.size() >= 2 &&
            ((value.front() == '"' && value.back() == '"') ||
             (value.front() == '\'' && value.back() == '\''))) {
            value = value.substr(1, value.size() - 2);
        }

#ifdef _WIN32
        _putenv_s(key.c_str(), value.c_str());
#else
        setenv(key.c_str(), value.c_str(), 0); // Don't overwrite existing env vars
#endif
    }
}