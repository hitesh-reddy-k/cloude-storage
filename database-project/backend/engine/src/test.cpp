#include "database_engine.hpp"
#include <cassert>
#include <iostream>

using json = nlohmann::json;

int main() {
    DatabaseEngine::init("./testdata");

    // INSERT
    DatabaseEngine::insert("system", "testdb", "users", {
        {"id", 1},
        {"email", "a@test.com"},
        {"username", "alpha"}
    });

    DatabaseEngine::insert("system", "testdb", "users", {
        {"id", 2},
        {"email", "b@test.com"},
        {"username", "beta"}
    });

    // FIND OR
    auto res = DatabaseEngine::find(
        "system",
        "testdb",
        "users",
        {
            {"$or", {
                {{"email","a@test.com"}},
                {}
            }}
        }
    );

    assert(res.size() == 2);

    // FIND EQ
    auto res2 = DatabaseEngine::find(
        "system",
        "testdb",
        "users",
        {{"username","beta"}}
    );

    assert(res2.size() == 1);

    std::cout << "✅ ALL TESTS PASSED\n";
    return 0;
}
