#pragma once

#include "spdlog/spdlog.h"
#include <string>
#include <sys/epoll.h>
#include <string.h>

#include <source_location>
#include <array>
#include <span>


inline int check_error(int result, const std::source_location location = std::source_location::current()) {
    if (result >= 0) {
        return result;
    }

    spdlog::error("{}. In \"{}\":{} ({})", strerror(errno), location.file_name(), location.line(), location.function_name());

    return result;
}

struct Epoll {
    constexpr static int s_max_events = 100;
public:

    Epoll(int descriptor): m_descriptor(descriptor) {}

    Epoll(): Epoll(epoll_create1(0)) {}

    int add(int descriptor, uint32_t events = EPOLLIN) {
        epoll_event event = {.events = events, .data{.fd = descriptor}};
        return check_error(epoll_ctl(m_descriptor, EPOLL_CTL_ADD, descriptor, &event));
    }

    int remove(int descriptor) {
        // Do I need to specify events that were given ad add?
        epoll_event event = {.events = 0, .data{.ptr = nullptr}};
        return check_error(epoll_ctl(m_descriptor, EPOLL_CTL_DEL, descriptor, &event));
    }

    int wait() {
        int res = check_error(epoll_wait(m_descriptor, m_events.data(), s_max_events, m_timeout));
        if (res >= 0) {
            m_number_of_events = uint32_t(res);
        } else {
            m_number_of_events = 0;
        }
        return res;
    }

    void set_timeout(int milliseconds) {
        m_timeout = milliseconds;
    }

    std::span<const epoll_event> get_events() const {
        return { m_events.data(), m_number_of_events };
    }

private:
    std::array<epoll_event, s_max_events> m_events;
    uint32_t m_number_of_events = 0;
    int m_timeout = 0;
    int m_descriptor;
};

inline static const char* const s_port = "7946";
inline static const size_t s_max_message_size = 512;
inline static char s_message_buffer[s_max_message_size];

