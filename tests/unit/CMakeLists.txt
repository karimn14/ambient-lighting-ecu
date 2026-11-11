# Host unit tests for the Automotive Ambient Lighting ECU.
#
# Build and run from the repository root with:   python tools/run_unit_tests.py
# or manually:
#   cmake -S tests/unit -B build/unit
#   cmake --build build/unit --config Debug
#   ctest --test-dir build/unit -C Debug --output-on-failure        (CTest 3.20 or newer)
#
# Options:
#   -DDELIBERATE_FAILURE=ON   adds one failing test, used to prove that the test run returns a non-zero exit
#                             code when a test fails (SRS TST-002, test case TC-TST-002-01).

cmake_minimum_required(VERSION 3.16)
project(ambient_lighting_host_tests C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

option(DELIBERATE_FAILURE "Add a test that always fails" OFF)

enable_testing()

# ---- Unity, pinned to a release tag for reproducibility -------------------------------------------------
# For stricter reproducibility replace the tag with the commit hash shown by: git ls-remote <url> v2.6.0
include(FetchContent)
FetchContent_Declare(
    unity
    GIT_REPOSITORY https://github.com/ThrowTheSwitch/Unity.git
    GIT_TAG        v2.6.0
)
FetchContent_MakeAvailable(unity)

# ---- Firmware include directories (all hardware-independent headers) -------------------------------------
get_filename_component(FW_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../firmware" ABSOLUTE)
set(FW_INCLUDE_DIRS
    ${FW_DIR}/common
    ${FW_DIR}/lamp_ecu/Application
    ${FW_DIR}/lamp_ecu/Services
    ${FW_DIR}/lamp_ecu/Drivers_App
    ${FW_DIR}/bcm/Application
    ${FW_DIR}/bcm/Drivers_App
)

if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    set(STRICT_FLAGS -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Werror)
    set(TEST_FLAGS   -Wall -Wextra -Wpedantic -Werror)
else()
    set(STRICT_FLAGS)
    set(TEST_FLAGS)
endif()

# ---- 1. Header self-containment and HAL-freedom -----------------------------------------------------------
# Every header is compiled alone in its own translation unit. A header that is not self-contained, or that
# includes a HAL or device header (which does not exist on the host), fails the build.
file(GLOB_RECURSE ALL_HEADERS CONFIGURE_DEPENDS
    ${FW_DIR}/common/*.h
    ${FW_DIR}/lamp_ecu/Application/*.h
    ${FW_DIR}/lamp_ecu/Services/*.h
    ${FW_DIR}/lamp_ecu/Drivers_App/*.h
    ${FW_DIR}/bcm/Application/*.h
    ${FW_DIR}/bcm/Drivers_App/*.h)

set(HEADER_CHECK_SOURCES)
foreach(header ${ALL_HEADERS})
    get_filename_component(name ${header} NAME_WE)
    get_filename_component(file ${header} NAME)
    set(src "${CMAKE_BINARY_DIR}/header_checks/hc_${name}.c")
    file(WRITE ${src}
        "#include \"${file}\"\n"
        "int HeaderCheck_${name}(void);\n"
        "int HeaderCheck_${name}(void) { return 0; }\n")
    list(APPEND HEADER_CHECK_SOURCES ${src})
endforeach()

add_library(header_checks OBJECT ${HEADER_CHECK_SOURCES})
target_include_directories(header_checks PRIVATE ${FW_INCLUDE_DIRS})
target_compile_options(header_checks PRIVATE ${STRICT_FLAGS})

# The include probes that CubeIDE compiles must also compile here.
add_library(include_probes OBJECT
    ${FW_DIR}/lamp_ecu/Application/include_probe.c)
target_include_directories(include_probes PRIVATE ${FW_INCLUDE_DIRS})
target_compile_options(include_probes PRIVATE ${STRICT_FLAGS})

# ---- 2. Unity tests ----------------------------------------------------------------------------------------
function(add_unit_test test_name)
    add_executable(${test_name} ${test_name}.c ${ARGN})
    target_include_directories(${test_name} PRIVATE ${FW_INCLUDE_DIRS})
    target_link_libraries(${test_name} PRIVATE unity)
    target_compile_options(${test_name} PRIVATE ${TEST_FLAGS})
    add_test(NAME ${test_name} COMMAND ${test_name})
endfunction()

add_unit_test(test_sanity)
add_unit_test(test_contracts)

if(DELIBERATE_FAILURE)
    add_unit_test(test_deliberate_failure)
endif()

# Make sure the header checks are built whenever tests are built.
add_custom_target(all_checks ALL DEPENDS header_checks include_probes)
