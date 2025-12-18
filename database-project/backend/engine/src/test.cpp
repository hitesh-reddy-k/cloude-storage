#include "database_engine.hpp"
#include <iostream>
#include <filesystem>

using json = nlohmann::json;
namespace fs = std::filesystem;

void printMenu() {
    std::cout << "\n========= DATABASE ENGINE TEST MENU =========\n";
    std::cout << "1. Init Database\n";
    std::cout << "2. Insert User\n";
    std::cout << "3. Find All Users\n";
    std::cout << "4. Find User (EQ)\n";
    std::cout << "5. Find Users (OR)\n";
    std::cout << "6. Update User\n";
    std::cout << "7. Delete User\n";
    std::cout << "0. Exit\n";
    std::cout << "Choose option: ";
}

int main() {
    std::string userId = "system";
    std::string dbName = "testdb";
    std::string collection = "users";

    int choice;

    while (true) {
        printMenu();
        std::cin >> choice;

        if (choice == 0) {
            std::cout << "👋 Exiting...\n";
            break;
        }

        switch (choice) {

        /* ---------------- INIT ---------------- */
        case 1: {
            DatabaseEngine::init("./testdata");

            fs::path base = "./testdata/system/testdb";
            if (fs::exists(base))
                std::cout << "✔ Database initialized\n";
            else
                std::cout << "❌ Init failed\n";
            break;
        }

        /* ---------------- INSERT ---------------- */
        case 2: {
            int id, age;
            std::string email, username;

            std::cout << "Enter ID: ";
            std::cin >> id;
            std::cout << "Enter Email: ";
            std::cin >> email;
            std::cout << "Enter Username: ";
            std::cin >> username;
            std::cout << "Enter Age: ";
            std::cin >> age;

            DatabaseEngine::insert(
                userId, dbName, collection,
                {
                    {"id", id},
                    {"email", email},
                    {"username", username},
                    {"age", age}
                }
            );

            std::cout << "✔ Inserted\n";
            break;
        }

        /* ---------------- FIND ALL ---------------- */
        case 3: {
            auto res = DatabaseEngine::find(
                userId, dbName, collection, {}
            );

            std::cout << "Found " << res.size() << " users\n";
            for (auto& d : res)
                std::cout << d.dump(2) << "\n";
            break;
        }

        /* ---------------- FIND EQ ---------------- */
        case 4: {
            std::string key, value;
            std::cout << "Field name: ";
            std::cin >> key;
            std::cout << "Value: ";
            std::cin >> value;

            auto res = DatabaseEngine::find(
                userId, dbName, collection,
                {{key, value}}
            );

            std::cout << "Found " << res.size() << " users\n";
            for (auto& d : res)
                std::cout << d.dump(2) << "\n";
            break;
        }

        /* ---------------- FIND OR ---------------- */
        case 5: {
            std::string email1, email2;
            std::cout << "Email 1: ";
            std::cin >> email1;
            std::cout << "Email 2: ";
            std::cin >> email2;

            auto res = DatabaseEngine::find(
                userId, dbName, collection,
                {
                    {"$or", {
                        {{"email", email1}},
                        {{"email", email2}}
                    }}
                }
            );

            std::cout << "Found " << res.size() << " users\n";
            for (auto& d : res)
                std::cout << d.dump(2) << "\n";
            break;
        }

        /* ---------------- UPDATE ---------------- */
        case 6: {
            int id, newAge;
            std::cout << "User ID to update: ";
            std::cin >> id;
            std::cout << "New age: ";
            std::cin >> newAge;

            bool ok = DatabaseEngine::updateOne(
                userId,
                dbName,
                collection,
                {{"id", id}},
                {{"age", newAge}}
            );

            std::cout << (ok ? "✔ Updated\n" : "❌ Update failed\n");
            break;
        }

        /* ---------------- DELETE ---------------- */
        case 7: {
            int id;
            std::cout << "User ID to delete: ";
            std::cin >> id;

            bool ok = DatabaseEngine::deleteOne(
                userId,
                dbName,
                collection,
                {{"id", id}}
            );

            std::cout << (ok ? "✔ Deleted\n" : "❌ Delete failed\n");
            break;
        }

        default:
            std::cout << "❌ Invalid option\n";
        }
    }

    return 0;
}
