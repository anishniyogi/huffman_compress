# Huffman File Compressor

A fast, lossless file compression utility built in C++ using the Huffman coding algorithm. This project reads raw byte streams, making it capable of compressing any file type, from standard text and CSV files to uncompiled source code.

## 🚀 Features

* **Lossless Compression:** Implements a standard Huffman tree architecture to reduce file size without any loss of data.
* **Smart Raw-Storage Fallback:** Intelligently detects high-entropy or pre-compressed files (e.g., modern `.pdf` or `.jpg` formats). If Huffman coding cannot yield a smaller file size, the program automatically falls back to raw byte storage to prevent unnecessary file bloat.
* **Integrity Verification:** Utilizes CRC32 checking during the decompression phase to ensure the restored file is byte-identical to the original input.

## 🛠️ Compilation

To compile the project from the root directory, you will need a C++ compiler that supports C++17 or higher (like GCC/MinGW). Run the following command in your terminal:

```powershell
g++ -std=c++17 -O2 -Iinclude -o huffman.exe src/main.cpp







The executable takes three arguments: the operation flag (c for compress, d for decompress), the input file path, and the output file path.

1. Compressing a File
Use the c flag to compress a file into the custom .huf format.
.\huffman.exe c [input_file] [output_file.huf]
.\huffman.exe c lidar_scan.csv lidar_scan.huf




2. Decompressing a File
Use the d flag to restore a .huf file back to its original format.

PowerShell
.\huffman.exe d [compressed_file.huf] [restored_file]
Example:

PowerShell
.\huffman.exe d lidar_scan.huf restored_lidar_scan.csv

3. Verifying Integrity (Windows)
To verify that the restored binary is completely identical to the original file, use the Windows binary comparison tool:

PowerShell
fc /b lidar_scan.csv restored_lidar_scan.csv
Expected Output: FC: no differences encountered








high compression in files like .wav ,,,  .bmp ,,,, .csv ,,,  .text

low compression in files like jpg,mp4,pdf



