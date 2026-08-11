#pragma once

#include <algorithm>
#include <cstdint>
#include <cstddef>
#include <array>

#include "nexus/core/hft_types.hpp"

namespace nexus::engine {
	using namespace nexus::core;

	constexpr std::size_t MAX_ORDERS_PER_LEVEL = 512;
	constexpr std::size_t MAX_BOOK_DEPTH = 32;

	// Cache-aligned contiguous price level data block
	struct alignas(CACHE_LINE_SIZE) BookLevel {
		int32_t price{ 0 };
		uint32_t total_volume{ 0 };
		uint32_t order_count{ 0 };

		std::array<Order*, MAX_ORDERS_PER_LEVEL> orders_queue{ };

		inline bool append_order(Order* order) noexcept {
			if (order_count >= MAX_ORDERS_PER_LEVEL) [[unlikely]] {
				return false;
			}
			orders_queue[order_count++] = order;
			total_volume += order->open_qty;
			return true;
		}

		inline void reset() noexcept {
			price = 0;
			total_volume = 0;
			order_count = 0;
			orders_queue.fill(nullptr);
		}
	};

	template <Side BookSide>
	class OrderBookSide {
	public:
		OrderBookSide() noexcept : m_active_levels(0) {}
		~OrderBookSide() noexcept = default;

		OrderBookSide(const OrderBookSide&) = delete;
		OrderBookSide& operator=(const OrderBookSide&) = delete;

		//Inserts an order into the appropriate price slot maintaining priority.
		inline bool insert_order(Order* order) noexcept {
			for (std::size_t i = 0;i < m_active_levels;++i) {
				if (m_levels[i].price == order->price) {
					return m_levels[i].append_order(order);
				}
			}

			if (m_active_levels >= MAX_BOOK_DEPTH) [[unlikely]] {
				return false;
			}

			BookLevel& new_level = m_levels[m_active_levels++];
			new_level.reset();
			new_level.price = order->price;

			bool success = new_level.append_order(order);
			sort_levels();
			return success;
		}

		[[nodiscard]] inline std::size_t active_levels() const noexcept { return m_active_levels; }
		[[nodiscard]] inline BookLevel* levels() noexcept { return m_levels.data(); }
		[[nodiscard]] inline const BookLevel* levels() const noexcept { return m_levels.data(); }
		inline void set_active_levels(std::size_t count) noexcept { m_active_levels = count; }

	private:
		// Sorts active levels
		inline void sort_levels() noexcept {
			if (m_active_levels < 2) return;

			std::sort(m_levels.begin(), m_levels.begin() + m_active_levels, [](const BookLevel& a, const BookLevel& b) {
				if constexpr (BookSide == Side::BUY) {
					return a.price > b.price; // Highest bid price has matching priority
				}
				else {
					return a.price < b.price; // Lowest ask price has matching priority
				}
				});
		}

		alignas(CACHE_LINE_SIZE) std::array<BookLevel, MAX_BOOK_DEPTH> m_levels;
		std::size_t m_active_levels{ 0 };
	};
}