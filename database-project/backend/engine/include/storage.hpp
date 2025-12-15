#pragma once
#include <string>
#include <nlohmann/json.hpp>
#include <vector>

class Storage {
public:
    static void appendDocument(const std::string& file,
                              const nlohmann::json& doc);

    static std::vector<nlohmann::json>
    readAll(const std::string& file);
};
