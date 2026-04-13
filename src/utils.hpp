#pragma once
#include <string>
#include <cstdint>
#include <filesystem>
#include <atomic>
#include <utility>

enum class LogLevel { Debug = 0, Info = 1, Warning = 2, Error = 3 };

void setLogLevel(LogLevel level);
LogLevel getLogLevel();
LogLevel parseLogLevel(const std::string &str);

std::string bytesToHexString(const unsigned char *bytes, size_t length);
std::string sha256(const std::string &str);
std::string chunkFilename(size_t index);
std::pair<std::string, uint16_t> parsePeerKey(const std::string &key);
void logMessage(const std::string &level, const std::string &msg);
bool checkLeadingZeroBits(const std::string &hashStr, uint32_t bitsNeeded);
std::string generate_uuid_v4();

// Lazy log macros — suppress message expression evaluation when the level is below threshold
#define LOG_DEBUG(msg) do { if (getLogLevel() <= LogLevel::Debug)   logMessage("DEBUG", msg); } while (0)
#define LOG_INFO(msg)  do { if (getLogLevel() <= LogLevel::Info)    logMessage("INFO",  msg); } while (0)
#define LOG_WARN(msg)  do { if (getLogLevel() <= LogLevel::Warning) logMessage("WARN",  msg); } while (0)
#define LOG_ERROR(msg) do { if (getLogLevel() <= LogLevel::Error)   logMessage("ERROR", msg); } while (0)