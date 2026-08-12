#pragma once

#include "nexus/core/hft_types.hpp"
#include <atomic>
#include <array>
#include <cstddef>
#include <new>

namespace nexus::core {
	
	//SPSC pattern
	template <typename T, std::size_t Capacity>
	class SPSCRingBuffer {
		static_assert((Capacity& (Capacity - 1)) == 0, "Ring buffer capacity must be a power of 2.");
	public:
		SPSCRingBuffer() noexcept :m_head(0), m_tail(0) {}
		~SPSCRingBuffer() noexcept = default;
		SPSCRingBuffer(const SPSCRingBuffer&) = delete;
		SPSCRingBuffer& operator=(const SPSCRingBuffer&) = delete;

		template <typename... Args>
		inline bool emplace(Args&&... args) noexcept {
			const std::size_t current_tail = m_tail.load(std::memory_order_relaxed);
			const std::size_t current_head = m_head.load(std::memory_order_acquire);

			if ((current_tail - current_head) == Capacity) {
				return false;
			}
			
			// Calculate the index thanks to the mask
			std::size_t index = current_tail & BUFFER_MASK;
			::new (static_cast<void*>(&m_buffer[index].storage)) T(std::forward<Args>(args)...);
			
			m_tail.store(current_tail + 1, std::memory_order_release);
			return true;
		}

		inline bool pop(T& out_item) noexcept {
			const std::size_t current_head = m_head.load(std::memory_order_relaxed);
			const std::size_t current_tail = m_tail.load(std::memory_order_acquire);

			if (current_head == current_tail) {
				return false;
			}

			std::size_t index = current_head & BUFFER_MASK;
			auto* item_ptr = reinterpret_cast<T*>(&m_buffer[index].storage);
			out_item = std::move(*item_ptr);
			item_ptr->~T(); // Manually invoke destructor without triggering standard runtime heap APIs

			m_head.store(current_head + 1, std::memory_order_release);
			return true;
		}

		[[nodiscard]] inline std::size_t size() const noexcept {
			std::size_t tail = m_tail.load(std::memory_order_relaxed);
			std::size_t head = m_head.load(std::memory_order_relaxed);
			return (tail >= head) ? (tail - head) : 0;
		}

	private:
		struct Node {
			alignas(alignof(T)) std::byte storage[sizeof(T)];
		};

		static constexpr std::size_t BUFFER_MASK = Capacity - 1;

		alignas(CACHE_LINE_SIZE) std::atomic<std::size_t> m_head;
		alignas(CACHE_LINE_SIZE) std::atomic<std::size_t> m_tail;
		alignas(CACHE_LINE_SIZE) std::array<Node, Capacity> m_buffer;
	};
}