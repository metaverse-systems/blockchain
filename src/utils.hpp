#pragma once
#include <string>
#include <filesystem>

std::string bytesToHexString(const unsigned char *bytes, size_t length);
std::string sha256(const std::string &str);
void logMessage(const std::string &level, const std::string &msg);
void loadDotEnv(const std::filesystem::path &path);