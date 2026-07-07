cmake_minimum_required(VERSION 3.20)
project(FreeAnimation2dStyle VERSION 2.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

if(MSVC)
    add_compile_options(/utf-8)
endif()

option(FAP_BUILD_TESTS "Build tests" ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Multimedia Svg)

include(FetchContent)

FetchContent_Declare(
    miniz
    GIT_REPOSITORY https://github.com/richgel999/miniz.git
    GIT_TAG 3.0.2
)
cmake_policy(SET CMP0169 OLD)
FetchContent_GetProperties(miniz)
if(NOT miniz_POPULATED)
    FetchContent_Populate(miniz)
endif()
cmake_policy(SET CMP0169 NEW)

file(WRITE ${miniz_SOURCE_DIR}/miniz_export.h
    "#ifndef MINIZ_EXPORT_H\n#define MINIZ_EXPORT_H\n#define MINIZ_EXPORT\n#endif\n")

add_library(miniz STATIC
    ${miniz_SOURCE_DIR}/miniz.c
    ${miniz_SOURCE_DIR}/miniz_tdef.c
    ${miniz_SOURCE_DIR}/miniz_tinfl.c
    ${miniz_SOURCE_DIR}/miniz_zip.c
)
target_include_directories(miniz PUBLIC ${miniz_SOURCE_DIR})
if(MSVC)
    target_compile_options(miniz PRIVATE /W0)
else()
    target_compile_options(miniz PRIVATE -w)
endif()

if(FAP_BUILD_TESTS)
    enable_testing()
    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG v1.14.0
    )
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)
endif()

set(CORE_HEADERS
    src/core/types.hpp
    src/core/layer.hpp
    src/core/sequence.hpp
    src/core/document.hpp    src/core/stroke.hpp
    src/core/canvas.hpp
    src/core/undo_manager.hpp
    src/core/tool_state.hpp
    src/core/app_state.hpp
)

set(CORE_SOURCES
    src/core/types.cpp
    src/core/layer.cpp
    src/core/sequence.cpp
    src/core/document.cpp    src/core/stroke.cpp
    src/core/canvas.cpp
    src/core/project.cpp
    src/core/undo_manager.cpp
    src/core/tool_state.cpp
    src/core/app_state.cpp
)

set(ENGINE_HEADERS
    src/engine/raster/raster_engine.hpp
    src/engine/vector/vector_engine.hpp
    src/engine/vector/bezier_path.hpp
    src/engine/brush/brush_engine.hpp
    src/engine/brush/ruler_guide.hpp
    src/engine/brush/pencil_retouch.hpp
    src/engine/brush/abr_importer.hpp
    src/engine/common/blend_utils.hpp
    src/engine/compositor/compositor.hpp
    src/engine/compositor/node_graph.hpp
    src/engine/deformation/deformation_mesh.hpp
    src/engine/deformation/deformation_engine.hpp
    src/engine/animation/audio_engine.hpp
    src/engine/animation/tween_engine.hpp
    src/engine/animation/frame_thumbnail.hpp
)

set(ENGINE_SOURCES
    src/engine/raster/raster_engine.cpp
    src/engine/raster/raster_stroke.cpp
    src/engine/vector/vector_engine.cpp
    src/engine/vector/vector_stroke.cpp
    src/engine/vector/bezier_path.cpp
    src/engine/animation/timeline_engine.cpp
    src/engine/animation/onion_skin.cpp
    src/engine/animation/playback.cpp
    src/engine/animation/audio_engine.cpp
    src/engine/animation/tween_engine.cpp
    src/engine/animation/frame_thumbnail.cpp
    src/engine/brush/brush_engine.cpp
    src/engine/brush/brush_preset.cpp
    src/engine/brush/paper_texture.cpp
    src/engine/brush/ruler_guide.cpp
    src/engine/brush/pencil_retouch.cpp
    src/engine/brush/abr_importer.cpp
    src/engine/compositor/compositor.cpp
    src/engine/compositor/node_graph.cpp
    src/engine/deformation/deformation_mesh.cpp
    src/engine/deformation/deformation_engine.cpp
)

set(UI_HEADERS
    src/ui_v2/main_window_v2.hpp
    src/ui_v2/canvas_widget_v2.hpp
    src/ui_v2/timeline_panel_v2.hpp
    src/ui_v2/toolbox_panel_v2.hpp
    src/ui_v2/color_panel_v2.hpp
    src/ui_v2/layer_panel_v2.hpp
    src/ui_v2/property_editor_v2.hpp
    src/ui_v2/audio_track_widget.hpp
)

