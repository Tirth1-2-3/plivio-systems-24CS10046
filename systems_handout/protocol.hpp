#pragma once

#include <array>
#include <cstdint>

namespace protocol {

constexpr int kFramesPerBlock = 1;
constexpr int kDataShards = 4;
constexpr int kParityShards = 3;
constexpr int kShardCount = kDataShards + kParityShards;
constexpr int kFrameBytes = 160;
constexpr int kShardBytes = kFrameBytes * kFramesPerBlock / kDataShards;
constexpr int kShardsPerFrame = kDataShards / kFramesPerBlock;
constexpr int kWireIdBytes = 3;
constexpr int kShardPacketBytes = kWireIdBytes + kShardBytes;

static_assert(kFrameBytes % kDataShards == 0,
              "A frame must split evenly across the data shards");
static_assert(kShardCount * kShardPacketBytes <=
                  2 * kFramesPerBlock * kFrameBytes,
              "The wire format must stay within the 2x bandwidth cap");
static_assert(kShardCount <= 8, "The shard number must fit in three bits");

using Frame = std::array<std::uint8_t, kFrameBytes>;
using Shard = std::array<std::uint8_t, kShardBytes>;
using ShardMatrix = std::array<std::array<std::uint8_t, kDataShards>, kDataShards>;

// GF(256) with the standard x^8+x^4+x^3+x^2+1 polynomial.
class GaloisField {
 public:
    GaloisField() {
        int value = 1;
        for (int i = 0; i < 255; ++i) {
            exponent_[i] = static_cast<std::uint8_t>(value);
            logarithm_[value] = static_cast<std::uint8_t>(i);
            value <<= 1;
            if (value & 0x100) value ^= 0x11d;
        }
        for (int i = 255; i < 512; ++i) exponent_[i] = exponent_[i - 255];
    }

    std::uint8_t multiply(std::uint8_t a, std::uint8_t b) const {
        if (a == 0 || b == 0) return 0;
        return exponent_[logarithm_[a] + logarithm_[b]];
    }

    std::uint8_t inverse(std::uint8_t value) const {
        return value == 0 ? 0 : exponent_[255 - logarithm_[value]];
    }

    std::uint8_t coefficient(int parity, int original) const {
        // Cauchy matrix: every square submatrix is invertible.
        return inverse(static_cast<std::uint8_t>((parity + 1) ^
                                                  (kParityShards + original + 1)));
    }

 private:
    std::array<std::uint8_t, 512> exponent_{};
    std::array<std::uint8_t, 256> logarithm_{};
};

inline std::array<std::uint8_t, kDataShards> coding_row(int shard,
                                                         const GaloisField& gf) {
    std::array<std::uint8_t, kDataShards> row{};
    if (shard < kDataShards) row[shard] = 1;
    else for (int original = 0; original < kDataShards; ++original)
        row[original] = gf.coefficient(shard - kDataShards, original);
    return row;
}

}  // namespace protocol
