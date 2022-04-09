#include  <spdlog/spdlog.h>
#include  <enet/enet.h>


int main() {
    spdlog::info("starting lobby server");

    if (int error = enet_initialize() != 0) {
        spdlog::error("could not initialize enet, error code {}", error);
        exit(error);
    }

    ENetAddress address;

    address.host = ENET_HOST_ANY;
    address.port = 7946;
    
    size_t peer_count = 32;
    size_t channel_limit = 2;
    uint32_t incoming_bandwith = 0;
    uint32_t outcoming_bandwith = 0;
    ENetHost* server = enet_host_create(&address, peer_count, channel_limit, incoming_bandwith, outcoming_bandwith);

    if (server == nullptr) {
        spdlog::error("could node create enet server");
    }

    while (true) {
        ENetEvent event;
        while(enet_host_service(server,&event, 10) > 0) {
            if (event.type == ENET_EVENT_TYPE_CONNECT) {
                spdlog::info("connection with {} established", event.peer->address.host,event.peer->address.port);
            } else if (event.type == ENET_EVENT_TYPE_RECEIVE) {
                spdlog::info("packet reecived: \"{}\"");
            } else {
                spdlog::warn("unsupported event type");
            }
        }
    }
}

