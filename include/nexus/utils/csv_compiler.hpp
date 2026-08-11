#pragma once

#include "nexus/core/hft_types.hpp"
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <string_view>
#include <filesystem>
#include <charconv>
#include <algorithm>
#include <limits>

namespace nexus::utils {

    using namespace nexus::core;
    namespace fs = std::filesystem;

    class CSVCompiler {
    public:
        CSVCompiler() noexcept = default;
        ~CSVCompiler() noexcept = default;
        CSVCompiler(const CSVCompiler&) = delete;
        CSVCompiler& operator=(const CSVCompiler&) = delete;

        // Processes and consolidates multi-asset text streams into a single chronological binary timeline
        static void compile_csv_directory(const std::vector<std::pair<std::string, std::string>>& symbol_files, const std::string& output_bin_path) {
            std::cout << "[+] Warming Engine starting multi-file consolidation mapping...\n";

            std::vector<FileStreamState> inputs;
            inputs.reserve(symbol_files.size());

            // 1. Warm-up and stage the initial line from each asset file
            for (const auto& [symbol_name, file_path] : symbol_files) {
                FileStreamState state;
                state.stream.open(file_path);
                if (!state.stream.is_open()) {
                    std::cerr << "[-] Warming Warning: Missing raw file component: " << file_path << "\n";
                    continue;
                }
                state.filename = file_path;
                state.symbol_hash = fnv1a_32(symbol_name);
                state.is_jpy = (symbol_name == "USDJPY");

                if (std::getline(state.stream, state.current_line)) {
                    if (parse_line_to_tick(state.current_line, state.symbol_hash, state.is_jpy, state.current_tick)) {
                        state.active = true;
                        inputs.push_back(std::move(state));
                    }
                }
            }

            // 2. Open the destination master timeline binary target
            std::ofstream out_bin(output_bin_path, std::ios::binary | std::ios::trunc);
            if (!out_bin.is_open()) {
                std::cerr << "[-] Fatal Error: Output stream path access failure.\n";
                return;
            }

            size_t consolidated_tick_count = 0;

            // 3. Multi-Way Merge Tournament Loop
            while (true) {
                size_t best_index = size_t(-1);
                uint64_t earliest_ts = (std::numeric_limits<uint64_t>::max)();

                for (size_t i = 0; i < inputs.size(); ++i) {
                    if (inputs[i].active && inputs[i].current_tick.timestamp_ns < earliest_ts) {
                        earliest_ts = inputs[i].current_tick.timestamp_ns;
                        best_index = i;
                    }
                }

                if (best_index == size_t(-1)) break;

                // 4. Stream-serialize the chronologically earliest tick out to the binary database
                auto& target = inputs[best_index];
                out_bin.write(reinterpret_cast<const char*>(&target.current_tick), sizeof(FXTick));
                consolidated_tick_count++;

                // 5. Refill the processed stream slot with its next sequential tick entry
                if (std::getline(target.stream, target.current_line)) {
                    if (!parse_line_to_tick(target.current_line, target.symbol_hash, target.is_jpy, target.current_tick)) {
                        target.active = false;
                    }
                }
                else {
                    target.active = false;
                }
            }

            out_bin.close();
            std::cout << "[+] Serialization complete. Generated: " << consolidated_tick_count << " rows in master binary timeline.\n";
        }

    private:
        struct FileStreamState {
            std::ifstream stream;
            std::string current_line;
            FXTick current_tick;
            uint32_t symbol_hash{ 0 };
            std::string filename;
            bool is_jpy{ false };
            bool active{ false };
        };

        // Transforms string slices into integer fixed-point price
        static inline int32_t parse_fixed_point(std::string_view sv, bool is_jpy) noexcept {
            size_t dot_pos = sv.find('.');
            int32_t integer_part = 0;
            int32_t fractional_part = 0;
            int32_t target_scale = is_jpy ? 1'000 : 1'000'000;
            int32_t decimals = is_jpy ? 3 : 6;

            if (dot_pos == std::string_view::npos) {
                std::from_chars(sv.data(), sv.data() + sv.size(), integer_part);
                return integer_part * target_scale;
            }

            std::from_chars(sv.data(), sv.data() + dot_pos, integer_part);

            std::string_view frac_sv = sv.substr(dot_pos + 1);
            std::from_chars(frac_sv.data(), frac_sv.data() + frac_sv.size(), fractional_part);

            size_t precision = frac_sv.size();
            int32_t multiplier = 1;
            for (size_t i = precision; i < static_cast<size_t>(decimals); ++i) multiplier *= 10;

            return (integer_part * target_scale) + (fractional_part * multiplier);
        }

        // Deconstructs TrueFX timestamp layouts directly into nanosecond integers
        static inline uint64_t parse_truefx_timestamp(std::string_view sv) noexcept {
            if (sv.size() < 18) return 0;
            int32_t year = 0, month = 0, day = 0, hour = 0, min = 0, sec = 0, ms = 0;

            std::from_chars(sv.data(), sv.data() + 4, year);
            std::from_chars(sv.data() + 4, sv.data() + 6, month);
            std::from_chars(sv.data() + 6, sv.data() + 8, day);
            std::from_chars(sv.data() + 9, sv.data() + 11, hour);
            std::from_chars(sv.data() + 11, sv.data() + 13, min);
            std::from_chars(sv.data() + 13, sv.data() + 15, sec);
            std::from_chars(sv.data() + 15, sv.data() + 18, ms);

            uint64_t total_seconds = (year - 1970) * 31'536'000ULL + (month * 2'592'000ULL) + (day * 86'400ULL);
            total_seconds += (hour * 3600ULL) + (min * 60ULL) + sec;

            return (total_seconds * 1'000'000'000ULL) + (ms * 1'000'000ULL);
        }

        static bool parse_line_to_tick(std::string_view line, uint32_t symbol_hash, bool is_jpy, FXTick& out_tick) noexcept {
            size_t c1 = line.find(',');
            if (c1 == std::string_view::npos) return false;
            size_t c2 = line.find(',', c1 + 1);
            if (c2 == std::string_view::npos) return false;
            size_t c3 = line.find(',', c2 + 1);
            if (c3 == std::string_view::npos) return false;

            std::string_view ts_sv = line.substr(0, c1);
            std::string_view bid_sv = line.substr(c1 + 1, c2 - (c1 + 1));
            std::string_view ask_sv = line.substr(c2 + 1, c3 - (c2 + 1));
            std::string_view vol_sv = line.substr(c3 + 1);

            out_tick.timestamp_ns = parse_truefx_timestamp(ts_sv);
            out_tick.symbol_hash = symbol_hash;
            out_tick.bid_price = parse_fixed_point(bid_sv, is_jpy);
            out_tick.ask_price = parse_fixed_point(ask_sv, is_jpy);

            uint32_t raw_vol = 0;
            std::from_chars(vol_sv.data(), vol_sv.data() + vol_sv.size(), raw_vol);

            out_tick.bid_volume = (raw_vol == 0) ? 10000U : raw_vol;
            out_tick.ask_volume = (raw_vol == 0) ? 10000U : raw_vol;
            out_tick.padding = 0;

            return true;
        }
    };

}