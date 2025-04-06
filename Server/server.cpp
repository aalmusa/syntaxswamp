#include "crow.h"
#include "crow/middlewares/cors.h"
#include "sqlite3.h"
#include "AuthMiddleware.h"
#include "DeadlockSafeMutex.h"
#include <iostream>
#include <thread>

// Include our new modular headers
#include "DatabaseUtils.h"
#include "PostLockSystem.h"
#include "Routes.h"

int main() {
    // Use CORS middleware by specifying it in the App template
    crow::App<crow::CORSHandler> app;
    
    // Configure CORS
    auto& cors = app.get_middleware<crow::CORSHandler>();
    cors.global()
        .origin("*")
        .methods("GET"_method, "POST"_method, "PUT"_method, "DELETE"_method, "OPTIONS"_method)
        .headers("Content-Type", "Accept", "Authorization")
        .max_age(3600);
    
    // Initialize SQLite database
    sqlite3* db;
    if (sqlite3_open("codepen.db", &db) != SQLITE_OK) {
        std::cerr << "Cannot open database: " << sqlite3_errmsg(db) << std::endl;
        return 1;
    }
    
    // Create tables if they don't exist
    initializeDatabase(db);
    
    // Create authentication middleware
    AuthMiddleware auth;
    
    // Use deadlock-safe mutexes instead of standard ones
    std::unordered_map<int, std::unique_ptr<DeadlockSafeMutex>> postMutexes;
    DeadlockSafeMutex mutexMapMutex("postMapMutex");
    
    // Post editing lock system
    std::unordered_map<int, PostLock> postLocks;
    DeadlockSafeMutex locksMapMutex("postLocksMapMutex");
    
    // Start a background thread to periodically clean up expired locks
    std::thread cleanupThread([&postLocks, &locksMapMutex]() {
        while (true) {
            std::this_thread::sleep_for(std::chrono::minutes(1));
            cleanupExpiredLocks(postLocks, locksMapMutex);
        }
    });
    cleanupThread.detach();  // Let it run independently
    
    // Root endpoint
    CROW_ROUTE(app, "/")([](){
        return "Codepen Style Website API";
    });
    
    // Setup all routes from our Routes.h module
    setupAuthRoutes(app, db, auth);
    setupPostRoutes(app, db, auth, postMutexes, mutexMapMutex, postLocks, locksMapMutex);
    setupPostLockRoutes(app, db, auth, postLocks, locksMapMutex);
    
    // Set the port, set the app to run on multiple threads, and run the app
    app.port(18080).multithreaded().run();
    
    // Close the database connection when the program ends
    sqlite3_close(db);
    return 0;
}