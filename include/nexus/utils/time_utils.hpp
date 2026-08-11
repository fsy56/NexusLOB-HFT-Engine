#pragma once

#include <cstdint>
#include <intrin.h> // Required for MSVC hardware intrinsic mappings

namespace nexus::utils {
    [[nodiscard]] inline uint64_t tsc_now() noexcept {
        unsigned int dummy_cpu_id;
        return __rdtscp(&dummy_cpu_id);
    }

    [[nodiscard]] inline double cycles_to_nanoseconds(uint64_t cycles, double tsc_frequency_ghz) noexcept {
        return static_cast<double>(cycles) / tsc_frequency_ghz;
    }

}
