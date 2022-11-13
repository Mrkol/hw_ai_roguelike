cmake_minimum_required(VERSION 3.20)


add_executable(roguelike
    "sources/main.cpp"
    "sources/stateMachine.cpp"
    "sources/behTree.cpp"
    "sources/gameplay/entityFactories.cpp"
    "sources/gameplay/systems.cpp"
    "sources/gameplay/aiSystems.cpp"
    "sources/gameplay/behTreeLibrary.cpp"
    "sources/gameplay/dungeon/dungeonGenerator.cpp"
    "sources/gameplay/dungeon/dungeonUtils.cpp"
    "sources/gameplay/dungeon/dmaps.cpp"
)
target_include_directories(roguelike PRIVATE "sources")
target_link_libraries(roguelike
    fmt spdlog function2 glm::glm "yaml-cpp" flecs_static allegro mdspan
    allegro_font allegro_image allegro_primitives DearImGui)
target_compile_definitions(roguelike PRIVATE "PROJECT_SOURCE_DIR=\"${PROJECT_SOURCE_DIR}\"")

copy_allegro_dlls(roguelike)
