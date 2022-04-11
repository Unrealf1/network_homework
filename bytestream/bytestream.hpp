#pragma once

#include <iterator>
#include <span>
#include <exception>
#include <stdexcept>
#include <cstring>


class ByteStream {
public:
    template<typename... T>
    ByteStream(const T&... args): m_buffer(args...) {}
protected:
    std::span<std::byte> m_buffer;
    std::span<std::byte>::iterator m_cursor = m_buffer.begin();
};

class InByteStream : public ByteStream {
public:
    template<typename... T>
    InByteStream(const T&... args): ByteStream(args...) {}

    template<typename T>
    void read(T& item) {
        decltype(m_buffer)::iterator::difference_type size = sizeof(item);
        if (size > std::distance(m_cursor, m_buffer.end())) {
            throw std::out_of_range("Not enough bytes to read");
        }

        memcpy(&item, &(*m_cursor), sizeof(item));
        m_cursor += size;
    }

    template<typename T>
    ByteStream& operator>>(T& item) {
        read(item);
        return *this;
    }
};

class OutByteStream : public ByteStream {
public:
    template<typename... T>
    OutByteStream(const T&... args): ByteStream(args...) {}

    template<typename T>
    void write(const T& item) {
        decltype(m_buffer)::iterator::difference_type size = sizeof(item);
        if (size > std::distance(m_cursor, m_buffer.end())) {
            throw std::out_of_range("Not enough bytes to read");
        }

        memcpy(&(*m_cursor), &item, sizeof(item));
        m_cursor += size;
    }

    template<typename T>
    ByteStream& operator<<(const T& item) {
        write(item);
        return *this;
    }
};

