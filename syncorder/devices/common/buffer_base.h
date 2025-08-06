#pragma once

#include <chrono>
#include <array>
#include <atomic>
#include <optional>
#include <iostream>


template <typename T, std::size_t N>
class BBuffer {
protected:
    std::atomic<std::size_t> m_head;
    std::array<T, N> m_buff;
    std::atomic<std::size_t> m_tail;

    std::atomic<bool> gate_{true};

public:
    constexpr BBuffer() noexcept
    : 
        m_head(0), 
        m_tail(0),
        gate_(true)
    {}
    virtual ~BBuffer() = default;

public:
    bool enqueue(T val) noexcept {
        // gate
        if (gate_.load(std::memory_order_acquire)) return false;

        // run
        std::size_t current_tail = m_tail.load(std::memory_order_relaxed);
        if (current_tail - m_head.load(std::memory_order_acquire) < N) {
            m_buff[current_tail % N] = std::move(val);
            m_tail.store(current_tail + 1, std::memory_order_release);
            return true;
        }
        
        onOverflow();
        return false;
    }
    
    std::optional<T> _dequeue() noexcept {
        std::size_t current_tail = m_tail.load(std::memory_order_acquire);
        std::size_t current_head = m_head.load(std::memory_order_relaxed);
        
        if (current_head != current_tail) {
            auto value = std::move(m_buff[current_head % N]);
            m_head.store(current_head + 1, std::memory_order_release);
            return std::optional<T>(std::move(value));
        }
        
        return std::nullopt;
    }

    void start() {
        gate_.store(false, std::memory_order_release);
    }

    void stop() {
        gate_.store(true, std::memory_order_release);
    }
    
    std::size_t size() const noexcept {
        return m_tail.load(std::memory_order_acquire) - m_head.load(std::memory_order_acquire);
    }

protected:
    virtual void onOverflow() noexcept = 0;
};