add_executable(FluentUITests
    test_math.cpp
    test_color.cpp
    test_generate_id.cpp
    test_undo_system.cpp
    test_shortcut_registry.cpp
    test_layout_serializer.cpp
    test_dock_system.cpp
    test_gpu_integration.cpp
)

target_link_libraries(FluentUITests PRIVATE
    FluentUI
    Catch2::Catch2WithMain
)

target_include_directories(FluentUITests PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

catch_discover_tests(FluentUITests)
