#include "storage.hpp"
#include <fstream>
#include <iostream>

using json = nlohmann::json;

void Storage::appendDocument(const std::string& filePath,
                             const json& doc) {

    std::cout << "[STORAGE] Writing to file: "
              << filePath << std::endl;

    std::ofstream out(filePath, std::ios::binary | std::ios::app);

    if (!out.is_open()) {
        std::cout << "[ERROR] Storage file open failed" << std::endl;
        return;
    }

    std::string data = doc.dump();
    uint32_t size = data.size();

    out.write(reinterpret_cast<char*>(&size), sizeof(size));
    out.write(data.data(), size);

    std::cout << "[STORAGE] Document written (" 
              << size << " bytes)" << std::endl;
}
