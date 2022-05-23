#pragma once

#include <iterator>
#include <span>
#include <exception>
#include <stdexcept>
#include <cstring>
#include <type_traits>
#include <vector>


class InByteStream;
class OutByteStream;

template<typename T>
concept serializable = requires (T value, InByteStream istr, OutByteStream ostr) {
    {istr >> value};
    {ostr << value};
};

template<typename T>
concept serializable_container = requires (T container, decltype(*std::declval<T>().begin()) item) {
    {*container.begin()} -> serializable;
    {container.size()} -> std::same_as<size_t>;
    {container.reserve(size_t(0))};
    {container.push_back(item)};
};

class InByteStream {
public:
    template<typename T>
    InByteStream(T* ptr, size_t size)
    : m_buffer(reinterpret_cast<std::byte*>(ptr), size) {
        static_assert(sizeof(T) == sizeof(std::byte));
    }

    template<typename T>
    InByteStream(const T* begin, const T* end): InByteStream(begin, end - begin) {}

    std::span<std::byte> get_span() {
        return {m_cursor, m_buffer.end()};
    }

    template<typename T>
    std::enable_if_t<std::is_trivially_copyable_v<T>, void> read(T& item) {
        decltype(m_buffer)::iterator::difference_type size = sizeof(item);
        if (size > std::distance(m_cursor, m_buffer.end())) {
            throw std::out_of_range("Not enough bytes to read");
        }

        memcpy(&item, &(*m_cursor), sizeof(item));
        m_cursor += size;
    }

    template <serializable_container Container>
    void read(Container& item) {
        using value_t = std::remove_reference_t<decltype(*item.begin())>;
        auto size = get<size_t>();
        item.reserve(size);
        for (size_t i = 0; i < size; ++i) {
            item.push_back(get<value_t>());
        }
    }


    void read(std::string& str) {
        auto char_ptr = reinterpret_cast<char*>(&(*m_cursor));
        auto len = strlen(char_ptr);
        if (int64_t(len) > std::distance(m_cursor, m_buffer.end())) {
            throw std::out_of_range("Not enough bytes to read string");
        }
        str = std::string(char_ptr, len);
        m_cursor += int64_t(len) + 1;
    }

    template<typename T>
    T get() {
        T res;
        *this >> res;
        return res;
    }

    template<typename T>
    InByteStream& operator>>(T& item) {
        read(item);
        return *this;
    }
protected:
    std::span<std::byte> m_buffer;
    std::span<std::byte>::iterator m_cursor = m_buffer.begin();

};

class OutByteStream {
public:
    OutByteStream(size_t initial_size = 20) : m_buffer(initial_size) { }

    template<typename T>
    std::enable_if_t<std::is_trivially_copyable_v<T>, void> write(const T& item) {
        auto size = sizeof(item);
        if (size > m_buffer.size() - m_cursor) {
            m_buffer.resize(m_buffer.size() + size);
        }

        memcpy(get_write_address(), &item, size);
        m_cursor += size;
    }

    void write(const std::string& str) {
        auto char_ptr = str.c_str();
        auto len = str.size() + 1;
        if (len > m_buffer.size() - m_cursor) {
            m_buffer.resize(m_buffer.size() + len);
        }
        if (*(char_ptr + len - 1) != 0) {
            throw std::runtime_error("string should end with \\0");
        }
        memcpy(get_write_address(), char_ptr, len);
        m_cursor += len;
    }

    void write(const std::span<const std::byte>& bytes) {
        if (bytes.size() > m_buffer.size() - m_cursor) {
            m_buffer.resize(m_buffer.size() + bytes.size());
        }
        memcpy(get_write_address(), bytes.data(), bytes.size());
        m_cursor += bytes.size();
    }

    void write(const std::vector<std::byte>& vector) {
        const std::span<const std::byte> span(vector.begin(), vector.end());
        write(span);
    }

    template<serializable_container Container>
    void write(const Container& container) {
        write(container.size());
        for (size_t i = 0; i < container.size(); ++i) {
            *this << container[i];
        }
    }

    template<typename T>
    OutByteStream& operator<<(const T& item) {
        write(item);
        return *this;
    }

    std::span<std::byte> get_span() {
        return std::span<std::byte>(m_buffer.begin(), m_cursor);
    }

protected:
    std::vector<std::byte> m_buffer;
    size_t m_cursor = 0;

    std::byte* get_write_address() {
        return &(*(m_buffer.begin() + int64_t(m_cursor)));
    }
};

