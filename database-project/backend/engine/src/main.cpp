#include "server.hpp"
#include "database_engine.hpp"
#include <iostream>
#include "lsm.hpp"


int main() {
    std::cout << "[MAIN] Starting DB Engine...\n";
    
    DatabaseEngine::init(
        "C:/Users/hites/Desktop/database-project/data"
    );

    // initialize LSM layer with same data root
    LSM::init("C:/Users/hites/Desktop/database-project/data");
    LSM::startBackgroundTasks();

    startServer();  // socket server loop
}
