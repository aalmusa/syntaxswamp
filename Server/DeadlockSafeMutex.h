#pragma once

#include <mutex>
#include <chrono>
#include <thread>
#include <string>
#include <iostream>

/**
 * Enhanced mutex with deadlock detection through timeouts
 * 
 * This class wraps a standard mutex but adds timeout capabilities that help detect
 * and prevent deadlocks. If a lock cannot be acquired within the specified time,
 * it's likely due to a deadlock or high contention.
 */
class DeadlockSafeMutex {
private:
    std::mutex mtx;
    std::string name;
    
public:
    // Constructor with optional name for better debugging
    DeadlockSafeMutex(const std::string& name = "unnamed") : name(name) {}
    
    /**
     * Attempt to lock the mutex with a timeout
     * 
     * @param timeout_ms Maximum time to wait for lock acquisition (in milliseconds)
     * @return true if lock acquired, false if timeout occurred
     */
    bool tryLockWithTimeout(int timeout_ms) {
        auto start = std::chrono::steady_clock::now();
        
        while (true) {
            // Try to get the lock
            if (mtx.try_lock()) {
                return true;
            }
            
            // Check if we've waited too long
            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - start);
            
            if (elapsed.count() >= timeout_ms) {
                std::cerr << "Deadlock warning: Failed to acquire lock on '" 
                          << name << "' after " << timeout_ms << "ms" << std::endl;
                return false;
            }
            
            // Sleep briefly to avoid CPU spin
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
    
    // Regular mutex operations
    void lock() { mtx.lock(); }
    bool try_lock() { return mtx.try_lock(); }
    void unlock() { mtx.unlock(); }
};
