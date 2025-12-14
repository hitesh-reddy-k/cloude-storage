#include "wal.hpp"

#include <fstream>
#include <iostream>
#include <vector>
#include <string>

void WAL::log(const std::string& walFile,
              const std::string& entry) {

    std::ofstream out(walFile, std::ios::app);
    if (!out.is_open()) {
        std::cout << "[WAL] Failed to open WAL file\n";
        return;
    }

    out << entry << "\n";
    std::cout << "[WAL] Logged entry\n";
}


void WAL::replay(const std::string& walFile) {
    std::ifstream in(walFile);
    std::string line;
    while (std::getline(in, line)) {
        std::cout << "Replaying: " << line << std::endl;
        // call database apply logic here
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
