#include "wal.hpp"
#include "database_engine.hpp"

#include <fstream>
#include <iostream>
#include <vector>
#include <string>
#include <nlohmann/json.hpp>

void WAL::log(const std::string& file, const nlohmann::json& entry) {
    std::ofstream out(file, std::ios::binary | std::ios::app);
    if (!out.is_open()) {
        std::cerr << "[WAL] Failed to open WAL file: " << file << "\n";
        return;
    }

    std::string payload = entry.dump();
    uint32_t size = static_cast<uint32_t>(payload.size());
    WalOp op = WalOp::INSERT; // currently only INSERT, extendable later

    out.write(reinterpret_cast<char*>(&op), sizeof(op));
    out.write(reinterpret_cast<char*>(&size), sizeof(size));
    out.write(payload.data(), size);
    out.flush();
}

void WAL::replay(const std::string& file) {
    std::ifstream in(file, std::ios::binary);
    if (!in.is_open()) return;

    while (true) {
        WalOp op;
        uint32_t size;

        if (!in.read(reinterpret_cast<char*>(&op), sizeof(op))) break;
        if (!in.read(reinterpret_cast<char*>(&size), sizeof(size))) break;

        if (size == 0) continue;

        std::string payload(size, '\0');
        if (!in.read(payload.data(), size)) break;

        try {
            auto e = nlohmann::json::parse(payload);
            if (op == WalOp::INSERT) {
                DatabaseEngine::insert(
                    e["userId"], e["db"], e["collection"], e["data"]
                );
            }
            // future: handle UPDATE / DELETE
        } catch (const std::exception& ex) {
            std::cerr << "[WAL] Failed to parse WAL entry: " << ex.what() << "\n";
        }
    }
}

// Read all WAL entries as JSON objects
std::vector<std::string> WAL::readAll(const std::string& walFile) {
    std::ifstream in(walFile, std::ios::binary);
    std::vector<std::string> entries;

    if (!in.is_open()) {
        std::cout << "[WAL] No WAL file found: " << walFile << "\n";
        return entries;
    }

    while (true) {
        WalOp op;
        uint32_t size;
        if (!in.read(reinterpret_cast<char*>(&op), sizeof(op))) break;
        if (!in.read(reinterpret_cast<char*>(&size), sizeof(size))) break;

        if (size == 0) continue;

        std::string payload(size, '\0');
        if (!in.read(payload.data(), size)) break;

        entries.push_back(payload); // keep as string
    }

    std::cout << "[WAL] Read " << entries.size() << " entries\n";
    return entries;
}
