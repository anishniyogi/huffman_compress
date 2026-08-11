#pragma once
#include <cstdint>
#include <memory>
#include <queue>
#include <vector>
#include <array>
#include <string>
#include <unordered_map>

// ---------------------------------------------------------------------------
// HuffmanTree.h
// Builds a Huffman tree from a byte-frequency table using a priority-based
// min-heap (std::priority_queue), then derives the prefix-free binary code
// for every symbol by walking the tree.
// ---------------------------------------------------------------------------

struct HuffmanNode {
    // For leaf nodes this is the actual byte value; for internal nodes it's
    // unused (we only care about combined frequency for tree construction).
    uint8_t symbol = 0;
    bool isLeaf = false;
    uint64_t frequency = 0;

    std::unique_ptr<HuffmanNode> left;
    std::unique_ptr<HuffmanNode> right;
};

// Comparator for the priority queue: we want a MIN-heap ordered by
// frequency, so the two least-frequent nodes are always popped first
// (the classic greedy Huffman construction step).
struct HuffmanNodeCompare {
    bool operator()(const HuffmanNode* a, const HuffmanNode* b) const {
        return a->frequency > b->frequency; // reversed => min-heap
    }
};

class HuffmanTree {
public:
    // Builds the tree from a frequency table (index = byte value 0-255).
    // Ownership of all nodes is held by root_ via unique_ptr chains.
    void build(const std::array<uint64_t, 256>& freq) {
        // Raw pointers live in the priority_queue for comparison purposes,
        // but every node is actually owned by a unique_ptr we stash in
        // `owned_` so nothing leaks and nothing double-frees.
        std::priority_queue<HuffmanNode*, std::vector<HuffmanNode*>, HuffmanNodeCompare> heap;

        for (int symbol = 0; symbol < 256; ++symbol) {
            if (freq[symbol] > 0) {
                auto node = std::make_unique<HuffmanNode>();
                node->symbol = static_cast<uint8_t>(symbol);
                node->isLeaf = true;
                node->frequency = freq[symbol];
                heap.push(node.get());
                owned_.push_back(std::move(node));
            }
        }

        // Edge case: exactly one distinct symbol in the whole input.
        // A single-node tree has no left/right, so no bit can distinguish
        // it from itself. We handle this specially in encode/decode by
        // just emitting a single "0" bit per occurrence (see Compressor).
        if (heap.empty()) {
            root_ = nullptr;
            return;
        }
        if (heap.size() == 1) {
            root_ = heap.top();
            return;
        }

        // Standard greedy merge: repeatedly take the two smallest-frequency
        // nodes and merge them under a new internal node until one remains.
        while (heap.size() > 1) {
            HuffmanNode* a = heap.top(); heap.pop();
            HuffmanNode* b = heap.top(); heap.pop();

            auto parent = std::make_unique<HuffmanNode>();
            parent->isLeaf = false;
            parent->frequency = a->frequency + b->frequency;

            // We need unique_ptr children, but `a`/`b` are raw pointers into
            // owned_. Find and move their owning unique_ptr into the parent.
            parent->left = takeOwnership(a);
            parent->right = takeOwnership(b);

            HuffmanNode* parentRaw = parent.get();
            owned_.push_back(std::move(parent));
            heap.push(parentRaw);
        }

        root_ = heap.top();
    }

    // Walks the tree and produces a symbol -> bit-string code map.
    std::unordered_map<uint8_t, std::string> buildCodeTable() const {
        std::unordered_map<uint8_t, std::string> table;
        if (!root_) return table;

        if (root_->isLeaf) {
            // Single-symbol edge case: assign a trivial 1-bit code.
            table[root_->symbol] = "0";
            return table;
        }

        std::string path;
        buildCodesRecursive(root_, path, table);
        return table;
    }

    const HuffmanNode* root() const { return root_; }

private:
    // Moves the unique_ptr owning `raw` out of owned_ and returns it.
    // Linear search is fine here: tree construction is O(n) merges total
    // and n is at most 256 (one per byte value).
    std::unique_ptr<HuffmanNode> takeOwnership(HuffmanNode* raw) {
        for (auto& ptr : owned_) {
            if (ptr.get() == raw) {
                std::unique_ptr<HuffmanNode> result = std::move(ptr);
                // Swap-and-pop removal from the owned_ vector.
                ptr = std::move(owned_.back());
                owned_.pop_back();
                return result;
            }
        }
        return nullptr; // should never happen
    }

    static void buildCodesRecursive(HuffmanNode* node, std::string& path,
                                     std::unordered_map<uint8_t, std::string>& table) {
        if (!node) return;
        if (node->isLeaf) {
            table[node->symbol] = path.empty() ? "0" : path;
            return;
        }
        path.push_back('0');
        buildCodesRecursive(node->left.get(), path, table);
        path.pop_back();

        path.push_back('1');
        buildCodesRecursive(node->right.get(), path, table);
        path.pop_back();
    }

    std::vector<std::unique_ptr<HuffmanNode>> owned_;
    HuffmanNode* root_ = nullptr;
};
