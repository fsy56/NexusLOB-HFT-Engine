#pragma once

#include "nexus/core/hft_types.hpp"
#include <iostream>
#include <string>
#include <string_view>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace nexus::utils {

    using namespace nexus::core;

    class BinaryLoader {
    public:
        BinaryLoader() noexcept = default;
        ~BinaryLoader() noexcept { unload(); }

        BinaryLoader(const BinaryLoader&) = delete;
        BinaryLoader& operator=(const BinaryLoader&) = delete;

        // Maps the binary payload straight into the virtual address space via native Win32 handles
        bool load(const std::string& bin_path) noexcept {
            unload(); // Clear any pre-existing mapped allocations

#ifdef _WIN32
            // 1. Establish low-overhead Windows native file handle
            m_file_handle = CreateFileA(bin_path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
            if (m_file_handle == INVALID_HANDLE_VALUE) return false;

            // 2. Measure total file dimensions for budget calculations
            LARGE_INTEGER file_size;
            if (!GetFileSizeEx(m_file_handle, &file_size)) { close_handles(); return false; }
            m_total_bytes = static_cast<size_t>(file_size.QuadPart);
            if (m_total_bytes == 0) { close_handles(); return false; }

            // 3. Create a kernel page-backed file-mapping object
            m_mapping_handle = CreateFileMappingA(m_file_handle, nullptr, PAGE_READONLY, 0, 0, nullptr);
            if (m_mapping_handle == nullptr) { close_handles(); return false; }

            // 4. Project the file bits directly into the process's address segment
            void* view = MapViewOfFile(m_mapping_handle, FILE_MAP_READ, 0, 0, 0);
            if (view == nullptr) { close_handles(); return false; }

            m_data_ptr = static_cast<FXTick*>(view);
#else
            // 1. Establish low-overhead file descriptor
            m_fd = ::open(bin_path.c_str(), O_RDONLY);
            if (m_fd == -1) return false;

            // 2. Measure file size using kernel stat models
            struct stat sb;
            if (::fstat(m_fd, &sb) == -1) { close_handles(); return false; }
            m_total_bytes = static_cast<size_t>(sb.st_size);
            if (m_total_bytes == 0) { close_handles(); return false; }

            // 3. Project the file bits directly into the process memory segment using mmap
            void* view = ::mmap(nullptr, m_total_bytes, PROT_READ, MAP_SHARED, m_fd, 0);
            if (view == MAP_FAILED) { close_handles(); return false; }

            m_data_ptr = static_cast<FXTick*>(view);
#endif
            m_tick_count = m_total_bytes / sizeof(FXTick);
            return true;
        }

        void unload() noexcept {
            if (m_data_ptr != nullptr) {
#ifdef _WIN32
                UnmapViewOfFile(m_data_ptr);
#else
                ::munmap(m_data_ptr, m_total_bytes);
#endif
                m_data_ptr = nullptr;
            }
            close_handles();
            m_total_bytes = 0;
            m_tick_count = 0;
        }

        [[nodiscard]] inline FXTick* data() noexcept { return m_data_ptr; }
        [[nodiscard]] inline const FXTick* data() const noexcept { return m_data_ptr; }
        [[nodiscard]] inline size_t tick_count() const noexcept { return m_tick_count; }

    private:
        void close_handles() noexcept {
#ifdef _WIN32
            if (m_mapping_handle != nullptr) { CloseHandle(m_mapping_handle); m_mapping_handle = nullptr; }
            if (m_file_handle != INVALID_HANDLE_VALUE) { CloseHandle(m_file_handle); m_file_handle = INVALID_HANDLE_VALUE; }
#else
            if (m_fd != -1) { ::close(m_fd); m_fd = -1; }
#endif
        }

        FXTick* m_data_ptr{ nullptr };
        size_t  m_total_bytes{ 0 };
        size_t  m_tick_count{ 0 };
#ifdef _WIN32
        HANDLE  m_file_handle{ INVALID_HANDLE_VALUE };
        HANDLE  m_mapping_handle{ nullptr };
#else
        int     m_fd{ -1 };// Linux native File Descriptor
#endif
    };

} // namespace nexus::utils
