#pragma once
#include <string>
#include <nlohmann/json.hpp>

enum class WalOp : uint8_t {
    INSERT = 1,
    UPDATE = 2,
    DELETE = 3
};

class WAL {
public:
    static void log(const std::string& file,
                    const nlohmann::json& entry);

                     static std::vector<std::string>
    readAll(const std::string& walFile);

    static void replay(const std::string& file);
    static void clear(const std::string& file);
};
