#include "wal.hpp"
#include "database_engine.hpp"

#include <fstream>
#include <iostream>
#include <vector>
#include <string>
#include <nlohmann/json.hpp>

void WAL::log(const std::string& file,
              const nlohmann::json& entry) {

    std::ofstream out(file, std::ios::binary | std::ios::app);

    std::string payload = entry.dump();
    uint32_t size = payload.size();
    WalOp op = WalOp::INSERT; // dynamic later


    

    out.write((char*)&op, sizeof(op));
    out.write((char*)&size, sizeof(size));
    out.write(payload.data(), size);
    out.flush();
}


void WAL::replay(const std::string& file) {
    std::ifstream in(file, std::ios::binary);
    if (!in.is_open()) return;

    while (true) {
        WalOp op;
        uint32_t size;

        if (!in.read((char*)&op, sizeof(op))) break;
        in.read((char*)&size, sizeof(size));

        std::string payload(size, '\0');
        in.read(payload.data(), size);

        auto e = nlohmann::json::parse(payload);

        if (op == WalOp::INSERT)
            DatabaseEngine::insert(
                e["userId"], e["db"], e["collection"], e["data"]);
    }
}



std::vector<std::string>
WAL::readAll(const std::string& walFile) {

    std::ifstream in(walFile);
    std::vector<std::string> entries;
    std::string line;

    if (!in.is_open()) {
        std::cout << "[WAL] No WAL file found\n";
        return entries;
    }

    std::cout << "[WAL] Writing entry to: " << walFile << std::endl;

    while (std::getline(in, line)) {
        entries.push_back(line);
    }

    std::cout << "[WAL] Read "
              << entries.size()
              << " WAL entries\n";

    return entries;
}

void WAL::clear(const std::string& walFile) {
    std::ofstream out(walFile, std::ios::trunc);
    std::cout << "[WAL] WAL cleared\n";
}
