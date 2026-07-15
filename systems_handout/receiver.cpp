#include "protocol.hpp"

#include <arpa/inet.h>
#include <array>
#include <cstdio>
#include <cstring>
#include <map>
#include <sys/socket.h>
#include <utility>
#include <unistd.h>

namespace {
using namespace protocol;
constexpr int kInputPort = 47002;
constexpr int kPlayerPort = 47020;

struct Block {
    std::array<Shard, kShardCount> shards{};
    std::array<bool, kShardCount> received{};
    std::array<bool, kFramesPerBlock> delivered{};
    int received_count = 0;
    bool decoded = false;
};

int open_bound_socket(int port) {
    const int fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) return -1;
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (bind(fd, reinterpret_cast<sockaddr*>(&address), sizeof(address)) < 0) {
        close(fd);
        return -1;
    }
    return fd;
}

bool send_frame(int socket_fd, const sockaddr_in& player, std::uint32_t sequence,
                const Frame& payload) {
    std::array<std::uint8_t, 4 + kFrameBytes> packet{};
    const std::uint32_t network_sequence = htonl(sequence);
    std::memcpy(packet.data(), &network_sequence, sizeof(network_sequence));
    std::memcpy(packet.data() + 4, payload.data(), payload.size());
    return sendto(socket_fd, packet.data(), packet.size(), 0,
                  reinterpret_cast<const sockaddr*>(&player), sizeof(player)) ==
           static_cast<ssize_t>(packet.size());
}

bool invert(ShardMatrix matrix, ShardMatrix& inverse, const GaloisField& gf) {
    inverse = {};
    for (int i = 0; i < kDataShards; ++i) inverse[i][i] = 1;

    for (int column = 0; column < kDataShards; ++column) {
        int pivot = column;
        while (pivot < kDataShards && matrix[pivot][column] == 0) ++pivot;
        if (pivot == kDataShards) return false;
        if (pivot != column) {
            std::swap(matrix[pivot], matrix[column]);
            std::swap(inverse[pivot], inverse[column]);
        }

        const std::uint8_t scale = gf.inverse(matrix[column][column]);
        for (int j = 0; j < kDataShards; ++j) {
            matrix[column][j] = gf.multiply(matrix[column][j], scale);
            inverse[column][j] = gf.multiply(inverse[column][j], scale);
        }
        for (int row = 0; row < kDataShards; ++row) {
            if (row == column || matrix[row][column] == 0) continue;
            const std::uint8_t factor = matrix[row][column];
            for (int j = 0; j < kDataShards; ++j) {
                matrix[row][j] ^= gf.multiply(factor, matrix[column][j]);
                inverse[row][j] ^= gf.multiply(factor, inverse[column][j]);
            }
        }
    }
    return true;
}

void forward_frame(int output, const sockaddr_in& player,
                   std::uint32_t block_number, int frame_number, Block& block) {
    if (block.delivered[frame_number]) return;
    const int first_shard = frame_number * kShardsPerFrame;
    for (int part = 0; part < kShardsPerFrame; ++part)
        if (!block.received[first_shard + part]) return;

    Frame frame{};
    for (int part = 0; part < kShardsPerFrame; ++part)
        std::memcpy(frame.data() + part * kShardBytes,
                    block.shards[first_shard + part].data(), kShardBytes);
    send_frame(output, player,
               block_number * kFramesPerBlock + frame_number, frame);
    block.delivered[frame_number] = true;
}

void recover_block(int output, const sockaddr_in& player,
                   std::uint32_t block_number, Block& block,
                   const GaloisField& gf) {
    if (block.received_count < kDataShards || block.decoded) return;
    std::array<int, kDataShards> selected{};
    ShardMatrix inverse{};
    bool found_solution = false;

    // Reordering may make the first four received shards linearly dependent.
    // Try every small combination, so a later packet can still unlock recovery.
    for (int a = 0; a < kShardCount && !found_solution; ++a) {
        for (int b = a + 1; b < kShardCount && !found_solution; ++b) {
            for (int c = b + 1; c < kShardCount && !found_solution; ++c) {
                for (int d = c + 1; d < kShardCount && !found_solution; ++d) {
                    if (!block.received[a] || !block.received[b] ||
                        !block.received[c] || !block.received[d]) continue;
                    selected = {a, b, c, d};
                    ShardMatrix equations{};
                    for (int row = 0; row < kDataShards; ++row)
                        equations[row] = coding_row(selected[row], gf);
                    found_solution = invert(equations, inverse, gf);
                }
            }
        }
    }
    if (!found_solution) return;
    for (int original = 0; original < kDataShards; ++original) {
        if (block.received[original]) continue;
        Shard recovered{};
        for (int source = 0; source < kDataShards; ++source)
            for (int byte = 0; byte < kShardBytes; ++byte)
                recovered[byte] ^= gf.multiply(inverse[original][source],
                                               block.shards[selected[source]][byte]);
        block.shards[original] = recovered;
        block.received[original] = true;
    }
    block.decoded = true;
    for (int frame = 0; frame < kFramesPerBlock; ++frame)
        forward_frame(output, player, block_number, frame, block);
}
}  // namespace

int main() {
    const int input = open_bound_socket(kInputPort);
    if (input < 0) { std::perror("bind receiver input"); return 1; }
    const int output = socket(AF_INET, SOCK_DGRAM, 0);
    if (output < 0) { std::perror("create receiver output"); close(input); return 1; }

    sockaddr_in player{};
    player.sin_family = AF_INET;
    player.sin_port = htons(kPlayerPort);
    player.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    GaloisField gf;
    std::map<std::uint32_t, Block> blocks;
    std::array<std::uint8_t, kShardPacketBytes> packet{};

    for (;;) {
        const ssize_t length = recvfrom(input, packet.data(), packet.size(), 0, nullptr, nullptr);
        if (length != static_cast<ssize_t>(packet.size())) continue;
        std::uint32_t network_block;
        std::memcpy(&network_block, packet.data(), sizeof(network_block));
        const std::uint32_t block_number = ntohl(network_block);
        const int shard = packet[4];
        if (shard >= kShardCount) continue;

        Block& block = blocks[block_number];
        if (block.received[shard]) continue;  // Duplicate caused by the relay.
        block.received[shard] = true;
        ++block.received_count;
        std::memcpy(block.shards[shard].data(), packet.data() + 5, kShardBytes);

        if (shard < kDataShards) {
            forward_frame(output, player, block_number,
                          shard / kShardsPerFrame, block);
        }
        recover_block(output, player, block_number, block, gf);

        // Keep delayed/adversarial streams from growing memory forever.
        while (blocks.size() > 256) blocks.erase(blocks.begin());
    }
}
