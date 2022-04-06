#include <functional>
#include <spdlog/spdlog.h>
#include "common.hpp"
#include "socket_tools.h"
#include <unordered_map>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <compare>


struct user_info {
    std::string addr;
    int port;
    auto operator<=>(const user_info&) const = default;
};

template <>
struct std::hash<user_info> {
    size_t operator()(const user_info& item) const {
        auto h1 = std::hash<std::string>{}(item.addr);
        auto h2 = std::hash<int>{}(item.port);
        return h1 ^ (h2 << 1);
    }
};

template <>
struct fmt::formatter<user_info> {
    template<typename ParseContext>
    constexpr auto parse(ParseContext& ctx) -> decltype(ctx.begin()) {
        return ctx.begin();
    }

    template <typename FormatContext>
    auto format(const user_info& info, FormatContext& ctx) -> decltype(ctx.out()) {
        return format_to(ctx.out(), "{} (port {})", info.addr, info.port);
    }
};


int main() {
    spdlog::info("starting server");
    Epoll epoll;
    epoll.set_timeout(1000);
    
    auto server = create_dgram_socket(nullptr, s_port, nullptr);
    epoll.add(server);

    std::unordered_map<user_info, std::string> clients;

    auto register_client = [&](user_info client, size_t bytes_read) {
        if (bytes_read > 0) {
            std::string client_id {s_message_buffer};
            clients.insert({client, client_id});
            spdlog::info("registered new client! id = {}; info = {}", client_id, client);
        } else {
            spdlog::error("warn user tried to register with empty name. Denying");
        }

    };

    auto process_client_input = [&](const sockaddr_in& address, size_t bytes_read) {
        user_info info { inet_ntoa(address.sin_addr), ntohs(address.sin_port) };
        auto id = clients.find(info);
        if (id == clients.end()) {
            // still not registered
            return register_client(info, bytes_read);
        } 
        
        if (bytes_read > 0) {
            spdlog::info("got message {} from client {}({})", std::string(s_message_buffer), id->second, info);
        }
    };

    while (true) {
        epoll.wait();
        auto events = epoll.get_events();
        if (events.empty()) {
            spdlog::info("no events");
        }
        for (const auto& event : events) {
            if ((event.events & EPOLLIN) != EPOLLIN) {
                // Only interested in read events
                spdlog::warn("unexpected events mask: {}", event.events);
                continue;
            }
            /*
            if (event.data.fd != server) {
                spdlog::error("wait, why do I have more sockets? Server socket is {}, given socket is {}", server, event.data.fd);
                continue;
            }*/
            
            sockaddr client_address;
            memset(&client_address, 0, sizeof(client_address));
            socklen_t address_len = 0;
            auto read = check_error(recvfrom(server, s_message_buffer, s_max_message_size, 0, &client_address, &address_len));

            if (client_address.sa_family != AF_INET) {
                spdlog::error("cannot process not ip4 connections (got {})", client_address.sa_family);
                continue;
            }
            auto& r = client_address;
            process_client_input(reinterpret_cast<sockaddr_in&>(r), read);
        }
    }
}

