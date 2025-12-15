#pragma once
#include "database_engine.hpp"
#include <nlohmann/json.hpp>
#include <vector>

struct QueryNode {
    enum class Type {ALWAYS_FALSE, AND, OR, EQ, GT, LT, MATCH_ALL,INVALID  } type;
    std::string field;
    nlohmann::json value;
    std::vector<QueryNode> children;
};

QueryNode parseQuery(const nlohmann::json& filter);
bool evalQuery(const QueryNode& q, const nlohmann::json& doc);
