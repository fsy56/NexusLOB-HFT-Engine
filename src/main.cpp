#include "nexus/engine/engine_driver.hpp"
#include "nexus/utils/time_utils.hpp"
#include "nexus/utils/csv_compiler.hpp"
#include <iostream>
#include <memory>
#include <filesystem>
#include <iomanip>

using namespace nexus::core;
using namespace nexus::engine;
using namespace nexus::utils;
namespace fs = std::filesystem;

int main() {
    std::cout << "=========================================================\n";
    std::cout << "          NEXUSLOB HFT FRAMEWORK SUBSYSTEM SETUP\n";
    std::cout << "=========================================================\n";

    std::string bin_path = "../../../../data/nexus_market_data.bin";

    // --- AUTOMATED WARMING DISPATCHER CHECK ---
    if (!fs::exists(bin_path)) {
        std::cout << "[+] Master binary missing. Starting self-healing warming phase...\n";

        std::vector<std::pair<std::string, std::string>> historical_csv_files = {
            {"EURUSD", "../../../../data/DAT_ASCII_EURUSD_T_202601.csv"},
            {"AUDUSD", "../../../../data/DAT_ASCII_AUDUSD_T_202601.csv"},
            {"GBPUSD", "../../../../data/DAT_ASCII_GBPUSD_T_202601.csv"},
            {"USDCHF", "../../../../data/DAT_ASCII_USDCHF_T_202601.csv"},
            {"USDJPY", "../../../../data/DAT_ASCII_USDJPY_T_202601.csv"}
        };

        uint64_t warm_start = tsc_now();
        CSVCompiler::compile_csv_directory(historical_csv_files, bin_path);
        uint64_t warm_end = tsc_now();

        double warm_ms = cycles_to_nanoseconds(warm_end - warm_start, 3.5) / 1'000'000.0;
        std::cout << "[+] Warming Compilation Duration: " << warm_ms << " ms\n";
        std::cout << "---------------------------------------------------------\n";
    }
    else {
        std::cout << "[+] Valid multi-asset cached binary detected. Bypassing warming step.\n";
        std::cout << "---------------------------------------------------------\n";
    }

    // --- ZERO-COPY SYSTEM ENGINE INITIALIZATION ---
    uint64_t map_start = tsc_now();
    auto framework_driver = std::make_unique<EngineDriver<131072>>(bin_path);
    uint64_t map_end = tsc_now();

    constexpr double CPU_SPEED_GHZ = 3.5;
    double map_us = cycles_to_nanoseconds(map_end - map_start, CPU_SPEED_GHZ) / 1000.0;

    std::cout << "[+] OS Virtual Subsystem Bound Successfully.\n";
    std::cout << "[+] Mmap Memory Projection Latency : " << std::fixed << std::setprecision(3) << map_us << " microseconds\n";
    std::cout << "---------------------------------------------------------\n";
    std::cout << "[+] Processing pipeline active. Launching Thread Execution Arrays...\n";

    uint64_t run_start = tsc_now();
    if (!framework_driver->start()) {
        std::cerr << "[-] Critical Error: Execution driver rejected pipeline launch.\n";
        return 1;
    }

    framework_driver->stop();
    uint64_t run_end = tsc_now();

    // --- RENDER PRODUCTION DASHBOARD METRICS ---
    uint64_t total_cycles = run_end - run_start;
    size_t processed_ticks = framework_driver->processed_ticks();
    double elapsed_sec = framework_driver->execution_seconds();
    double million_tps = framework_driver->million_tps();

    double avg_cycles_per_tick = static_cast<double>(total_cycles) / static_cast<double>(processed_ticks);
    double avg_ns_per_tick = cycles_to_nanoseconds(total_cycles, CPU_SPEED_GHZ) / static_cast<double>(processed_ticks);

    std::cout << "---------------------------------------------------------\n";
    std::cout << "                  EXECUTIVE SIMULATION RUN METRICS       \n";
    std::cout << "---------------------------------------------------------\n";
    std::cout << "[+] Total Multi-Asset Ticks Processed : " << processed_ticks << " records\n";
    std::cout << "[+] Pure Matching Loop Duration       : " << elapsed_sec << " seconds\n";
    std::cout << "[+] System Engine Throughput Speed    : " << million_tps << " Million TPS\n";
    std::cout << "---------------------------------------------------------\n";
    std::cout << "                  QUANTITATIVE BACKTEST STRATEGY LOGS    \n";
    std::cout << "---------------------------------------------------------\n";
    std::cout << "[+] Arbitrage Spread Strategy Fills   : " << framework_driver->total_strategy_hits() << " hits\n";
    std::cout << "[+] Strategy Net Closed Profit & Loss : " << std::setprecision(2) << framework_driver->total_strategy_pnl() << " USD\n";
    std::cout << "---------------------------------------------------------\n";
    std::cout << "                  HARDWARE EFFICIENCY SIGNATURES         \n";
    std::cout << "---------------------------------------------------------\n";
    std::cout << "[+] CPU Clock Cycles Consumed (Total) : " << total_cycles << " cycles\n";
    std::cout << "[+] CPU Efficiency Budget Per Tick    : " << std::setprecision(1) << avg_cycles_per_tick << " cycles/tick\n";
    std::cout << "[+] Inlined Execution Path Latency    : " << std::setprecision(2) << avg_ns_per_tick << " nanoseconds/path\n";

    if (avg_ns_per_tick < 100.0) {
        std::cout << "[+] SLA PERFORMANCE TARGET            : PASSED (Sub-100ns Deterministic Matrix)\n";
    }
    else {
        std::cout << "[-] FAILED: Latency exceeded 100ns SLA boundary.\n";
    }
    std::cout << "=========================================================\n";

    return 0;
}