#include "server.hpp"
#include "database_engine.hpp"
#include <ws2tcpip.h>
#include <thread>
#include <iostream>
#include <nlohmann/json.hpp>

#pragma comment(lib, "Ws2_32.lib")

using json = nlohmann::json;

void handleClient(unsigned long long clientSocket) {
    SOCKET sock = (SOCKET)clientSocket;

    char buffer[8192];
    int bytes = recv(sock, buffer, sizeof(buffer) - 1, 0);

    if (bytes <= 0) {
        closesocket(sock);
        return;
    }

    buffer[bytes] = '\0';
    std::cout << "[SERVER] Received: " << buffer << std::endl;

    json req = json::parse(buffer, nullptr, false);
    json res;

    if (req.is_discarded()) {
        res = { {"error", "Invalid JSON"} };
    }
    else {
        std::string action = req.value("action", "");
        std::cout << "[SERVER] Action = " << action << std::endl;

        // ---------------- PING ----------------
        if (action == "ping") {
            res = { {"status", "pong"} };
        }

        // ---------------- INIT USER SPACE ----------------
        else if (action == "initUserSpace") {
            std::string userId = req.value("userId", "system");
            DatabaseEngine::ensureUserRoot(userId);
            res = { {"status", "ok"}, {"message", "user workspace initialized"} };
        }

        // ---------------- CREATE DATABASE ----------------
        else if (action == "createDatabase") {
            std::string userId = req.value("userId", "system");
            std::string dbName = req.value("dbName", "");

            if (!dbName.empty()) {
                DatabaseEngine::createDatabase(userId, dbName);
                res = { {"status", "ok"}, {"message", "database created"} };
            } else {
                res = { {"error", "dbName required"} };
            }
        }

        // ---------------- CREATE COLLECTION ----------------
        else if (action == "createCollection") {
            std::string userId = req.value("userId", "system");
            std::string dbName = req.value("dbName", "");
            std::string coll  = req.value("collection", "");

            if (!dbName.empty() && !coll.empty()) {
                DatabaseEngine::createCollection(userId, dbName, coll);
                res = { {"status", "ok"}, {"message", "collection created"} };
            } else {
                res = { {"error", "dbName and collection required"} };
            }
        }

        // ---------------- LIST DATABASES ----------------
        else if (action == "listDatabases") {
            std::string userId = req.value("userId", "system");
            auto dbs = DatabaseEngine::listDatabases(userId);
            res = dbs; // respond as array
        }

        // ---------------- INSERT ----------------
        else if (action == "insert") {
            DatabaseEngine::insert(
                req.value("userId", "system"),
                req["dbName"],
                req["collection"],
                req["data"]
            );
            res = { {"status", "inserted"} };
        }

        // ---------------- INSERT VECTOR ----------------
        else if (action == "insertVector") {
            DatabaseEngine::insertVector(
                req.value("userId", "system"),
                req["dbName"],
                req["collection"],
                req["data"]
            );
            res = { {"status", "inserted"} };
        }

        // ---------------- FIND ----------------
        else if (action == "find") {
            std::cout << "[SERVER] Dispatching FIND\n";

            auto results = DatabaseEngine::find(
                req.value("userId", "system"),
                req["dbName"],
                req["collection"],
                req["filter"]
            );

            res["status"] = "ok";
            res["count"]  = results.size();
            res["data"]   = results;
        }

        // ---------------- VECTOR QUERY ----------------
        else if (action == "queryVector") {
            std::cout << "[SERVER] Dispatching VECTOR QUERY\n";
            auto results = DatabaseEngine::queryVector(
                req.value("userId", "system"),
                req["dbName"],
                req["collection"],
                req
            );
            res["status"] = "ok";
            res["count"] = results.size();
            res["data"] = results;
        }

        // ---------------- UPDATE ONE ----------------
        else if (action == "updateOne") {
            std::cout << "[SERVER] Dispatching UPDATE_ONE\n";

            bool ok = DatabaseEngine::updateOne(
                req.value("userId", "system"),
                req["dbName"],
                req["collection"],
                req["filter"],
                req["update"]
            );

            res["status"] = ok ? "updated" : "not_found";
        }

        // ---------------- DELETE ONE ----------------
        else if (action == "deleteOne") {
            std::cout << "[SERVER] Dispatching DELETE_ONE\n";

            bool ok = DatabaseEngine::deleteOne(
                req.value("userId", "system"),
                req["dbName"],
                req["collection"],
                req["filter"]
            );

            res["status"] = ok ? "deleted" : "not_found";
        }

        // ---------------- BULK ----------------
        else if (action == "bulk") {
            // expect: { action: 'bulk', ops: [ { action: 'insert', ... }, { action: 'deleteOne', ... } ] }
            auto ops = req.value("ops", json::array());
            int inserted = 0, updated = 0, deleted_count = 0, errors = 0;

            for (auto &op : ops) {
                try {
                    std::string a = op.value("action", "");
                    if (a == "insert") {
                        DatabaseEngine::insert(op.value("userId", "system"), op["dbName"], op["collection"], op["data"]);
                        inserted++;
                    } else if (a == "updateOne") {
                        bool ok2 = DatabaseEngine::updateOne(op.value("userId", "system"), op["dbName"], op["collection"], op["filter"], op["update"]);
                        if (ok2) updated++; else errors++;
                    } else if (a == "deleteOne") {
                        bool ok3 = DatabaseEngine::deleteOne(op.value("userId", "system"), op["dbName"], op["collection"], op["filter"]);
                        if (ok3) deleted_count++; else errors++;
                    } else {
                        errors++;
                    }
                } catch (...) {
                    errors++;
                }
            }

            res = { {"status", "ok"}, {"inserted", inserted}, {"updated", updated}, {"deleted", deleted_count}, {"errors", errors} };
        }

        // ---------------- UNKNOWN ----------------
        else {
            res = {
                {"error", "Unknown action"},
                {"action", action}
            };
        }
    }

    std::string out = res.dump();
    send(sock, out.c_str(), (int)out.size(), 0);
    closesocket(sock);
}

void startServer() {
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);

    SOCKET server = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(9000);
    addr.sin_addr.s_addr = INADDR_ANY;

    bind(server, (sockaddr*)&addr, sizeof(addr));
    listen(server, SOMAXCONN);

    std::cout << "[SERVER] Listening on port 9000...\n";

    while (true) {
        SOCKET client = accept(server, nullptr, nullptr);
        std::thread(handleClient, (unsigned long long)client).detach();
    }
}
