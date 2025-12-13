#include "wal.hpp"
#include <fstream>
#include <iostream>

void WAL::log(const std::string& walPath,
              const std::string& record) {

    std::cout << "[WAL] Appending WAL record" << std::endl;

    std::ofstream out(walPath, std::ios::app);

    if (!out.is_open()) {
        std::cout << "[ERROR] WAL open failed" << std::endl;
        return;
    }

    out << record << "\n";

    std::cout << "[WAL] WAL write successful" << std::endl;
}
