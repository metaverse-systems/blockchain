#pragma once

#include <stdexcept>
#include <string>

class ChainError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

class ValidationError : public ChainError {
public:
    using ChainError::ChainError;
};

class PersistenceError : public ChainError {
public:
    using ChainError::ChainError;
};

class PeerError : public ChainError {
public:
    using ChainError::ChainError;
};
