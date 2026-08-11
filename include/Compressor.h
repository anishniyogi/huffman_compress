#pragma once
#include <array>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "BitIO.h"
#include "FileFormat.h"
#include "HuffmanTree.h"

// ---------------------------------------------------------------------------
// Compressor.h
// High-level compress()/decompress() entry points. Handles:
//   - reading input file fully into memory
//   - frequency counting
//   - building the self-describing header (magic, size, CRC32, freq table)
//   - Huffman encoding via BitWriter
//   - raw-storage fallback when Huffman doesn't actually help
//   - decompression + CRC32 verification
// ---------------------------------------------------------------------------

class Compressor {
public:
    // Reads inputPath, compresses it, writes result to outputPath.
    // Returns true on success. Throws std::runtime_error on I/O failure.
    static bool compress(const std::string& inputPath, const std::string& outputPath) {
        std::vector<uint8_t> input = readFile(inputPath);

        // --- Step 1: frequency table ---------------------------------------
        std::array<uint64_t, 256> freq{};
        for (uint8_t b : input) freq[b]++;

        uint32_t crc = CRC32::compute(input.data(), input.size());

        // --- Step 2: build Huffman tree + code table ------------------------
        HuffmanTree tree;
        tree.build(freq);
        auto codeTable = tree.buildCodeTable();

        // --- Step 3: encode into a bitstream ---------------------------------
        BitWriter writer;
        for (uint8_t b : input) {
            writer.writeCode(codeTable.at(b));
        }
        const std::vector<uint8_t>& packed = writer.data();

        // --- Step 4: decide whether Huffman coding actually helped ----------
        // We compare compressed payload size (packed bits + freq table
        // overhead) against the raw input size. If compression doesn't pay
        // off (e.g. random/incompressible data, or a tiny file where header
        // overhead dominates), we fall back to storing the data raw.
        size_t distinctSymbols = codeTable.size();
        size_t freqTableBytes = distinctSymbols * (1 + 4); // symbol + uint32 freq
        size_t huffmanTotal = packed.size() + freqTableBytes;

        bool useRaw = huffmanTotal >= input.size();

        // --- Step 5: write output file ---------------------------------------
        std::ofstream out(outputPath, std::ios::binary);
        if (!out) throw std::runtime_error("Cannot open output file: " + outputPath);

        out.write(MAGIC, 4);

        uint8_t flags = useRaw ? FLAG_RAW_FALLBACK : 0x00;
        out.write(reinterpret_cast<const char*>(&flags), 1);

        uint32_t originalSize = static_cast<uint32_t>(input.size());
        writeU32(out, originalSize);
        writeU32(out, crc);

        if (useRaw) {
            // Raw fallback: no frequency table, no padding byte, just the
            // original bytes verbatim after the header.
            out.write(reinterpret_cast<const char*>(input.data()), input.size());
        } else {
            uint16_t numSymbols = static_cast<uint16_t>(distinctSymbols); // 1..256; 256 wraps but see note below
            // NOTE: uint16_t can represent up to 65535 so 256 fits fine (no wraparound here).
            writeU16(out, numSymbols);

            for (int symbol = 0; symbol < 256; ++symbol) {
                if (freq[symbol] > 0) {
                    uint8_t sym = static_cast<uint8_t>(symbol);
                    out.write(reinterpret_cast<const char*>(&sym), 1);
                    writeU32(out, static_cast<uint32_t>(freq[symbol]));
                }
            }

            uint8_t padding = writer.paddingBits();
            out.write(reinterpret_cast<const char*>(&padding), 1);
            out.write(reinterpret_cast<const char*>(packed.data()), packed.size());
        }

        out.close();
        return true;
    }

