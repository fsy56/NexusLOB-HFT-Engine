#pragma once

#include <cstdint>

#ifdef _MSC_VER
#include <intrin.h> // Required for MSVC hardware intrinsic mappings
#else
#include <x86intrin.h>
#endif

namespace nexus::utils {
    [[nodiscard]] inline uint64_t tsc_now() noexcept {
#ifdef _MSC_VER
        unsigned int dummy_cpu_id;
        return __rdtscp(&dummy_cpu_id);
#else
        unsigned int dummy_cpu_id;
        return __rdtscp(&dummy_cpu_id);
#endif
    }

    [[nodiscard]] inline double cycles_to_nanoseconds(uint64_t cycles, double tsc_frequency_ghz) noexcept {
        return static_cast<double>(cycles) / tsc_frequency_ghz;
    }

}
