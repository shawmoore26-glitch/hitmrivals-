// REGISTRY/Hash/Sha256.h
// Standard SHA-256 (FIPS 180-4), hand-rolled to match this engine's
// zero-external-dependency convention (same reasoning as MiniJson.h).
// Correctness is not assumed -- it's verified in
// tests/registry/test_sha256.cpp against digests computed independently
// via Python's hashlib (ground truth, not memory): SHA256("") and
// SHA256("abc") are the two standard published test vectors, plus one
// vector matching this module's own canonical genome format. A hash
// function that's silently wrong is worse than no hash function --
// LAW C014 (no fake systems) applies here as much as anywhere else in
// this engine.
#pragma once

#include <array>
#include <cstdint>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

namespace dominus::registry {

class Sha256 {
public:
    static std::string Hash(const std::string& input) {
        std::vector<uint8_t> data(input.begin(), input.end());
        return HashBytes(std::move(data));
    }

    static std::string HashBytes(std::vector<uint8_t> data) {
        const uint64_t bitLen = static_cast<uint64_t>(data.size()) * 8;
        data.push_back(0x80);
        while (data.size() % 64 != 56) data.push_back(0x00);
        for (int i = 7; i >= 0; --i) {
            data.push_back(static_cast<uint8_t>((bitLen >> (i * 8)) & 0xFF));
        }

        std::array<uint32_t, 8> h = {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
                                      0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};

        for (size_t chunkStart = 0; chunkStart < data.size(); chunkStart += 64) {
            uint32_t w[64];
            for (int i = 0; i < 16; ++i) {
                w[i] = (static_cast<uint32_t>(data[chunkStart + i * 4]) << 24) |
                       (static_cast<uint32_t>(data[chunkStart + i * 4 + 1]) << 16) |
                       (static_cast<uint32_t>(data[chunkStart + i * 4 + 2]) << 8) |
                       (static_cast<uint32_t>(data[chunkStart + i * 4 + 3]));
            }
            for (int i = 16; i < 64; ++i) {
                uint32_t s0 = RightRotate(w[i - 15], 7) ^ RightRotate(w[i - 15], 18) ^ (w[i - 15] >> 3);
                uint32_t s1 = RightRotate(w[i - 2], 17) ^ RightRotate(w[i - 2], 19) ^ (w[i - 2] >> 10);
                w[i] = w[i - 16] + s0 + w[i - 7] + s1;
            }

            uint32_t a = h[0], b = h[1], c = h[2], d = h[3], e = h[4], f = h[5], g = h[6], hh = h[7];
            for (int i = 0; i < 64; ++i) {
                uint32_t s1 = RightRotate(e, 6) ^ RightRotate(e, 11) ^ RightRotate(e, 25);
                uint32_t ch = (e & f) ^ ((~e) & g);
                uint32_t temp1 = hh + s1 + ch + kRoundConstants[i] + w[i];
                uint32_t s0 = RightRotate(a, 2) ^ RightRotate(a, 13) ^ RightRotate(a, 22);
                uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
                uint32_t temp2 = s0 + maj;
                hh = g;
                g = f;
                f = e;
                e = d + temp1;
                d = c;
                c = b;
                b = a;
                a = temp1 + temp2;
            }
            h[0] += a;
            h[1] += b;
            h[2] += c;
            h[3] += d;
            h[4] += e;
            h[5] += f;
            h[6] += g;
            h[7] += hh;
        }

        std::ostringstream out;
        out << std::hex << std::setfill('0');
        for (uint32_t word : h) out << std::setw(8) << word;
        return out.str();
    }

private:
    static uint32_t RightRotate(uint32_t value, int count) { return (value >> count) | (value << (32 - count)); }

    static constexpr uint32_t kRoundConstants[64] = {
        0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
        0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
        0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
        0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
        0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
        0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
        0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
        0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};
};

}  // namespace dominus::registry
