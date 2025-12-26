#include "database_engine.hpp"
#include <iostream>
#include <filesystem>
#include <fstream>
#include <vector>
#include <limits>

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
    std::cout << "8. Insert Image (base64)\n";
    std::cout << "0. Exit\n";
    std::cout << "Choose option: ";
}

static std::string base64Encode(const std::vector<unsigned char>& data) {
    static const std::string chars =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz"
        "0123456789+/";

    std::string out;
    int val = 0;
    int valb = -6;

    for (unsigned char c : data) {
        val = (val << 8) + c;
        valb += 8;
        while (valb >= 0) {
            out.push_back(chars[(val >> valb) & 0x3F]);
            valb -= 6;
        }
    }

    if (valb > -6) {
        out.push_back(chars[((val << 8) >> (valb + 8)) & 0x3F]);
    }

    while (out.size() % 4) out.push_back('=');
    return out;
}

int main() {
    std::string userId = "system";
    std::string dbName = "testdb";
    std::string collection = "users";

    while (true) {
        int choice = -1;
        printMenu();
        std::cin >> choice;

        if (std::cin.fail()) {
            std::cin.clear();
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
            std::cout << "❌ Invalid input\n";
            continue;
        }

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

        /* ---------------- INSERT IMAGE ---------------- */
        case 8: {
            std::string imageId, imagePath;

            // consume leftover newline from menu choice
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

            std::cout << "Image ID: ";
            std::getline(std::cin, imageId);

            std::cout << "Image file path: ";
            std::getline(std::cin, imagePath);

            if (imageId.empty() || imagePath.empty()) {
                std::cout << "❌ Image ID or path cannot be empty\n";
                break;
            }

            std::ifstream file(imagePath, std::ios::binary);
            if (!file) {
                std::cout << "❌ Could not open file" << std::endl;
                break;
            }

            std::vector<unsigned char> buffer(
                (std::istreambuf_iterator<char>(file)),
                std::istreambuf_iterator<char>()
            );

            auto encoded = base64Encode(buffer);
            std::string filename = fs::path(imagePath).filename().string();

            DatabaseEngine::insert(
                userId,
                dbName,
                "images",
                {
                    {"id", imageId},
                    {"filename", filename},
                    {"size_bytes", static_cast<long long>(buffer.size())},
                    {"data_base64", encoded}
                }
            );

            std::cout << "✔ Image inserted into 'images' collection\n";
            break;
        }

        default:
            std::cout << "❌ Invalid option\n";
        }
    }

    return 0;
}
