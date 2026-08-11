#pragma once
#include <cstdint>
#include <array>

// ---------------------------------------------------------------------------
// FileFormat.h
// Defines the on-disk binary layout for our .huf compressed files, plus a
// standalone CRC32 implementation (no external deps) used for integrity
// verification after decompression.
// ---------------------------------------------------------------------------

// Magic bytes at the start of every compressed file. Lets a decompressor
// immediately reject files that aren't ours instead of garbage-decoding them.
constexpr char MAGIC[4] = {'H', 'U', 'F', '1'};

// Flags byte: bit 0 set => the payload is stored RAW (no Huffman coding),
// used as a fallback when compression would not actually save space
// (e.g. already-compressed or random data).
constexpr uint8_t FLAG_RAW_FALLBACK = 0x01;

// ---------------------------------------------------------------------------
// CRC32 (standard polynomial 0xEDB88320, same table as zlib/PNG/gzip use).
// We build the lookup table once at first use ("lazy static" pattern) so
// there's no separate init step the caller has to remember to call.
// ---------------------------------------------------------------------------
class CRC32 {
public:
    // Computes the CRC32 checksum of a buffer of `length` bytes.
    static uint32_t compute(const uint8_t* data, size_t length) {
        const std::array<uint32_t, 256>& table = getTable();
        uint32_t crc = 0xFFFFFFFFu;
        for (size_t i = 0; i < length; ++i) {
            uint8_t index = static_cast<uint8_t>((crc ^ data[i]) & 0xFF);
            crc = (crc >> 8) ^ table[index];
        }
        return crc ^ 0xFFFFFFFFu;
    }

private:
    static const std::array<uint32_t, 256>& getTable() {
        static const std::array<uint32_t, 256> table = [] {
            std::array<uint32_t, 256> t{};
            constexpr uint32_t polynomial = 0xEDB88320u;
            for (uint32_t i = 0; i < 256; ++i) {
                uint32_t c = i;
                for (int bit = 0; bit < 8; ++bit) {
                    // If the low bit is set, shift and XOR with the polynomial;
                    // otherwise just shift. This is the standard bit-by-bit
                    // CRC table generation algorithm.
                    c = (c & 1) ? (polynomial ^ (c >> 1)) : (c >> 1);
                }
                t[i] = c;
            }
            return t;
        }();
        return table;
    }
};
