#pragma once
#include <string>
#include <vector>

class WAL {
public:
    static void log(const std::string& walFile,
                    const std::string& entry);

    static std::vector<std::string>
    readAll(const std::string& walFile);

    static void clear(const std::string& walFile);
};
