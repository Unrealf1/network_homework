#include <spdlog/spdlog.h>
#include "common.hpp"
#include "socket_tools.h"
#include <string_view>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <cxxopts.hpp>
#include <thread>
#include <sys/timerfd.h>
#include <netdb.h>



int main(int argc, char** argv) {
    // Dealing with cli
    cxxopts::Options options("client");
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

    spdlog::info("starting client");

    Epoll epoll;
    epoll.set_timeout(5000);
    // listen for stdin (user input)
    epoll.add(0);
    
    // create client socket
    addrinfo my_info;
    auto client = create_dgram_socket("localhost", port.c_str(), &my_info);
    epoll.add(client);

    // create timer for heartbeat
    timespec time;
    time.tv_sec = s_heartbeat_delay_millis / 1000;
    time.tv_nsec = 0;

    itimerspec spec;
    spec.it_value = time;
    spec.it_interval = time;
    auto timer = timerfd_create(CLOCK_MONOTONIC, 0);
    timerfd_settime(timer, 0, &spec, nullptr);
    epoll.add(timer);

    auto send_data = [&](const auto& msg) mutable {
        spdlog::debug("sending {} to the server", msg);
        check_error(sendto(client, msg.data(), msg.size(), 0, my_info.ai_addr, my_info.ai_addrlen));
    };
    
    auto register_self = [&]() mutable {
        std::string msg = std::to_string(std::hash<std::thread::id>{}(std::this_thread::get_id()));
        spdlog::info("registering with id {}", msg);
        check_error(sendto(client, msg.c_str(), msg.size(), 0, my_info.ai_addr, my_info.ai_addrlen));
    };
    register_self();

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

            if (event.data.fd == 0) {
                // input from terminal
                auto num_bytes = check_error(read(0, s_message_buffer, s_max_message_size));
                if (num_bytes > 0) {
                    std::string_view message(s_message_buffer, size_t(num_bytes));
                    send_data(message);
                }
                continue;
            } else if (event.data.fd == timer) {
                // keep alive
                read(timer, s_message_buffer, 8);
                send_data(s_heartbeat_msg);
                continue;
            } else if (event.data.fd == client) {
                // input from server
                auto read = check_error(recv(client, s_message_buffer, s_max_message_size, 0));
                
                if (read > 0) {
                    std::string_view answer {s_message_buffer, size_t(read)};
                    spdlog::info("got answer from server: {}", answer);
                } else {
                    spdlog::warn("empty input from server");
                }
            } else {
                spdlog::warn("unknown file descriptor!");
            }
        }
    }
}

