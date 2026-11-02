cmake_minimum_required(VERSION 3.20)
project(secondscreen_engine
    VERSION 0.4.0
    DESCRIPTION "SecondScreen Windows Native Streaming Engine"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Ensure standalone, zero-dependency deployment on any clean Windows PC
if(MINGW)
    add_link_options(-static -static-libgcc -static-libstdc++)
endif()

# Compile Windows PE icon resource
if(MINGW)
    add_custom_command(
        OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/app_icon.res.obj
        COMMAND windres -i app.rc -o app_icon.res.obj
        COMMAND ${CMAKE_COMMAND} -E copy_if_different app_icon.res.obj ${CMAKE_CURRENT_BINARY_DIR}/app_icon.res.obj
        COMMAND ${CMAKE_COMMAND} -E remove app_icon.res.obj
        DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/resources/app.rc ${CMAKE_CURRENT_SOURCE_DIR}/resources/duetview.ico
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/resources
    )
    set(APP_ICON_RES ${CMAKE_CURRENT_BINARY_DIR}/app_icon.res.obj)
else()
    set(APP_ICON_RES resources/app.rc)
endif()

# -----------------------------------------------------------------------------
# 1. Protocol, Capture & Encoder Foundation Library
# -----------------------------------------------------------------------------
add_library(secondscreen_protocol STATIC
    src/protocol/protocol_header.cpp
    src/protocol/packet_parser.cpp
    src/config/engine_config.cpp
    src/metrics/metrics_tracker.cpp
    src/capture/dxgi_capturer.cpp
    src/encoder/h264_parser.cpp
    src/encoder/mf_h264_encoder.cpp
    src/transport/tcp_transport_server.cpp
    src/adaptive/adaptive_controller.cpp
    src/discovery/udp_discovery_responder.cpp
    src/pairing/qrcodegen.cpp
    src/pairing/pairing_manager.cpp
    src/input/input_injector.cpp
    src/capture/cursor_manager.cpp
    src/driver/driver_manager.cpp
    src/engine/engine_runner.cpp
)

target_include_directories(secondscreen_protocol PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(secondscreen_protocol PUBLIC
    d3d11
    dxgi
    mf
    mfplat
    mfuuid
    ole32
    oleaut32
    ws2_32
    iphlpapi
    user32
    gdi32
    setupapi
    cfgmgr32
    shlwapi
)

if(MSVC)
    target_compile_options(secondscreen_protocol PRIVATE
        /W4
        /permissive-
    )
else()
    target_compile_options(secondscreen_protocol PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -Wconversion
        -Wshadow
    )
endif()

# -----------------------------------------------------------------------------
# 2. Windows Desktop Application (GUI & Unified Streaming Engine)
# -----------------------------------------------------------------------------
add_executable(DuetView-PC WIN32
    src/app/main_window.cpp
    src/app/process_manager.cpp
    ${APP_ICON_RES}
)

target_link_libraries(DuetView-PC PRIVATE
    secondscreen_protocol
    comctl32
    dwmapi
    shlwapi
    shell32
)

if(MSVC)
    target_compile_options(DuetView-PC PRIVATE /W4 /permissive-)
else()
    target_compile_options(DuetView-PC PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Stage driver package files into build/driver directory
add_custom_target(stage_driver_package ALL
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/driver
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/../virtual_display_driver/SecondScreenIdd.inf
        ${CMAKE_BINARY_DIR}/driver/SecondScreenIdd.inf
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/../virtual_display_driver/SecondScreenIdd.dll
        ${CMAKE_BINARY_DIR}/driver/SecondScreenIdd.dll
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/../virtual_display_driver/SecondScreenIdd.cat
        ${CMAKE_BINARY_DIR}/driver/SecondScreenIdd.cat
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/../virtual_display_driver/scripts/install_driver.bat
        ${CMAKE_BINARY_DIR}/driver/install_driver.bat
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/../virtual_display_driver/scripts/uninstall_driver.bat
        ${CMAKE_BINARY_DIR}/driver/uninstall_driver.bat
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/resources/duetview.ico
        ${CMAKE_BINARY_DIR}/duetview.ico
    COMMENT "Staging driver package files to build/driver"
)

if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/../virtual_display_driver/devcon.exe)
    add_custom_command(TARGET stage_driver_package POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${CMAKE_CURRENT_SOURCE_DIR}/../virtual_display_driver/devcon.exe
            ${CMAKE_BINARY_DIR}/driver/devcon.exe
    )
endif()

# -----------------------------------------------------------------------------
# 6. DuetView PC Windows Standalone Installer
# -----------------------------------------------------------------------------
add_executable(DuetViewPC-Setup WIN32
    ../windows_installer/src/installer_main.cpp
)

target_link_libraries(DuetViewPC-Setup PRIVATE
    comctl32
    shlwapi
    shell32
    ole32
    uuid
)

if(MSVC)
    target_compile_options(DuetViewPC-Setup PRIVATE /W4 /permissive-)
else()
    target_compile_options(DuetViewPC-Setup PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_dependencies(DuetViewPC-Setup stage_driver_package DuetView-PC)

# -----------------------------------------------------------------------------
# 7. Automated Tests
# -----------------------------------------------------------------------------
include(CTest)
if(BUILD_TESTING)
    # Protocol Tests
    add_executable(secondscreen_protocol_tests
        tests/test_protocol.cpp
    )
    target_link_libraries(secondscreen_protocol_tests PRIVATE secondscreen_protocol)
    if(MSVC)
        target_compile_options(secondscreen_protocol_tests PRIVATE /W4 /permissive-)
    else()
        target_compile_options(secondscreen_protocol_tests PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    add_test(NAME ProtocolTests COMMAND secondscreen_protocol_tests)

    # Capture Tests
    add_executable(secondscreen_capture_tests
        tests/test_capture.cpp
    )
    target_link_libraries(secondscreen_capture_tests PRIVATE secondscreen_protocol)
    if(MSVC)
        target_compile_options(secondscreen_capture_tests PRIVATE /W4 /permissive-)
    else()
        target_compile_options(secondscreen_capture_tests PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    add_test(NAME CaptureTests COMMAND secondscreen_capture_tests)

    # Encoder & NAL Parser Tests
    add_executable(secondscreen_encoder_tests
        tests/test_encoder.cpp
    )
    target_link_libraries(secondscreen_encoder_tests PRIVATE secondscreen_protocol)
    if(MSVC)
        target_compile_options(secondscreen_encoder_tests PRIVATE /W4 /permissive-)
    else()
        target_compile_options(secondscreen_encoder_tests PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    add_test(NAME EncoderTests COMMAND secondscreen_encoder_tests)

    # Driver Manager & Lifecycle Tests
    add_executable(secondscreen_driver_tests
        tests/test_driver_manager.cpp
    )
    target_link_libraries(secondscreen_driver_tests PRIVATE secondscreen_protocol)
    if(MSVC)
        target_compile_options(secondscreen_driver_tests PRIVATE /W4 /permissive-)
    else()
        target_compile_options(secondscreen_driver_tests PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    add_test(NAME DriverTests COMMAND secondscreen_driver_tests)
endif()

# -----------------------------------------------------------------------------
# 8. CPack Packaging Configuration
# -----------------------------------------------------------------------------
add_custom_command(TARGET DuetViewPC-Setup POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:DuetViewPC-Setup>
        ${CMAKE_BINARY_DIR}/DuetView-PC-Setup.exe
)

install(TARGETS DuetView-PC DuetViewPC-Setup
    RUNTIME DESTINATION .
)

install(FILES
    ${CMAKE_BINARY_DIR}/DuetView-PC-Setup.exe
    DESTINATION .
)

install(DIRECTORY ${CMAKE_BINARY_DIR}/driver
    DESTINATION .
)

if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/resources/duetview.ico)
    install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/resources/duetview.ico
        DESTINATION .
    )
endif()

set(CPACK_PACKAGE_NAME "DuetView-PC")
set(CPACK_PACKAGE_VENDOR "DuetView Project")
set(CPACK_PACKAGE_VERSION "0.4.0")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "DuetView PC Virtual Display & Screen Mirroring Host")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "DuetView")
set(CPACK_GENERATOR "ZIP")
include(CPack)
