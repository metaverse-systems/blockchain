#include "utils.hpp"
#include <iomanip>
#include <openssl/evp.h>    

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

    EVP_MD_CTX* mdctx;
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