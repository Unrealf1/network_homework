cmake_minimum_required(VERSION 3.13)

project(hw2)

CPMAddPackage("gh:lsalzman/enet@1.3.17")
if(enet_ADDED)
  target_include_directories(enet INTERFACE "${enet_SOURCE_DIR}/include")
endif()

CPMAddPackage("gh:ArthurSonzogni/FTXUI@2.0.0")

add_executable(client2 client.cpp)
target_link_libraries(client2 PRIVATE project_options project_warnings)
target_link_libraries(client2 PUBLIC spdlog cxxopts nlohmann_json::nlohmann_json enet ftxui::screen ftxui::dom)

add_executable(server2 server.cpp game_server.hpp)
target_link_libraries(server2 PRIVATE project_options project_warnings)
target_link_libraries(server2 PUBLIC spdlog cxxopts nlohmann_json::nlohmann_json enet ftxui::screen ftxui::dom)

add_executable(lobby2 lobby.cpp)
target_link_libraries(lobby2 PRIVATE project_options project_warnings)
target_link_libraries(lobby2 PUBLIC spdlog cxxopts nlohmann_json::nlohmann_json enet)
