#pragma once
#include <string>
#include <vector>

class WAL {
public:
    static void log(const std::string& walFile,const std::string& entry);

    static void replay(const std::string& walFile);

    static std::vector<std::string>
    readAll(const std::string& walFile);

    static void clear(const std::string& walFile);
};
