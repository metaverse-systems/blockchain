#pragma once

#include <string>
#include <regex>
#include <boost/serialization/serialization.hpp>
#include <boost/serialization/string.hpp>

struct StreamEntry {
    std::string stream;
    std::string key;
    std::string data;

    friend class boost::serialization::access;
    template<class Archive>
    void serialize(Archive &ar, const unsigned int /*version*/) {
        ar & stream;
        ar & key;
        ar & data;
    }
};

inline bool isValidStreamName(const std::string &name) {
    static const std::regex pattern("^[a-zA-Z0-9_-]{1,256}$");
    return std::regex_match(name, pattern);
}

inline bool isValidStreamEntry(const StreamEntry &entry) {
    static constexpr size_t kMaxDataSize = 128ULL * 1024 * 1024; // 128 MB
    return isValidStreamName(entry.stream)
        && !entry.key.empty()
        && entry.data.size() <= kMaxDataSize;
}