    // Reads a compressed inputPath, decompresses it, writes result to outputPath.
    // Verifies CRC32 after decoding and throws if it doesn't match (corruption).
    static bool decompress(const std::string& inputPath, const std::string& outputPath) {
        std::vector<uint8_t> input = readFile(inputPath);
        if (input.size() < 13) {
            throw std::runtime_error("File too small to be a valid .huf file");
        }

        size_t pos = 0;
        if (std::memcmp(input.data(), MAGIC, 4) != 0) {
            throw std::runtime_error("Bad magic bytes: not a valid .huf file");
        }
        pos += 4;

        uint8_t flags = input[pos]; pos += 1;
        uint32_t originalSize = readU32(input, pos);
        uint32_t expectedCrc = readU32(input, pos);

        std::vector<uint8_t> output;
        output.reserve(originalSize);

        if (flags & FLAG_RAW_FALLBACK) {
            if (input.size() - pos < originalSize) {
                throw std::runtime_error("Truncated raw payload");
            }
            output.assign(input.begin() + pos, input.begin() + pos + originalSize);
        } else {
            uint16_t numSymbols = readU16(input, pos);

            std::array<uint64_t, 256> freq{};
            for (uint16_t i = 0; i < numSymbols; ++i) {
                if (pos + 5 > input.size()) throw std::runtime_error("Truncated frequency table");
                uint8_t symbol = input[pos]; pos += 1;
                uint32_t f = readU32(input, pos);
                freq[symbol] = f;
            }

            if (pos >= input.size()) throw std::runtime_error("Missing padding byte");
            uint8_t padding = input[pos]; pos += 1;

            // Rebuild the exact same tree from the stored frequency table.
            // Huffman construction is deterministic given the same frequencies
            // and the same tie-breaking rule, so this reconstructs identical
            // codes to what the compressor used -- no code table needs to be
            // stored explicitly, only the frequencies (this is the
            // "self-describing" part of the format).
            HuffmanTree tree;
            tree.build(freq);
            const HuffmanNode* root = tree.root();
            if (!root) throw std::runtime_error("Empty tree for non-empty compressed data");

            BitReader reader(input.data() + pos, input.size() - pos, padding);

            if (root->isLeaf) {
                // Single-distinct-symbol edge case: every bit maps to the same
                // symbol; we just emit it originalSize times.
                for (uint32_t i = 0; i < originalSize; ++i) {
                    output.push_back(root->symbol);
                }
            } else {
                const HuffmanNode* node = root;
                while (output.size() < originalSize) {
                    int bit = reader.readBit();
                    if (bit < 0) throw std::runtime_error("Bitstream ended before decoding finished (corrupt file)");
                    node = (bit == 0) ? node->left.get() : node->right.get();
                    if (!node) throw std::runtime_error("Invalid Huffman path (corrupt file)");
                    if (node->isLeaf) {
                        output.push_back(node->symbol);
                        node = root;
                    }
                }
            }
        }

        // --- Integrity check --------------------------------------------------
        uint32_t actualCrc = CRC32::compute(output.data(), output.size());
        if (actualCrc != expectedCrc) {
            throw std::runtime_error("CRC32 mismatch: decompressed data is corrupt");
        }

        std::ofstream out(outputPath, std::ios::binary);
        if (!out) throw std::runtime_error("Cannot open output file: " + outputPath);
        out.write(reinterpret_cast<const char*>(output.data()), output.size());
        return true;
    }

private:
    static std::vector<uint8_t> readFile(const std::string& path) {
        std::ifstream in(path, std::ios::binary | std::ios::ate);
        if (!in) throw std::runtime_error("Cannot open input file: " + path);
        std::streamsize size = in.tellg();
        in.seekg(0, std::ios::beg);
        std::vector<uint8_t> buffer(static_cast<size_t>(size));
        if (size > 0 && !in.read(reinterpret_cast<char*>(buffer.data()), size)) {
            throw std::runtime_error("Failed to read file: " + path);
        }
        return buffer;
    }

    static void writeU32(std::ofstream& out, uint32_t value) {
        // Little-endian, byte by byte -- portable across machine endianness
        // since we always write/read in the same fixed byte order regardless
        // of host architecture.
        for (int i = 0; i < 4; ++i) {
            uint8_t byte = static_cast<uint8_t>((value >> (i * 8)) & 0xFF);
            out.write(reinterpret_cast<const char*>(&byte), 1);
        }
    }

    static void writeU16(std::ofstream& out, uint16_t value) {
        for (int i = 0; i < 2; ++i) {
            uint8_t byte = static_cast<uint8_t>((value >> (i * 8)) & 0xFF);
            out.write(reinterpret_cast<const char*>(&byte), 1);
        }
    }

    static uint32_t readU32(const std::vector<uint8_t>& buf, size_t& pos) {
        if (pos + 4 > buf.size()) throw std::runtime_error("Unexpected end of file reading uint32");
        uint32_t value = 0;
        for (int i = 0; i < 4; ++i) {
            value |= static_cast<uint32_t>(buf[pos + i]) << (i * 8);
        }
        pos += 4;
        return value;
    }

    static uint16_t readU16(const std::vector<uint8_t>& buf, size_t& pos) {
        if (pos + 2 > buf.size()) throw std::runtime_error("Unexpected end of file reading uint16");
        uint16_t value = 0;
        for (int i = 0; i < 2; ++i) {
            value |= static_cast<uint16_t>(buf[pos + i]) << (i * 8);
        }
        pos += 2;
        return value;
    }
};
