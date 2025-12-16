#include "query.hpp"
#include <iostream>

using json = nlohmann::json;

/* ================= PARSER ================= */

QueryNode parseQuery(const json& filter) {
    QueryNode node;

    // EMPTY OBJECT = MATCH ALL
    if (filter.is_object() && filter.empty()) {
        node.type = QueryNode::Type::MATCH_ALL;
        return node;
    }

    // $OR
    if (filter.contains("$or") && filter["$or"].is_array()) {
        node.type = QueryNode::Type::OR;

        for (const auto& f : filter["$or"]) {
            // 🚫 Skip empty clauses INSIDE $or
            if (f.is_object() && f.empty()) continue;

            node.children.push_back(parseQuery(f));
        }

        return node;
    }

    // $AND
    if (filter.contains("$and") && filter["$and"].is_array()) {
        node.type = QueryNode::Type::AND;

        for (const auto& f : filter["$and"]) {
            node.children.push_back(parseQuery(f));
        }

        return node;
    }

    // FIELD = VALUE
    if (!filter.is_object() || filter.size() != 1) {
        node.type = QueryNode::Type::INVALID;
        return node;
    }

    node.type = QueryNode::Type::EQ;
    auto it = filter.begin();
    node.field = it.key();
    node.value = it.value();

    return node;
}

/* ================= EVALUATOR ================= */

static bool matchField(const QueryNode& node, const json& doc) {
    if (!doc.contains(node.field)) return false;
    return doc[node.field] == node.value;
}

bool evalQuery(const QueryNode& node, const json& doc) {

    switch (node.type) {

    case QueryNode::Type::MATCH_ALL:
        return true;

    case QueryNode::Type::ALWAYS_FALSE:
    case QueryNode::Type::INVALID:
        return false;

    case QueryNode::Type::EQ:
        return matchField(node, doc);

    case QueryNode::Type::AND:
        for (const auto& child : node.children) {
            if (!evalQuery(child, doc)) return false;
        }
        return true;

    case QueryNode::Type::OR: {
        bool hasValidClause = false;

        for (const auto& child : node.children) {
            if (child.isMatchAll()) continue; // 🚫 ignore empty {}
            hasValidClause = true;

            if (evalQuery(child, doc)) return true;
        }

        // If OR had only empty clauses → false
        return hasValidClause ? false : false;
    }
    }

    return false;
}
