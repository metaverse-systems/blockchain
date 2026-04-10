#pragma once
#include <string>
#include <cstdint>
#include <filesystem>

std::string bytesToHexString(const unsigned char *bytes, size_t length);
std::string sha256(const std::string &str);
void logMessage(const std::string &level, const std::string &msg);
bool checkLeadingZeroBits(const std::string &hashStr, uint32_t bitsNeeded);
std::string generate_uuid_v4();