# Huffman Compressor

A from-scratch, dependency-free file compressor in C++17 using canonical Huffman coding. Built as a CLI tool that compresses any file into a self-describing `.huf` format and losslessly restores it, with CRC32 integrity verification on decompression.

## Features

- **Self-describing format** — the frequency table is stored in the file header, so the decompressor rebuilds the exact same Huffman tree without needing a separate code table.
- **Bit-packed output** — codes are packed 8 bits per byte (MSB-first) via a custom `BitWriter`/`BitReader`, instead of wasting a byte per `'0'`/`'1'` character.
- **CRC32 integrity check** — every decompression is verified against a stored CRC32 checksum; corrupted output throws instead of silently succeeding.
- **Raw fallback mode** — if Huffman coding would *increase* the file size (e.g. already-compressed or high-entropy data), the compressor detects this and stores the file raw instead, so output is never larger than input by more than the header size.
- **Single-symbol edge case handled** — files containing only one distinct byte value are compressed correctly (no tree with a single node/no left-right ambiguity issue).
- **Zero external dependencies** — pure C++17 standard library.

## How it works

1. **Frequency counting** — read the whole input file and count occurrences of each of the 256 possible byte values.
2. **Tree construction** — build a Huffman tree with a min-heap (`std::priority_queue`): repeatedly merge the two lowest-frequency nodes until one root remains.
3. **Code table** — walk the tree to assign each byte a prefix-free binary code (shorter codes for more frequent bytes).
4. **Bit-packed encoding** — write each byte's code into a `BitWriter`, packing bits tightly into a byte buffer.
5. **Fallback check** — compare the packed size (+ frequency table overhead) against the original size; use raw storage if Huffman coding doesn't actually win.
6. **Header + payload** — write magic bytes, flags, original size, CRC32, (frequency table + padding info if Huffman was used), then the payload.

Decompression reverses this: read the header, rebuild the identical tree from the stored frequencies, walk the tree bit-by-bit to decode symbols, and verify the CRC32 of the result.

## File format (`.huf`)

| Offset | Field | Size |
|---|---|---|
| 0 | Magic bytes `"HUF1"` | 4 bytes |
| 4 | Flags (bit 0 = raw fallback) | 1 byte |
| 5 | Original size (little-endian) | 4 bytes |
| 9 | CRC32 of original data (little-endian) | 4 bytes |
| 13 | *(Huffman mode only)* Number of distinct symbols | 2 bytes |
| — | *(Huffman mode only)* Frequency table: `[symbol, freq]` pairs | 5 bytes each |
| — | *(Huffman mode only)* Padding bits in final byte | 1 byte |
| — | Payload (raw bytes, or bit-packed Huffman codes) | variable |

## Build

Requires CMake ≥ 3.10 and a C++17 compiler.

```bash
mkdir build && cd build
cmake ..
make
```

This produces a `huffman` executable.

> **Note:** the current `CMakeLists.txt` expects an `include/` directory for headers and `src/main.cpp` as the source. Arrange the project as:
> ```
> .
> ├── CMakeLists.txt
> ├── include/
> │   ├── BitIO.h
> │   ├── Compressor.h
> │   ├── FileFormat.h
> │   └── HuffmanTree.h
> └── src/
>     └── main.cpp
> ```

## Usage

```bash
# Compress
./huffman c <input> <output.huf>

# Decompress
./huffman d <input.huf> <output>
```

Example:

```bash
./huffman c photo.bmp photo.huf
./huffman d photo.huf photo_restored.bmp
```

The CLI reports input/output size, compression ratio, and elapsed time on compress; and CRC32 verification status on decompress.

## Compression behavior by file type

Huffman coding's effectiveness depends entirely on how skewed a file's byte-frequency distribution is — it compresses well when some bytes are much more common than others, and poorly (or not at all) when byte values are close to uniformly distributed.

| File type | Result | Why |
|---|---|---|
| `.txt`, `.csv` | Strong compression | Text has a highly skewed byte distribution (common letters/delimiters vs. rare ones) |
| `.bmp` | Strong compression | Uncompressed bitmaps often have repetitive byte patterns (large flat color regions, padding) |
| `.wav` | Good compression | Uncompressed PCM audio has non-uniform amplitude byte distributions, especially with silence/low-dynamic-range passages |
| `.pdf` | Weak/partial compression | PDFs mix plain text/structure (compressible) with already-compressed streams (e.g. Flate-encoded content), so gains are limited to the uncompressed portions |
| `.jpg` | No compression (raw fallback triggers, or output ≈ input size) | JPEG is already an entropy-coded, near-random byte stream; there's no meaningful frequency skew left for Huffman to exploit |

This matches the theory: **Huffman coding cannot meaningfully compress data that is already compressed or is high-entropy/random**, since its whole gain comes from redundancy in the symbol distribution. The raw-fallback mechanism exists precisely to guard against actively *growing* such files.

## Limitations

- Static/single-pass Huffman coding (one code table per file) — no adaptive modeling, so it can't exploit patterns beyond byte-frequency (e.g. repeated substrings, as LZ-family compressors do).
- Whole file is read into memory — not suited for files larger than available RAM.
- No multi-file/archive support — one file in, one file out.

## Possible future improvements

- Combine with an LZ-style pass (e.g. LZ77 + Huffman, like DEFLATE) to also exploit repeated substrings — this would meaningfully improve PDF and JPEG-adjacent results.
- Streaming mode to avoid loading the entire file into memory.
- Directory/archive support.

