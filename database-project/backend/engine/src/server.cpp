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
        res = {{"error","Invalid JSON"}};
    }
    else {
        std::string action = req.value("action", "");

        std::cout << "[SERVER] Action = " << action << std::endl;

        // ---------------- PING ----------------
        if (action == "ping") {
            res = {{"status","pong"}};
        }

        // ---------------- INSERT ----------------
        else if (action == "insert") {
            DatabaseEngine::insert(
                "system",                  // userId
                req["dbName"],
                req["collection"],
                req["data"]
            );

            res = {{"status","inserted"}};
        }

        // ---------------- FIND ----------------
        else if (action == "find") {
            std::cout << "[SERVER] Dispatching FIND\n";

            auto results = DatabaseEngine::find(
                "system",                  // userId
                req["dbName"],
                req["collection"],
                req["filter"]
            );

            res["status"] = "ok";
            res["count"]  = results.size();
            res["data"]   = results;
        }

        // ---------------- UNKNOWN ----------------
        else {
            res = {
                {"error","Unknown action"},
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
