#include <iostream>
#include "query.hpp"

static int indent = 0;

static void printIndent() {
    for (int i = 0; i < indent; i++)
        std::cout << "  ";
}

QueryNode parseQuery(const nlohmann::json& filter) {
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
            QueryNode child = parseQuery(f);
            node.children.push_back(child);
        }

        return node;
    }

    // $AND
    if (filter.contains("$and") && filter["$and"].is_array()) {
        node.type = QueryNode::Type::AND;

        for (const auto& f : filter["$and"]) {
            QueryNode child = parseQuery(f);
            node.children.push_back(child);
        }

        return node;
    }

    // FIELD MATCH
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

bool evalQuery(const QueryNode& q, const nlohmann::json& doc) {
    switch (q.type) {

        case QueryNode::Type::INVALID:
            return false;

        case QueryNode::Type::MATCH_ALL:
            return true;

        case QueryNode::Type::OR:
            for (const auto& c : q.children)
                if (evalQuery(c, doc)) return true;
            return false;

        case QueryNode::Type::AND:
            for (const auto& c : q.children)
                if (!evalQuery(c, doc)) return false;
            return true;

        case QueryNode::Type::EQ:
            return doc.contains(q.field) && doc[q.field] == q.value;

        case QueryNode::Type::GT:
            return doc.contains(q.field) && doc[q.field] > q.value;

        case QueryNode::Type::LT:
            return doc.contains(q.field) && doc[q.field] < q.value;
    }

    return false;
}
