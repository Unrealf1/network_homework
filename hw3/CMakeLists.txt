cmake_minimum_required(VERSION 3.13)

project(hw3)

CPMAddPackage("gh:lsalzman/enet@1.3.17")
if(enet_ADDED)
  target_include_directories(enet INTERFACE "${enet_SOURCE_DIR}/include")
endif()


add_executable(server3 server.cpp)
target_link_libraries(server3 PRIVATE project_options project_warnings)
target_link_libraries(server3 PUBLIC spdlog cxxopts nlohmann_json::nlohmann_json enet bytestream)

