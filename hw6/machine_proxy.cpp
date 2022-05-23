#include <sstream>

#include <enet/enet.h>

#include "base_server.hpp"
#include "lobby.hpp"


class ProxyServer: public BaseServer<ProxyServer> {
public:
    ProxyServer(ENetHost* host): BaseServer(host) {}

    static constexpr std::chrono::milliseconds s_update_time = 10ms;
public:
    void update() {
        if (matchmaking == nullptr) {
            ENetAddress address;
            enet_address_set_host(&address, "localhost");
            address.port = s_matchmaking_server_port;
            matchmaking = enet_host_connect(m_host, &address, 2, 0);
            return;
        }
        if (!registered) {
            OutByteStream msg;
            msg << MessageType::register_provider;
            if (send_bytes<true>(msg.get_span(), matchmaking) == 0) {
                registered = true;
            } 
        }
    }
    void on_start() {}
    void on_finish() {}
    void process_new_connection(ENetEvent&) { }

    void process_data(ENetEvent& event) {
        InByteStream istr(event.packet->data, event.packet->dataLength);
        MessageType type;
        istr >> type;
        if (type == MessageType::lobby_start) {
            auto name = istr.get<std::string>();
            auto num_mods = istr.get<size_t>();
            std::vector<Mod> mods;
            mods.reserve(num_mods);
            for (size_t i = 0; i < num_mods; ++i) {
                mods.push_back(istr.get<Mod>());
            }
            launch_server_process(mods, name);
        }
    }

    void process_disconnect(ENetEvent& event) {
        if (event.peer->address == matchmaking->address) {
            matchmaking = nullptr;
            registered = false;
        }
    }

private:
    ENetPeer* matchmaking = nullptr;
    std::vector<std::jthread> server_threads;
    uint16_t next_port;
    bool registered = false;

    static constexpr const char* s_server_executable = "./build/hw6/server6";

    void launch_server_process(const std::vector<Mod>& mods, const std::string& name) {
        server_threads.emplace_back([=]{
            std::stringstream command;
            command << s_server_executable << ' ' << next_port++ << ' ' <<  name;
            for (const auto& mod : mods) {
                command << ' ' << mod.to_str();
            }
            spdlog::info("creating server with command {}", command.str());
            std::system(command.str().c_str());        
        });
    }

};

int main() {
    ProxyServer proxy(create_host(7'000));
    proxy.run();
}
