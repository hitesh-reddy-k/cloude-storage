#include "database_engine.hpp"
#include <nlohmann/json.hpp>
#include <iostream>

int main() {
    std::cout << "=== ENGINE START ===" << std::endl;

    DatabaseEngine::init("../../../../data/databases/");

    DatabaseEngine::createDatabase("test-user-123", "users-db");
    DatabaseEngine::createCollection("test-user-123", "users-db", "users");

    nlohmann::json doc = {
        {"name", "Hitesh reddy k"},
        {"age", 19}
    };

    DatabaseEngine::insert("test-user-123", "users-db", "users", doc);

    std::cout << "=== ENGINE END ===" << std::endl;
    return 0;
}
