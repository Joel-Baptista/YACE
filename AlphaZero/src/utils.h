#pragma once

#include <fstream>
#include <string>
#include <ctime>
#include <iostream>
#include <random>
#include <torch/torch.h>

#define U64 unsigned long long

std::string getCurrentTimestamp();
void logMessage(const std::string& message, const std::string& filename);
torch::Tensor dirichlet_noise(double alpha, int batch_size);
void initLogFile(const std::string& filename);

void copy_weights(const torch::nn::Module& source, torch::nn::Module& target);

// Define a wrapper class for the array
class BitboardKey {
public:
    U64 bitboards[12];

    // Constructor
    BitboardKey(const U64 bbs[12]) {
        std::memcpy(bitboards, bbs, sizeof(bitboards));
    }

    // Equality operator
    bool operator==(const BitboardKey& other) const {
        return std::memcmp(bitboards, other.bitboards, sizeof(bitboards)) == 0;
    }
};

// Define the hash function for the wrapper class
struct BitboardHash {
    std::size_t operator()(const BitboardKey& key) const {
        std::size_t hash = 0;
        for (const auto& value : key.bitboards) {
            hash ^= std::hash<U64>{}(value) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
        }
        return hash;
    }
};