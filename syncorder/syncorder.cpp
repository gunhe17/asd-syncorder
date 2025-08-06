#pragma once

#include <vector>
#include <thread>
#include <memory>
#include <atomic>
#include <iostream>
#include <chrono>
#include <future>
#include <functional>
#include <string>

#include <syncorder/devices/common/manager_base.h>

/**
 * @class
 */

class Syncorder {
private:
    std::vector<std::unique_ptr<BManager>> managers_;
    std::atomic<bool> abort_flag_{false};
    std::chrono::milliseconds default_timeout_{5000};
    
public:
    void addDevice(std::unique_ptr<BManager> manager) {
        if (!manager) {
            std::cout << "[syncorder] Warning: null manager ignored\n";
            return;
        }
        
        std::cout << "[syncorder] Added manager: " << manager->__name__() << "\n";
        managers_.push_back(std::move(manager));
    }
    
    bool executeSetup() {
        std::cout << "[syncorder] Coordinating setup phase...\n";
        bool result = executeStage("setup", [](BManager& manager) {
            manager.setup();
            return manager.__is_setup__();
        });

        std::cout << "[syncorder] Setup phase " << (result ? "completed" : "failed") << "\n";
        return result;
    }
    
    bool executeWarmup() {
        std::cout << "[syncorder] Coordinating warmup phase...\n";
        bool result = executeStage("warmup", [](BManager& manager) {
            manager.warmup();
            return manager.__is_warmup__();
        });

        std::cout << "[syncorder] Warmup phase " << (result ? "completed" : "failed") << "\n";
        return result;
    }
    
    bool executeStart() {
        std::cout << "[syncorder] Coordinating start phase...\n";
        bool result = executeStage("start", [](BManager& manager) {
            manager.start();
            return manager.__is_running__();
        });

        std::cout << "[syncorder] Start phase " << (result ? "completed" : "failed") << "\n";
        return result;
    }
    
    void executeStop() {
        std::cout << "[syncorder] Coordinating stop phase...\n";
        std::vector<std::future<void>> futures;
        
        for (auto& manager : managers_) {
            futures.push_back(
                std::async(std::launch::async, [&manager]() {
                    try {
                        manager->stop();
                        std::cout << "[" << manager->__name__() << "] Manager stopped\n";
                    } catch (const std::exception& e) {
                        std::cout << "[" << manager->__name__() << "] Stop error: " << e.what() << "\n";
                    }
                })
            );
        }
        
        waitForAllFutures(futures, default_timeout_);

        std::cout << "[syncorder] Stop phase completed\n";
    }
    
    void executeCleanup() {
        std::cout << "[syncorder] Coordinating cleanup phase...\n";
        for (auto& manager : managers_) {
            try {
                manager->cleanup();
                std::cout << "[" << manager->__name__() << "] Manager cleaned up\n";
            } catch (const std::exception& e) {
                std::cout << "[" << manager->__name__() << "] Cleanup error: " << e.what() << "\n";
            }
        }

        std::cout << "[syncorder] Cleanup phase completed\n";
    }
    
    void abort() {
        std::cout << "[syncorder] Abort requested\n";
        abort_flag_.store(true);
        executeStop();
    }
    
    void setTimeout(std::chrono::milliseconds timeout) {
        default_timeout_ = timeout;
        std::cout << "[syncorder] Timeout set to " << timeout.count() << "ms\n";
    }
    
    size_t getDeviceCount() const {
        return managers_.size();
    }
    
    bool isAborted() const {
        return abort_flag_.load();
    }

private:
    template<typename StageFunc>
    bool executeStage(const std::string& stage_name, StageFunc func) {
        if (abort_flag_.load()) {
            std::cout << "[syncorder] Aborted during " << stage_name << "\n";
            return false;
        }
        
        if (managers_.empty()) {
            std::cout << "[syncorder] No managers registered\n";
            return false;
        }
        
        std::vector<std::future<bool>> futures;
        
        for (auto& manager : managers_) {
            futures.push_back(
                std::async(std::launch::async, [&manager, &func, &stage_name]() {
                    try {
                        bool success = func(*manager);

                        return success;
                    } catch (const std::exception& e) {
                        std::cout << "[" << manager->__name__() << "] Manager " << stage_name << " error: " << e.what() << "\n";
                        return false;
                    }
                })
            );
        }
        
        bool all_success = waitForAllFutures(futures, default_timeout_);
        
        if (!all_success) {
            std::cout << "[syncorder] " << stage_name << " phase failed\n";
            abort_flag_.store(true);
        } else {
            std::cout << "[syncorder] " << stage_name << " phase completed successfully\n";
        }
        
        return all_success;
    }
    
    template<typename T>
    bool waitForAllFutures(std::vector<std::future<T>>& futures, std::chrono::milliseconds timeout) {
        auto deadline = std::chrono::steady_clock::now() + timeout;
        bool all_success = true;
        
        for (auto& future : futures) {
            auto remaining = deadline - std::chrono::steady_clock::now();
            if (remaining <= std::chrono::milliseconds(0)) {
                std::cout << "[syncorder] Timeout waiting for completion\n";
                return false;
            }
            
            if (future.wait_for(remaining) == std::future_status::timeout) {
                std::cout << "[syncorder] Manager timeout\n";
                return false;
            }
            
            if constexpr (std::is_same_v<T, bool>) {
                try {
                    if (!future.get()) {
                        all_success = false;
                    }
                } catch (const std::exception& e) {
                    std::cout << "[syncorder] Future exception: " << e.what() << "\n";
                    all_success = false;
                }
            } else {
                try {
                    future.get();
                } catch (const std::exception& e) {
                    std::cout << "[syncorder] Future exception: " << e.what() << "\n";
                }
            }
        }
        
        return all_success;
    }
};