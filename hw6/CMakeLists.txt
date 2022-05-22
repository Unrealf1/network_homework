cmake_minimum_required(VERSION 3.13)

project(hw6)

CPMAddPackage("gh:g-truc/glm@0.9.9.8#0.9.9.8")
CPMAddPackage("gh:lsalzman/enet@1.3.17")
if(enet_ADDED)
  target_include_directories(enet INTERFACE "${enet_SOURCE_DIR}/include")
endif()

CPMAddPackage(
    NAME allegro
    GITHUB_REPOSITORY liballeg/allegro5
    GIT_TAG 5.2.7.0
    OPTIONS
      "WANT_DOCS off"
      "WANT_EXAMPLES off"
      "WANT_DEMO off"
      "WANT_TESTS off"
      "WANT_VIDEO off"
      "WANT_PHYSFS off"
      "WANT_IMAGE_WEBP off"
      "WANT_AUDIO off"
)

if(allegro_ADDED)
  target_include_directories(allegro INTERFACE
      "${allegro_SOURCE_DIR}/include"
      "${allegro_BINARY_DIR}/include"
  )

  foreach(ADDON font image color primitives)
    target_include_directories("allegro_${ADDON}" INTERFACE
        "${allegro_SOURCE_DIR}/addons/${ADDON}"
  )
  endforeach()
endif()

set(external_libraries spdlog cxxopts nlohmann_json::nlohmann_json enet allegro allegro_font allegro_primitives glm ftxui::screen ftxui::dom)

add_executable(matchmaking6 matchmaking.cpp matchmaking_server.cpp)
target_link_libraries(matchmaking6 PRIVATE project_options project_warnings)
target_link_libraries(matchmaking6 PUBLIC ${external_libraries} bytestream)

add_executable(proxy6 machine_proxy.cpp)
target_link_libraries(proxy6 PRIVATE project_options project_warnings)
target_link_libraries(proxy6 PUBLIC ${external_libraries} bytestream)

add_executable(server6 server.cpp game_server.cpp)
target_link_libraries(server6 PRIVATE project_options project_warnings)
target_link_libraries(server6 PUBLIC ${external_libraries} bytestream)
