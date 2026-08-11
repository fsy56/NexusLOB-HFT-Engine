#pragma once

#include <cstdint>
#include <cstddef>
#include <concepts>
#include <new>
#include <array>
#include <utility>
#include <string_view>

namespace nexus::core {

	//parameters for x^$ target environment
	constexpr std::size_t CACHE_LINE_SIZE = 64;

	enum class Side :uint8_t {
		BUY=0,
		SELL=1
	};

	// Aligned to 32B to pack two FXTick per cache line
	struct alignas(32) FXTick {
		uint64_t timestamp_ns;
		uint32_t symbol_hash;  // (EURUSD,...)
		int32_t  bid_price;
		int32_t  ask_price;
		uint32_t bid_volume;
		uint32_t ask_volume;
		uint32_t padding;      // Explicit to complete the 32-byte block
	};

	// Aligned to 64B to pack two FXTick per cache line
	struct alignas(CACHE_LINE_SIZE) Order {
		uint64_t order_id;
		uint64_t timestamp_ns;
		uint32_t symbol_hash;
		int32_t  price;
		uint32_t total_qty;
		uint32_t open_qty;
		Side     side;
		char     padding[27]; // Explicit to complete the 64 bytes
	};

	static_assert(sizeof(FXTick) == 32, "FXTick structure size must be exactly 32 bytes.");
	static_assert(alignof(FXTick) == 32, "FXTick structure alignment must be 32 bytes.");
	static_assert(sizeof(Order) == CACHE_LINE_SIZE, "Order must match architectural cache line size.");
	static_assert(alignof(Order) == CACHE_LINE_SIZE, "Order alignment must match architectural cache line size.");

	// Compile-time FNV-1a lookup token generator
	constexpr uint32_t fnv1a_32(std::string_view str) noexcept {
		uint32_t hash = 0x811C9DC5;
		for (char c : str) {
			hash ^= static_cast<uint8_t>(c);
			hash *= 0x01000193;
		}
		return hash;
	}

	//Object Pool
	template <typename T, std::size_t Capacity>
	requires (alignof(T) >= alignof(void*))
	class FixedMemoryPool {
	public:
		FixedMemoryPool() noexcept {
			// Splice our internal uninitialized storage blocks into a linked free list chain
			for (std::size_t i{ 0 };i < Capacity - 1;++i) {
				auto* current_node = reinterpret_cast<Node*>(&m_storage[i]);
				auto* next_node = reinterpret_cast<Node*>(&m_storage[i+1]);
				current_node->next_free = next_node;
			}
			reinterpret_cast<Node*>(&m_storage[Capacity - 1])->next_free = nullptr;
			m_free_head = reinterpret_cast<Node*>(&m_storage);
		}

		~FixedMemoryPool() = default;

		// Delete copy and move semantics to preserve locality
		FixedMemoryPool(const FixedMemoryPool&) = delete;
		FixedMemoryPool& operator=(const FixedMemoryPool&) = delete;
		FixedMemoryPool(FixedMemoryPool&&) noexcept = delete;
		FixedMemoryPool& operator=(FixedMemoryPool&&) noexcept = delete;

		template <typename... Args>
		[[nodiscard]] inline T* allocate(Args&&... args) noexcept {
			if (m_free_head == nullptr) [[unlikely]] {
				return nullptr;
			}

			Node* node = m_free_head;
			m_free_head = m_free_head->next_free;
			return ::new (static_cast<void*>(node)) T(std::forward<Args>(args)...);
		}


		inline void deallocate(T* ptr) noexcept {
			if (ptr == nullptr) [[unlikely]] return;

			ptr->~T(); // Manually invoke destructor without triggering standard runtime heap APIs
			auto* node = reinterpret_cast<Node*>(ptr);
			node->next_free = m_free_head;
			m_free_head = node;
		}

		[[nodiscard]] constexpr std::size_t capacity() const noexcept { return Capacity; }

	private:
		union Node {
			Node* next_free;
			alignas(T) char storage_space[sizeof(T)];
		};

		alignas(CACHE_LINE_SIZE) std::array<typename std::aligned_storage<sizeof(T), alignof(T)>::type, Capacity> m_storage;
		Node* m_free_head{ nullptr };
	};
}