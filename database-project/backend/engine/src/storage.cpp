#include "storage.hpp"
#include <fstream>
#include <iostream>

using json = nlohmann::json;

void Storage::appendDocument(const std::string& file, const json& doc) {
    std::ofstream out(file, std::ios::binary | std::ios::app);
    if (!out) {
        std::cerr << "[STORAGE][ERROR] Failed to open file for append: " << file << "\n";
        return;
    }

    std::string data = doc.dump();
    uint32_t len = static_cast<uint32_t>(data.size());
    out.write(reinterpret_cast<char*>(&len), sizeof(len));
    out.write(data.data(), len);
    out.flush();

    std::cout << "[STORAGE][APPEND] Wrote doc to " << file << ": " << data << "\n";
}

std::vector<json> Storage::readAll(const std::string& file) {
    std::ifstream in(file, std::ios::binary);
    std::vector<json> docs;
    if (!in) {
        std::cerr << "[STORAGE][READ] File does not exist: " << file << "\n";
        return docs;
    }

    while (true) {
        uint32_t len;
        if (!in.read(reinterpret_cast<char*>(&len), sizeof(len))) break;
        if (len == 0) continue;

        std::string buf(len, '\0');
        if (!in.read(&buf[0], len)) break;

        try {
            docs.push_back(json::parse(buf));
        } catch (...) {
            std::cerr << "[STORAGE][ERROR] Failed to parse doc: " << buf << "\n";
        }
    }

    std::cout << "[STORAGE][READ] Read " << docs.size() << " docs from " << file << "\n";
    return docs;
}

void Storage::writeAll(const std::string& file, const std::vector<json>& docs) {
    std::ofstream out(file, std::ios::binary | std::ios::trunc);
    if (!out) {
        std::cerr << "[STORAGE][WRITEALL] Failed to open file: " << file << "\n";
        return;
    }

    for (const auto& doc : docs) {
        std::string data = doc.dump();
        uint32_t len = static_cast<uint32_t>(data.size());
        out.write(reinterpret_cast<char*>(&len), sizeof(len));
        out.write(data.data(), len);
    }

    std::cout << "[STORAGE][WRITEALL] Wrote " << docs.size() << " docs to " << file << "\n";
}
