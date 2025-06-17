#pragma once

#include <deque>
#include <mutex>
#include <condition_variable>

// Forward declaration to avoid circular dependency
struct message;

class TsQueue
{
public:
    TsQueue() {}

    message pop_front();
    void push_back(message msg);
    bool empty();

private:
    std::mutex m_mux;
    std::condition_variable cv;
    std::deque<message> m_deque;
};

// Include message.h after the class declaration
#include "message.h"

// Implement methods inline after including message.h
inline message TsQueue::pop_front()
{
    std::unique_lock<std::mutex> lock(m_mux);
    cv.wait(lock, [this]
            { return !m_deque.empty(); });
    message temp_msg = std::move(m_deque.front());
    m_deque.pop_front();
    return temp_msg;
}

inline void TsQueue::push_back(message msg)
{
    {
        std::scoped_lock lock(m_mux);
        m_deque.push_back(std::move(msg));
    }
    cv.notify_one();
}

inline bool TsQueue::empty()
{
    std::scoped_lock lock(m_mux);
    return m_deque.empty();
}