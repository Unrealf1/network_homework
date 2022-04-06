cmake_minimum_required(VERSION 3.13)


add_executable(client client.cpp common.hpp socket_tools.cpp)
target_link_libraries(client PRIVATE project_options project_warnings)
target_link_libraries(client PUBLIC spdlog cxxopts)

add_executable(server server.cpp common.hpp socket_tools.cpp)
target_link_libraries(server PRIVATE project_options project_warnings)
target_link_libraries(server PUBLIC spdlog cxxopts)

