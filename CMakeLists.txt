cmake_minimum_required(VERSION 3.16)

project(harness_tester_simulator LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(host_simulator
  simulator/src/board.cpp
  simulator/src/runtime.cpp
)

target_include_directories(host_simulator PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/simulator/include
  ${CMAKE_CURRENT_SOURCE_DIR}/firmware
)

target_compile_options(host_simulator PRIVATE
  -Wall
  -Wextra
  -Wpedantic
)

function(add_firmware_case target firmware_source)
  set_source_files_properties(${firmware_source} PROPERTIES
    LANGUAGE CXX
    COMPILE_OPTIONS "-x;c++"
  )
  add_executable(${target}
    simulator/tests/firmware_cases.cpp
    ${firmware_source}
    firmware/CY8C9560.cpp
  )
  target_link_libraries(${target} PRIVATE host_simulator)
  target_compile_definitions(${target} PRIVATE
    HOST_SIM_ROOT="${CMAKE_CURRENT_SOURCE_DIR}"
  )
  target_compile_options(${target} PRIVATE
    -Wall
    -Wextra
    -Wpedantic
  )
endfunction()

add_firmware_case(firmware_original_cases firmware/firmware.ino)
add_firmware_case(firmware_unmasked_cases simulator/variants/firmware_unmasked.ino)

add_executable(harness_simulator_original
  simulator/src/original_main.cpp
  firmware/firmware.ino
  firmware/CY8C9560.cpp
)
set_source_files_properties(firmware/firmware.ino PROPERTIES
  LANGUAGE CXX
  COMPILE_OPTIONS "-x;c++"
)
target_link_libraries(harness_simulator_original PRIVATE host_simulator)
target_compile_definitions(harness_simulator_original PRIVATE
  HOST_SIM_ROOT="${CMAKE_CURRENT_SOURCE_DIR}"
)

add_executable(board_model_cases
  simulator/tests/board_model_cases.cpp
)
target_link_libraries(board_model_cases PRIVATE host_simulator)
target_compile_definitions(board_model_cases PRIVATE
  HOST_SIM_ROOT="${CMAKE_CURRENT_SOURCE_DIR}"
)

add_executable(runtime_cases
  simulator/tests/runtime_cases.cpp
)
target_link_libraries(runtime_cases PRIVATE host_simulator)
target_compile_definitions(runtime_cases PRIVATE
  HOST_SIM_ROOT="${CMAKE_CURRENT_SOURCE_DIR}"
)

enable_testing()
add_test(NAME pcb_overlay_model COMMAND board_model_cases)
add_test(NAME runtime_defaults COMMAND runtime_cases)
add_test(NAME original_good COMMAND firmware_original_cases original-good)
add_test(NAME original_broken COMMAND firmware_original_cases original-broken)
add_test(NAME patched_good_schematic COMMAND firmware_unmasked_cases patched-good-schematic)
add_test(NAME patched_broken_schematic COMMAND firmware_unmasked_cases patched-broken-schematic)
add_test(NAME patched_good_asdrawn COMMAND firmware_unmasked_cases patched-good-asdrawn)
