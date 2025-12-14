#include "server.hpp"
#include "database_engine.hpp"
#include <iostream>


int main() {
    std::cout << "[MAIN] Starting DB Engine...\n";
    
    DatabaseEngine::init(
        "C:/Users/hites/Desktop/database-project/data"
    );


    startServer();  // socket server loop
}
