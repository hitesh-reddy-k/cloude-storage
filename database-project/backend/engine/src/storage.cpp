#include "storage.hpp"
#include <fstream>
#include <iostream>

using json = nlohmann::json;

void Storage::appendDocument(const std::string& file, json doc) {
    std::ofstream out(file, std::ios::binary | std::ios::app);
    std::string data = doc.dump() + "\n";
    out.write(data.c_str(), data.size());
}

std::vector<json> Storage::readAll(const std::string& file) {
    std::ifstream in(file, std::ios::binary);
    std::vector<json> result;

    std::string line;
    while (std::getline(in, line)) {
        try {
            result.push_back(json::parse(line));
        } catch (...) {}
    }
    return result;
}
