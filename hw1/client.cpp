#include <spdlog/spdlog.h>
#include "common.hpp"
#include "socket_tools.h"
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>


struct addrinfo
{
  int ai_flags;			/* Input flags.  */
  int ai_family;		/* Protocol family for socket.  */
  int ai_socktype;		/* Socket type.  */
  int ai_protocol;		/* Protocol for socket.  */
  socklen_t ai_addrlen;		/* Length of socket address.  */
  struct sockaddr *ai_addr;	/* Socket address for socket.  */
  char *ai_canonname;		/* Canonical name for service location.  */
  struct addrinfo *ai_next;	/* Pointer to next in list.  */
};

int main() {
    spdlog::info("starting client");

    Epoll epoll;
    epoll.set_timeout(1000);
    
    addrinfo my_info;
    auto client = create_dgram_socket("localhost", s_port, &my_info);
    epoll.add(client);

    auto send_data = [&]() {
        ssize_t res = sendto(client, "abacaba", 8, 0, my_info.ai_addr, my_info.ai_addrlen);
    };

    while (true) {
        send_data();

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

            if (event.data.fd != client) {
                spdlog::error("wait, why do I have more sockets?");
                continue;
            }
            
            sockaddr server_address;
            memset(&server_address, 0, sizeof(server_address));
            socklen_t address_len = 0;
            auto read = check_error(recvfrom(client, s_message_buffer, s_max_message_size, 0, &server_address, &address_len));

            if (server_address.sa_family != AF_INET) {
                spdlog::error("cannot process not ip4 connections");
                continue;
            }
        }
    }
}

