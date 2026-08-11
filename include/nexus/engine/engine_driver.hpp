#pragma once

#include "nexus/core/hft_types.hpp"
#include "nexus/core/ring_buffer.hpp"
#include "nexus/utils/time_utils.hpp"
#include "nexus/utils/binary_loader.hpp"

#include <string>
#include <thread>
#include <atomic>
#include <iostream>
#include <memory>

#if defined(_MSC_VER)
#include <immintrin.h> 
#define NEXUS_SPIN_HINT() _mm_pause() // Directly invokes x64 PAUSE to optimize core instruction port sharing
#else
#define NEXUS_SPIN_HINT() std::this_thread::hint_spin_loop()
#endif

namespace nexus::engine {

    using namespace nexus::core;
    using namespace nexus::utils;

    template <std::size_t QueueCapacity = 131072>
    class EngineDriver {
    public:
        // Allocates the heavy SPSC buffer via std::make_unique to shield the 1MB Windows stack boundary
        explicit EngineDriver(std::string binary_path) noexcept
            : m_binary_path(std::move(binary_path)), m_ingestion_complete(false) {
            m_queue = std::make_unique<SPSCRingBuffer<FXTick, QueueCapacity>>();
        }

        ~EngineDriver() noexcept {
            stop();
        }

        EngineDriver(const EngineDriver&) = delete;
        EngineDriver& operator=(const EngineDriver&) = delete;

        bool start() noexcept {
            if (!m_loader.load(m_binary_path)) {
                std::cerr << "[-] Driver Error: Unable to map file payload: " << m_binary_path << "\n";
                return false;
            }

            m_ingestion_complete.store(false, std::memory_order::memory_order_relaxed);

            m_consumer_thread = std::thread(&EngineDriver::run_backtest_loop, this);
            m_producer_thread = std::thread(&EngineDriver::run_ingestion_loop, this);

            return true;
        }

        void stop() noexcept {
            if (m_producer_thread.joinable()) m_producer_thread.join();
            if (m_consumer_thread.joinable()) m_consumer_thread.join();
        }

        [[nodiscard]] inline size_t processed_ticks() const noexcept { return m_processed_ticks; }
        [[nodiscard]] inline double execution_seconds() const noexcept { return m_execution_seconds; }
        [[nodiscard]] inline double million_tps() const noexcept {
            return (m_execution_seconds > 0.0) ? ((m_processed_ticks / m_execution_seconds) / 1'000'000.0) : 0.0;
        }
        [[nodiscard]] inline size_t total_strategy_hits() const noexcept { return m_strategy_hits; }
        [[nodiscard]] inline double total_strategy_pnl() const noexcept { return m_strategy_pnl; }

    private:
        // INGESTION PROCESS (Producer Path): Direct streaming writes from the zero-copy file projection
        void run_ingestion_loop() noexcept {
            const size_t total_ticks = m_loader.tick_count();
            const FXTick* mapped_ticks = m_loader.data();

            for (size_t i = 0; i < total_ticks; ++i) {
                while (!m_queue->emplace(mapped_ticks[i])) {
                    NEXUS_SPIN_HINT();
                }
            }
            m_ingestion_complete.store(true, std::memory_order::memory_order_release);
        }

        void run_backtest_loop() noexcept {
            FXTick tick_buffer;
            uint64_t start_cycles = tsc_now();

            // Institutional Threshold parameters (Scaled by raw entry point definitions)
            constexpr int32_t SPREAD_TRIGGER_STANDARD = 150; // 1.5 Pips for 5/6 decimal pairs
            constexpr int32_t SPREAD_TRIGGER_JPY = 15;  // 1.5 Pips for 2/3 decimal JPY pairs
            constexpr uint32_t LOT_SIZE = 10000U;

            while (true) {
                if (m_queue->pop(tick_buffer)) {
                    m_processed_ticks++;

                    int32_t market_spread = tick_buffer.ask_price - tick_buffer.bid_price;
                    uint32_t sym = tick_buffer.symbol_hash;

                    // Branchless/O(1) multi-asset precision normalization matching loop
                    if (sym == m_usdjpy_hash) {
                        if (market_spread > SPREAD_TRIGGER_JPY) [[unlikely]] {
                            m_strategy_hits += 2;
                            // JPY pairs are scaled by 1,000 in TrueFX binary packaging maps
                            double captured_pips = static_cast<double>(market_spread) / 1'000.0;
                            m_strategy_pnl += (captured_pips * LOT_SIZE);
                        }
                    }
                    else if (sym == m_eurusd_hash || sym == m_audusd_hash || sym == m_gbpusd_hash || sym == m_usdchf_hash) {
                        if (market_spread > SPREAD_TRIGGER_STANDARD) [[unlikely]] {
                            m_strategy_hits += 2;
                            // Standard pairs are scaled by 1,000,000
                            double captured_pips = static_cast<double>(market_spread) / 1'000'000.0;
                            m_strategy_pnl += (captured_pips * LOT_SIZE);
                        }
                    }
                }
                else {
                    if (m_ingestion_complete.load(std::memory_order::memory_order_acquire) && m_queue->size() == 0) {
                        break;
                    }
                    NEXUS_SPIN_HINT();
                }
            }

            uint64_t end_cycles = tsc_now();
            constexpr double ESTIMATED_CPU_FREQUENCY = 3.5;
            m_execution_seconds = cycles_to_nanoseconds(end_cycles - start_cycles, ESTIMATED_CPU_FREQUENCY) / 1'000'000'000.0;
        }

        std::string m_binary_path;
        BinaryLoader m_loader;
        std::atomic<bool> m_ingestion_complete;

        std::unique_ptr<SPSCRingBuffer<FXTick, QueueCapacity>> m_queue;

        std::thread m_producer_thread;
        std::thread m_consumer_thread;

        // Compile-time static hash lookups
        const uint32_t m_eurusd_hash{ fnv1a_32("EURUSD") };
        const uint32_t m_audusd_hash{ fnv1a_32("AUDUSD") };
        const uint32_t m_gbpusd_hash{ fnv1a_32("GBPUSD") };
        const uint32_t m_usdchf_hash{ fnv1a_32("USDCHF") };
        const uint32_t m_usdjpy_hash{ fnv1a_32("USDJPY") };

        size_t m_processed_ticks{ 0 };
        double m_execution_seconds{ 0.0 };

        size_t  m_strategy_hits{ 0 };
        double  m_strategy_pnl{ 0.0 };
    };

}