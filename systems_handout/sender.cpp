#include "protocol.hpp"

#include <arpa/inet.h>
#include <array>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <sys/socket.h>
#include <unistd.h>

namespace {
using namespace protocol;
constexpr int kInputPort = 47010;
constexpr int kRelayPort = 47001;

bool send_shard(int socket_fd, const sockaddr_in& relay, std::uint32_t block,
                std::uint8_t shard, const Shard& payload) {
    std::array<std::uint8_t, kShardPacketBytes> packet{};
    // Pack a 20-bit frame number and 4-bit shard number into three bytes.
    // At 50 frames/second, the frame field lasts more than five hours.
    const std::uint32_t wire_id = (block << 4) | shard;
    packet[0] = static_cast<std::uint8_t>(wire_id >> 16);
    packet[1] = static_cast<std::uint8_t>(wire_id >> 8);
    packet[2] = static_cast<std::uint8_t>(wire_id);
    std::memcpy(packet.data() + kWireIdBytes, payload.data(), payload.size());
    return sendto(socket_fd, packet.data(), packet.size(), 0,
                  reinterpret_cast<const sockaddr*>(&relay), sizeof(relay)) ==
           static_cast<ssize_t>(packet.size());
}

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
}  // namespace

int main() {
    const int input = open_bound_socket(kInputPort);
    if (input < 0) { std::perror("bind sender input"); return 1; }
    const int output = socket(AF_INET, SOCK_DGRAM, 0);
    if (output < 0) { std::perror("create sender output"); close(input); return 1; }

    sockaddr_in relay{};
    relay.sin_family = AF_INET;
    relay.sin_port = htons(kRelayPort);
    relay.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    GaloisField gf;
    std::array<Shard, kDataShards> data{};
    std::array<std::uint8_t, 4 + kFrameBytes> source_packet{};

    for (;;) {
        const ssize_t length = recvfrom(input, source_packet.data(), source_packet.size(), 0, nullptr, nullptr);
        if (length != static_cast<ssize_t>(source_packet.size())) continue;
        std::uint32_t network_sequence;
        std::memcpy(&network_sequence, source_packet.data(), sizeof(network_sequence));
        const std::uint32_t sequence = ntohl(network_sequence);
        const int frame_in_block = static_cast<int>(sequence % kFramesPerBlock);
        const std::uint32_t block = sequence / kFramesPerBlock;

        // Split each frame into small shards so its parity can be sent now,
        // without waiting for future frames to arrive.
        for (int part = 0; part < kShardsPerFrame; ++part) {
            const int shard = frame_in_block * kShardsPerFrame + part;
            std::memcpy(data[shard].data(),
                        source_packet.data() + 4 + part * kShardBytes,
                        kShardBytes);
            send_shard(output, relay, block, static_cast<std::uint8_t>(shard),
                       data[shard]);
        }

        if (frame_in_block != kFramesPerBlock - 1) continue;
        for (int parity = 0; parity < kParityShards; ++parity) {
            Shard encoded{};
            for (int original = 0; original < kDataShards; ++original) {
                const auto coefficient = gf.coefficient(parity, original);
                for (int byte = 0; byte < kShardBytes; ++byte)
                    encoded[byte] ^= gf.multiply(coefficient, data[original][byte]);
            }
            send_shard(output, relay, block,
                       static_cast<std::uint8_t>(kDataShards + parity), encoded);
        }
    }
}
