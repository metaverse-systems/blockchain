#pragma once
#include <string>
#include <cstdint>
#include <filesystem>
#include <atomic>

enum class LogLevel { Debug = 0, Info = 1, Warning = 2, Error = 3 };

void setLogLevel(LogLevel level);
LogLevel getLogLevel();
LogLevel parseLogLevel(const std::string &str);

std::string bytesToHexString(const unsigned char *bytes, size_t length);
std::string sha256(const std::string &str);
void logMessage(const std::string &level, const std::string &msg);
bool checkLeadingZeroBits(const std::string &hashStr, uint32_t bitsNeeded);
std::string generate_uuid_v4();