set(UI_SOURCES
    src/ui_v2/main_window_v2.cpp
    src/ui_v2/canvas_widget_v2.cpp
    src/ui_v2/timeline_panel_v2.cpp
    src/ui_v2/toolbox_panel_v2.cpp
    src/ui_v2/color_panel_v2.cpp
    src/ui_v2/layer_panel_v2.cpp
    src/ui_v2/layer_lock_button.cpp
    src/ui_v2/property_editor_v2.cpp
    src/ui_v2/audio_track_widget.cpp
)

set(IO_HEADERS
    src/io/document_manager.hpp
    src/io/serialization_common.hpp
)

set(IO_SOURCES
    src/io/document_io.cpp
    src/io/video_export.cpp
    src/io/file_format.cpp
    src/io/document_manager.cpp
    src/io/serialization_common.cpp
)

set(PLATFORM_HEADERS
    src/platform/input_manager.hpp
    src/platform/tablet_handler.hpp
)

set(PLATFORM_SOURCES
    src/platform/input_manager.cpp
    src/platform/tablet_handler.cpp
)

set(ALL_SOURCES
    src/main.cpp
    ${CORE_HEADERS}
    ${CORE_SOURCES}
    ${ENGINE_HEADERS}
    ${ENGINE_SOURCES}
    ${UI_HEADERS}
    ${UI_SOURCES}
    ${IO_HEADERS}
    ${IO_SOURCES}
    ${PLATFORM_HEADERS}
    ${PLATFORM_SOURCES}
    resources/resources.qrc
)

add_executable(free-animation-2d-style ${ALL_SOURCES})

target_include_directories(free-animation-2d-style PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/core
    ${CMAKE_SOURCE_DIR}/src/engine
    ${CMAKE_SOURCE_DIR}/src/ui_v2
    ${CMAKE_SOURCE_DIR}/src/io
    ${CMAKE_SOURCE_DIR}/src/platform
)

target_link_libraries(free-animation-2d-style PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Multimedia
    Qt6::Svg
    miniz
)

target_compile_definitions(free-animation-2d-style PRIVATE
    FAP_VERSION="2.0.0"
    FAP_APP_NAME="FreeAnimation2dStyle"
)

if(FAP_BUILD_TESTS)
    set(TEST_SOURCES
        tests/test_document.cpp
        tests/test_brush_engine.cpp
        tests/test_bezier.cpp
        tests/test_file_format.cpp
        tests/test_layer.cpp
        tests/test_layer_memory.cpp
        tests/test_compositor.cpp
        tests/test_undo.cpp
        tests/test_deformation.cpp
        tests/test_node_graph.cpp
        tests/test_audio_engine.cpp
        tests/test_ruler_guide.cpp
        tests/test_tween_engine.cpp
        tests/test_pencil_retouch.cpp
        tests/test_abr_importer.cpp
    )
    set(TEST_IMPL_SOURCES
        ${CORE_SOURCES}
        ${ENGINE_SOURCES}
        ${IO_SOURCES}
        ${PLATFORM_SOURCES}
    )
    add_executable(fap-tests ${TEST_SOURCES} ${TEST_IMPL_SOURCES})
    target_include_directories(fap-tests PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/src/core
        ${CMAKE_SOURCE_DIR}/src/engine
        ${CMAKE_SOURCE_DIR}/src/ui_v2
        ${CMAKE_SOURCE_DIR}/src/io
        ${CMAKE_SOURCE_DIR}/src/platform
    )
    target_link_libraries(fap-tests PRIVATE GTest::gtest_main Qt6::Core Qt6::Gui Qt6::Svg miniz)
    include(GoogleTest)
    gtest_discover_tests(fap-tests)
endif()

if(WIN32)
    get_target_property(QT_QMAKE_EXECUTABLE Qt6::qmake IMPORTED_LOCATION)
    get_filename_component(QT_BIN_DIR "${QT_QMAKE_EXECUTABLE}" DIRECTORY)
    find_program(WINDEPLOYQT_EXECUTABLE windeployqt HINTS "${QT_BIN_DIR}")
    if(WINDEPLOYQT_EXECUTABLE)
        add_custom_command(TARGET free-animation-2d-style POST_BUILD
            COMMAND "${WINDEPLOYQT_EXECUTABLE}" "$<TARGET_FILE:free-animation-2d-style>"
            COMMENT "Deploying Qt DLLs..."
            VERBATIM
        )
    endif()
endif()

install(TARGETS free-animation-2d-style DESTINATION bin)
install(DIRECTORY resources/ DESTINATION share/free-animation-2d-style)
