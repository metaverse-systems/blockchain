#pragma once
#include <string>

std::string bytesToHexString(const unsigned char *bytes, size_t length);
std::string sha256(const std::string &str);