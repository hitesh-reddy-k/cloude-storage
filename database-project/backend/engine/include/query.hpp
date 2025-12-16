#pragma once
#include <nlohmann/json.hpp>
#include <vector>
#include <string>

struct QueryNode {
    enum class Type {
        ALWAYS_FALSE,
        AND,
        OR,
        EQ,
        MATCH_ALL,
        INVALID
    } type;

    std::string field;
    nlohmann::json value;
    std::vector<QueryNode> children;

    // ✅ Helper to detect MATCH_ALL
    bool isMatchAll() const {
        return type == Type::MATCH_ALL;
    }
};

QueryNode parseQuery(const nlohmann::json& filter);
bool evalQuery(const QueryNode& q, const nlohmann::json& doc);
