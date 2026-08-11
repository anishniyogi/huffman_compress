#pragma once
#include <cstdint>
#include <vector>
#include <cstddef>

// ---------------------------------------------------------------------------
// BitIO.h
// Packs individual bits (0/1) into a byte buffer (BitWriter) and unpacks
// them back out one at a time (BitReader). This is how we avoid storing
// each Huffman code as a full std::string of '0'/'1' chars in the output --
// we pack them tightly, 8 bits per byte, MSB-first.
// ---------------------------------------------------------------------------

class BitWriter {
public:
    // Appends a single bit (0 or 1) to the internal buffer.
    void writeBit(uint8_t bit) {
        // Start a new byte whenever we've used up all 8 bits of the current one.
        if (bitPos_ == 0) {
            buffer_.push_back(0);
        }
        if (bit) {
            // MSB-first packing: bit 0 of the code goes into the highest
            // remaining free bit slot of the current byte.
            buffer_.back() |= static_cast<uint8_t>(1u << (7 - bitPos_));
        }
        bitPos_ = (bitPos_ + 1) % 8;
    }

    // Writes an entire code (given as a string of '0'/'1' chars) bit by bit.
    void writeCode(const std::string& code) {
        for (char c : code) {
            writeBit(c == '1' ? 1 : 0);
        }
    }

    // Writes a raw byte directly (used for header fields, not bit-packed data).
    void writeByte(uint8_t byte) {
        buffer_.push_back(byte);
    }

    // Number of padding bits added to fill out the final byte.
    // The decompressor needs this to know when to stop reading bits.
    uint8_t paddingBits() const {
        return bitPos_ == 0 ? 0 : static_cast<uint8_t>(8 - bitPos_);
    }

    const std::vector<uint8_t>& data() const { return buffer_; }

private:
    std::vector<uint8_t> buffer_;
    int bitPos_ = 0; // 0-7, how many bits of the current trailing byte are used
};

class BitReader {
public:
    BitReader(const uint8_t* data, size_t sizeBytes, uint8_t paddingBits)
        : data_(data), sizeBytes_(sizeBytes), paddingBits_(paddingBits) {}

    // Reads the next bit. Returns -1 if we've exhausted all real (non-padding) bits.
    int readBit() {
        size_t totalBits = sizeBytes_ * 8 - paddingBits_;
        if (bitPos_ >= totalBits) {
            return -1;
        }
        size_t byteIndex = bitPos_ / 8;
        int offset = static_cast<int>(bitPos_ % 8);
        int bit = (data_[byteIndex] >> (7 - offset)) & 1;
        ++bitPos_;
        return bit;
    }

    bool hasMore() const {
        return bitPos_ < (sizeBytes_ * 8 - paddingBits_);
    }

private:
    const uint8_t* data_;
    size_t sizeBytes_;
    uint8_t paddingBits_;
    size_t bitPos_ = 0;
};
