#include "server.hpp"

#ifdef _WIN32   // WINDOWS ONLY

#include <winsock2.h>
#include <ws2tcpip.h>
#include <iostream>

#pragma comment(lib, "Ws2_32.lib")

void startServer() {
    WSADATA wsa;
    SOCKET serverSocket, clientSocket;
    sockaddr_in serverAddr{};
    char buffer[4096];

    std::cout << "[SERVER] Initializing Winsock...\n";
    WSAStartup(MAKEWORD(2, 2), &wsa);

    serverSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (serverSocket == INVALID_SOCKET) {
        std::cout << "[ERROR] Socket creation failed\n";
        return;
    }

    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(9000);
    serverAddr.sin_addr.s_addr = INADDR_ANY;

    bind(serverSocket, (sockaddr*)&serverAddr, sizeof(serverAddr));
    listen(serverSocket, 5);

    std::cout << "[SERVER] Listening on port 9000...\n";

    clientSocket = accept(serverSocket, nullptr, nullptr);
    std::cout << "[SERVER] Client connected\n";

    int bytes = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);
    if (bytes > 0) {
        buffer[bytes] = '\0';
        std::cout << "[SERVER] Received: " << buffer << "\n";
    }

    closesocket(clientSocket);
    closesocket(serverSocket);
    WSACleanup();
}

#else
#error This server implementation supports Windows only
#endif
