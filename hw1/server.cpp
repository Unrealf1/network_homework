#include <chrono>
#include <functional>
#include <spdlog/spdlog.h>
#include "common.hpp"
#include "socket_tools.h"
#include <string_view>
#include <unordered_map>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <compare>
#include <cxxopts.hpp>


struct user_info {
    std::string addr;
    uint16_t port;
    auto operator<=>(const user_info&) const = default;
};

struct client_info {
    using clock_t = std::chrono::steady_clock;

    std::string id;
    clock_t::time_point last_heartbeat{clock_t::now()};
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



int main(int argc, char** argv) {
    cxxopts::Options options("server");
    options.add_options()
        ("p,port", "Specify port", cxxopts::value<std::string>()->default_value(s_port))
        ("h,help", "Show help")
        ;
    auto parsed = options.parse(argc, argv);
    if (parsed.count("h") > 0) {
        std::cout << options.help() << '\n';
        return 0;
    }
    auto port = parsed["p"].as<std::string>();
    spdlog::info("starting server");
    Epoll epoll;
    epoll.set_timeout(5000);
    
    auto server = create_dgram_socket(nullptr, port.c_str(), nullptr);
    epoll.add(server);

    std::unordered_map<user_info, client_info> clients;

    auto register_client = [&](user_info client, size_t bytes_read) {
        if (bytes_read > 0) {
            std::string client_id { s_message_buffer, bytes_read };
            clients.insert({client, {client_id}});
            spdlog::info("registered new client! id = {}; info = {}", client_id, client);
        } else {
            spdlog::error("warn user tried to register with empty name. Denying");
        }

    };

    auto broadcast_message = [&](const std::string& message) {
            for (auto [user, client] : clients) {
                sockaddr_in address;
                in_addr inet_addr;
                check_error(inet_aton(user.addr.c_str(), &inet_addr));
                address.sin_family = AF_INET;
                address.sin_addr = inet_addr;
                address.sin_port = ntohs(user.port);
                check_error(sendto(
                            server, 
                            message.c_str(), 
                            message.size(), 
                            0, 
                            reinterpret_cast<const sockaddr*>(&address), 
                            sizeof(address))
                );
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
            std::string_view client_message(s_message_buffer, bytes_read);
            if (client_message == s_heartbeat_msg) {
                spdlog::info("Huge success(heartbeat) {}", info);
                id->second.last_heartbeat = client_info::clock_t::now();
                return;
            }
            spdlog::info("got message {} from client {}({})", client_message, id->second.id, info);
            std::string answer = fmt::format("Broadcasting message from, client {}: \"{}\"", id->second.id, client_message);
            spdlog::info("Broadcasting {}", answer);
            broadcast_message(answer);
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
            
            if (event.data.fd != server) {
                spdlog::error("wait, why do I have more sockets? Server socket is {}, given socket is {}", server, event.data.fd);
                continue;
            }
            
            sockaddr client_address;
            memset(&client_address, 0, sizeof(client_address));
            socklen_t address_len = sizeof(client_address);
            auto read = check_error(recvfrom(server, s_message_buffer, s_max_message_size, 0, &client_address, &address_len));

            if (client_address.sa_family != AF_INET) {
                spdlog::error("cannot process not ip4 connections (got {})", client_address.sa_family);
                continue;
            }
            auto& actual_address = reinterpret_cast<sockaddr_in&>(client_address);
            if (read > 0) {
                process_client_input(actual_address, size_t(read));
            }
        }

        // remove dead clients
        auto time = client_info::clock_t::now();
        auto count = std::erase_if(clients, [&](const auto& item) {
            return std::chrono::duration_cast<std::chrono::milliseconds>(time - item.second.last_heartbeat).count() > 3 * s_heartbeat_delay_millis;
        });

        if (count > 0) {
            spdlog::warn("{} client(s) were removed due to inactivity", count);
        }
    }
}

