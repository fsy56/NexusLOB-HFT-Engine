#pragma once

#include <cstdint>
#include <iostream>
#include <algorithm> 

#include "nexus/core/hft_types.hpp"
#include "nexus/engine/order_book.hpp"

namespace nexus::engine {

    using namespace nexus::core;

    class MatchingEngine {
    public:
        MatchingEngine() noexcept = default;
        ~MatchingEngine() noexcept = default;

        MatchingEngine(const MatchingEngine&) = delete;
        MatchingEngine& operator=(const MatchingEngine&) = delete;

        inline void match_order(Order* incoming_order, size_t* out_fills = nullptr, int64_t* out_fill_price_sum = nullptr) noexcept {
            if (incoming_order == nullptr) [[unlikely]] return;

            m_current_fills = 0;
            m_current_fill_price_sum = 0;

            if (incoming_order->side == Side::BUY) {
                match_against_side(incoming_order, m_asks);
                if (incoming_order->open_qty > 0) {
                    m_bids.insert_order(incoming_order);
                }
            }
            else {
                match_against_side(incoming_order, m_bids);
                if (incoming_order->open_qty > 0) {
                    m_asks.insert_order(incoming_order);
                }
            }

            if (out_fills) *out_fills = m_current_fills;
            if (out_fill_price_sum) *out_fill_price_sum = m_current_fill_price_sum;
        }

        [[nodiscard]] inline OrderBookSide<Side::BUY>& bids() noexcept { return m_bids; }
        [[nodiscard]] inline OrderBookSide<Side::SELL>& asks() noexcept { return m_asks; }

    private:
        template <Side ContraSide>
        inline void match_against_side(Order* incoming, OrderBookSide<ContraSide>& contra_book) noexcept {
            std::size_t active_levels = contra_book.active_levels();
            BookLevel* levels = contra_book.levels();

            std::size_t raw_level_idx = 0;
            while (raw_level_idx < active_levels && incoming->open_qty > 0) {
                BookLevel& level = levels[raw_level_idx];

                if constexpr (ContraSide == Side::SELL) {
                    if (incoming->price < level.price) break;
                }
                else {
                    if (incoming->price > level.price) break;
                }

                std::size_t order_idx = 0;
                while (order_idx < level.order_count && incoming->open_qty > 0) {
                    Order* resting = level.orders_queue[order_idx];

                    if (resting->open_qty > 0) {
                        uint32_t match_qty = (std::min)(incoming->open_qty, resting->open_qty);

                        incoming->open_qty -= match_qty;
                        resting->open_qty -= match_qty;
                        level.total_volume -= match_qty;

                        // Capture fill details if it belongs to our tracking strategy stream
                        m_current_fills++;
                        m_current_fill_price_sum += (static_cast<int64_t>(level.price) * match_qty);
                    }

                    if (resting->open_qty == 0) {
                        order_idx++;
                    }
                }

                if (order_idx == level.order_count && level.total_volume == 0) {
                    level.reset();
                    for (std::size_t shift = raw_level_idx; shift < active_levels - 1; ++shift) {
                        levels[shift] = std::move(levels[shift + 1]);
                    }
                    active_levels--;
                    levels[active_levels].reset();
                }
                else {
                    if (order_idx > 0) {
                        std::size_t remaining_orders = level.order_count - order_idx;
                        for (std::size_t i = 0; i < remaining_orders; ++i) {
                            level.orders_queue[i] = level.orders_queue[order_idx + i];
                        }
                        level.order_count = static_cast<uint32_t>(remaining_orders);
                    }
                    raw_level_idx++;
                }
            }

            contra_book.set_active_levels(active_levels);
        }

        OrderBookSide<Side::BUY>  m_bids;
        OrderBookSide<Side::SELL> m_asks;

        // Hot-path scalar caches to bypass thread return tuple allocations
        size_t  m_current_fills{ 0 };
        int64_t m_current_fill_price_sum{ 0 };
    };

}