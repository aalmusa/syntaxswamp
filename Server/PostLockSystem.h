#pragma once
#include "DeadlockSafeMutex.h"
#include <unordered_map>
#include <chrono>
#include <string>
#include <memory>

// Post editing lock system
struct PostLock {
    int user_id;          // User who acquired the lock
    std::chrono::time_point<std::chrono::system_clock> expires_at; // When the lock expires
    std::string username; // Username of the lock holder (for display purposes)
};

// Default lock duration in seconds
constexpr int DEFAULT_LOCK_DURATION = 300; // 5 minutes

// Function to clean up expired locks
inline void cleanupExpiredLocks(
    std::unordered_map<int, PostLock>& postLocks,
    DeadlockSafeMutex& locksMapMutex) 
{
    if (!locksMapMutex.tryLockWithTimeout(100)) {
        return;  // Skip cleanup if can't acquire lock quickly
    }
    
    try {
        auto now = std::chrono::system_clock::now();
        for (auto it = postLocks.begin(); it != postLocks.end();) {
            if (it->second.expires_at <= now) {
                it = postLocks.erase(it);
            } else {
                ++it;
            }
        }
        locksMapMutex.unlock();
    }
    catch (...) {
        locksMapMutex.unlock();
        std::cerr << "Error during lock cleanup" << std::endl;
    }
}
