#include <iostream>
#include <chrono>
#include <iomanip>
#include <sys/stat.h>

#include "Compressor.h"

// ---------------------------------------------------------------------------
// main.cpp
// Simple CLI:
//   huffman c <input> <output.huf>     compress
//   huffman d <input.huf> <output>     decompress
// ---------------------------------------------------------------------------

static long fileSize(const std::string& path) {
    struct stat st{};
    if (stat(path.c_str(), &st) != 0) return -1;
    return static_cast<long>(st.st_size);
}

static void printUsage(const char* prog) {
    std::cerr << "Usage:\n"
              << "  " << prog << " c <input> <output.huf>   Compress a file\n"
              << "  " << prog << " d <input.huf> <output>   Decompress a file\n";
}

int main(int argc, char** argv) {
    if (argc != 4) {
        printUsage(argv[0]);
        return 1;
    }

    std::string mode = argv[1];
    std::string inputPath = argv[2];
    std::string outputPath = argv[3];

    try {
        auto start = std::chrono::high_resolution_clock::now();

        if (mode == "c") {
            Compressor::compress(inputPath, outputPath);
            auto end = std::chrono::high_resolution_clock::now();
            double ms = std::chrono::duration<double, std::milli>(end - start).count();

            long inSize = fileSize(inputPath);
            long outSize = fileSize(outputPath);
            double ratio = inSize > 0 ? (100.0 * (1.0 - static_cast<double>(outSize) / inSize)) : 0.0;

            std::cout << "Compressed successfully.\n"
                      << "  Input:  " << inputPath << " (" << inSize << " bytes)\n"
                      << "  Output: " << outputPath << " (" << outSize << " bytes)\n"
                      << "  Ratio:  " << std::fixed << std::setprecision(2) << ratio << "% smaller\n"
                      << "  Time:   " << std::fixed << std::setprecision(2) << ms << " ms\n";
        } else if (mode == "d") {
            Compressor::decompress(inputPath, outputPath);
            auto end = std::chrono::high_resolution_clock::now();
            double ms = std::chrono::duration<double, std::milli>(end - start).count();

            std::cout << "Decompressed successfully.\n"
                      << "  Input:  " << inputPath << "\n"
                      << "  Output: " << outputPath << "\n"
                      << "  CRC32 verified OK\n"
                      << "  Time:   " << std::fixed << std::setprecision(2) << ms << " ms\n";
        } else {
            printUsage(argv[0]);
            return 1;
        }
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
