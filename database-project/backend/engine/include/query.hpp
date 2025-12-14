#pragma once
#include "database_engine.hpp"
#include <nlohmann/json.hpp>
#include <vector>

struct QueryNode {
    enum class Type { AND, OR, EQ, GT, LT } type;
    std::string field;
    nlohmann::json value;
    std::vector<QueryNode> children;
};

QueryNode parseQuery(const nlohmann::json& filter);
bool evalQuery(const QueryNode& q, const nlohmann::json& doc);
