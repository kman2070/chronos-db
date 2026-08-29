#include <iostream>
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>
#include <cassert>
#include <csignal>
#include <unistd.h>
#include <sys/wait.h>

extern "C" {
    #include "chronos.h"
    #include "ares_bft.h"
}

std::atomic<bool> g_stop_signal{false};
constexpr int NUM_THREADS = 8;

void run_worker(ChronosContext* ctx, int thread_id) {
    uint64_t counter = 0;
    while (!g_stop_signal.load()) {
        std::string payload = "thread_" + std::to_string(thread_id) + "_tx_" + std::to_string(counter);
        
        // Appends to Chronos log with context, module ID, severity, payload pointer, and payload length
        chronos_append(ctx, (uint32_t)thread_id, 1, (const uint8_t*)payload.c_str(), payload.size());
        
        counter++;
    }
}

void execute_child_workload(ChronosContext* ctx) {
    std::vector<std::thread> threads;
    threads.reserve(NUM_THREADS);

    for (int i = 0; i < NUM_THREADS; ++i) {
        threads.emplace_back(run_worker, ctx, i);
    }

    for (auto& t : threads) {
        if (t.joinable()) t.join();
    }
}

void run_test_controller() {
    std::cout << "[+] Spawning ChronosDB engine in isolated child process...\n";
    pid_t pid = fork();

    if (pid == 0) {
        // --- CHILD PROCESS ---
        ChronosContext ctx = {}; 
        
        execute_child_workload(&ctx);
        
        exit(0);
    } else if (pid > 0) {
        // --- PARENT PROCESS ---
        std::cout << "[+] Stressing engine for 3 seconds under multi-threaded writes...\n";
        std::this_thread::sleep_for(std::chrono::seconds(3));

        std::cout << "[!] Injecting SIGKILL (simulating hard power failure mid-write)...\n";
        kill(pid, SIGKILL);

        int status;
        waitpid(pid, &status, 0);
        std::cout << "[+] Child process killed abruptly.\n\n";

        std::cout << "[+] Starting Post-Crash Verification Suite...\n";
        
        ChronosContext recovery_ctx = {};
        bool integrity_ok = chronos_verify_integrity(&recovery_ctx);
        
        if (integrity_ok) {
            std::cout << "--------------------------------------------\n";
            std::cout << "[SUCCESS] ChronosDB log & SHA256 integrity intact after crash!\n";
            std::cout << "--------------------------------------------\n";
        } else {
            std::cerr << "--------------------------------------------\n";
            std::cerr << "[FAIL] Data corruption or broken hash chain detected!\n";
            std::cerr << "--------------------------------------------\n";
            assert(false && "ChronosDB recovery integrity test failed!");
        }
    } else {
        std::cerr << "Fork failed!\n";
    }
}

int main() {
    run_test_controller();
    return 0;
}
