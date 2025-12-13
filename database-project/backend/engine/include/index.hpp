#pragma once
#include <string>
#include <vector>

class WAL {
public:
    static void log(const std::string& file, const std::string& entry);
    static void replay(const std::string& walFile);
};
