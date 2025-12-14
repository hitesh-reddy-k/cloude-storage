#include "query.hpp"

QueryNode parseQuery(const nlohmann::json& filter) {
    QueryNode root;
    if (filter.contains("$or")) {
        root.type = QueryNode::Type::OR;
        for (auto& f : filter["$or"])
            root.children.push_back(parseQuery(f));
    } else if (filter.contains("$and")) {
        root.type = QueryNode::Type::AND;
        for (auto& f : filter["$and"])
            root.children.push_back(parseQuery(f));
    } else {
        root.type = QueryNode::Type::EQ;
        for (auto& [k,v] : filter.items()) {
            root.field = k;
            root.value = v;
        }
    }
    return root;
}


bool evalQuery(const QueryNode& q, const nlohmann::json& doc) {
    switch (q.type) {
        case QueryNode::Type::OR:
            for (auto& c : q.children)
                if (evalQuery(c, doc)) return true;
            return false;
        case QueryNode::Type::AND:
            for (auto& c : q.children)
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