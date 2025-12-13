#include "database_engine.hpp"
#include <nlohmann/json.hpp>

#include <iostream>
#include <string>

using json = nlohmann::json;

void showMenu() {
    std::cout << "\n========== DB ENGINE MENU ==========\n";
    std::cout << "1. Create Database\n";
    std::cout << "2. Create Collection\n";
    std::cout << "3. Insert Document\n";
    std::cout << "4. Find Documents\n";
    std::cout << "5. Update One Document\n";
    std::cout << "6. Delete One Document\n";
    std::cout << "7. Exit\n";
    std::cout << "===================================\n";
    std::cout << "Enter choice: ";
}

int main() {
    std::cout << "\n=== DATABASE ENGINE STARTED ===\n";

    // IMPORTANT: correct data root
    DatabaseEngine::init("../../../../data/databases/");

    std::string userId;
    std::cout << "Enter User ID: ";
    std::getline(std::cin, userId);

    if (userId.empty()) {
        std::cout << "[ERROR] User ID cannot be empty\n";
        return 1;
    }

    while (true) {
        showMenu();

        int choice;
        std::cin >> choice;
        std::cin.ignore(); // flush newline

        if (choice == 1) {
            std::string dbName;
            std::cout << "Enter Database Name: ";
            std::getline(std::cin, dbName);

            if (dbName.empty()) {
                std::cout << "[ERROR] Database name cannot be empty\n";
                continue;
            }

            DatabaseEngine::createDatabase(userId, dbName);
        }

        else if (choice == 2) {
            std::string dbName, collection;
            std::cout << "Enter Database Name: ";
            std::getline(std::cin, dbName);

            std::cout << "Enter Collection Name: ";
            std::getline(std::cin, collection);

            if (dbName.empty() || collection.empty()) {
                std::cout << "[ERROR] DB / Collection name missing\n";
                continue;
            }

            DatabaseEngine::createCollection(userId, dbName, collection);
        }

        else if (choice == 3) {
            std::string dbName, collection, jsonInput;

            std::cout << "Enter Database Name: ";
            std::getline(std::cin, dbName);

            std::cout << "Enter Collection Name: ";
            std::getline(std::cin, collection);

            std::cout << "Enter JSON Document:\n";
            std::cout << "Example: {\"name\":\"Hitesh\",\"age\":19}\n> ";
            std::getline(std::cin, jsonInput);

            try {
                json doc = json::parse(jsonInput);
                DatabaseEngine::insert(userId, dbName, collection, doc);
            }
            catch (const std::exception& e) {
                std::cout << "[ERROR] Invalid JSON: " << e.what() << "\n";
            }
        }

        else if (choice == 4) {
            std::string dbName, collection, filterInput;

            std::cout << "Enter Database Name: ";
            std::getline(std::cin, dbName);

            std::cout << "Enter Collection Name: ";
            std::getline(std::cin, collection);

            std::cout << "Enter Filter JSON ({} for all):\n> ";
            std::getline(std::cin, filterInput);

            try {
                json filter = json::parse(filterInput);
                DatabaseEngine::find(userId, dbName, collection, filter);
            }
            catch (...) {
                std::cout << "[ERROR] Invalid filter JSON\n";
            }
        }

        else if (choice == 5) {
            std::string dbName, collection, filterInput, updateInput;

            std::cout << "Enter Database Name: ";
            std::getline(std::cin, dbName);

            std::cout << "Enter Collection Name: ";
            std::getline(std::cin, collection);

            std::cout << "Enter Filter JSON:\n> ";
            std::getline(std::cin, filterInput);

            std::cout << "Enter Update JSON:\n";
            std::cout << "Example: {\"age\":20}\n> ";
            std::getline(std::cin, updateInput);

            try {
                json filter = json::parse(filterInput);
                json update = json::parse(updateInput);

                bool updated = DatabaseEngine::updateOne(
                    userId, dbName, collection, filter, update
                );

                if (!updated) {
                    std::cout << "[INFO] No matching document found\n";
                }
            }
            catch (...) {
                std::cout << "[ERROR] Invalid JSON input\n";
            }
        }

        else if (choice == 6) {
            std::string dbName, collection, filterInput;

            std::cout << "Enter Database Name: ";
            std::getline(std::cin, dbName);

            std::cout << "Enter Collection Name: ";
            std::getline(std::cin, collection);

            std::cout << "Enter Filter JSON:\n> ";
            std::getline(std::cin, filterInput);

            try {
                json filter = json::parse(filterInput);

                bool deleted = DatabaseEngine::deleteOne(
                    userId, dbName, collection, filter
                );

                if (!deleted) {
                    std::cout << "[INFO] No matching document found\n";
                }
            }
            catch (...) {
                std::cout << "[ERROR] Invalid JSON input\n";
            }
        }

        else if (choice == 7) {
            std::cout << "\n[ENGINE] Shutting down...\n";
            break;
        }

        else {
            std::cout << "[ERROR] Invalid menu option\n";
        }
    }

    std::cout << "=== ENGINE STOPPED ===\n";
    return 0;
}
