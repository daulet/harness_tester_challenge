cmake_minimum_required(VERSION 3.16)

project(harness_tester_simulator LANGUAGES CXX)

include(CheckCXXSourceCompiles)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(host_simulator
  simulator/src/analog.cpp
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

function(add_bug_evidence_case target firmware_source)
  set_source_files_properties(${firmware_source} PROPERTIES
    LANGUAGE CXX
    COMPILE_OPTIONS "-x;c++"
  )
  add_executable(${target}
    simulator/tests/bug_evidence_cases.cpp
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
add_bug_evidence_case(firmware_original_bug_cases firmware/firmware.ino)
add_bug_evidence_case(firmware_unmasked_bug_cases simulator/variants/firmware_unmasked.ino)

function(check_sanitizer_available sanitizer result)
  set(saved_required_flags "${CMAKE_REQUIRED_FLAGS}")
  set(saved_required_link_options "${CMAKE_REQUIRED_LINK_OPTIONS}")
  set(CMAKE_REQUIRED_FLAGS "-fsanitize=${sanitizer}")
  set(CMAKE_REQUIRED_LINK_OPTIONS "-fsanitize=${sanitizer}")
  check_cxx_source_compiles("int main() { return 0; }" ${result})
  set(CMAKE_REQUIRED_FLAGS "${saved_required_flags}")
  set(CMAKE_REQUIRED_LINK_OPTIONS "${saved_required_link_options}")
  set(${result} "${${result}}" PARENT_SCOPE)
endfunction()

function(add_firmware_sanitizer_case target sanitizer)
  set_source_files_properties(firmware/firmware.ino PROPERTIES
    LANGUAGE CXX
    COMPILE_OPTIONS "-x;c++"
  )
  add_executable(${target}
    simulator/tests/sanitizer_cases.cpp
    firmware/firmware.ino
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
    -fno-omit-frame-pointer
    -fsanitize=${sanitizer}
  )
  target_link_options(${target} PRIVATE
    -fno-omit-frame-pointer
    -fsanitize=${sanitizer}
  )
  if(sanitizer STREQUAL "undefined")
    target_compile_options(${target} PRIVATE -fno-sanitize-recover=undefined)
    target_link_options(${target} PRIVATE -fno-sanitize-recover=undefined)
  endif()
endfunction()

check_sanitizer_available(address HOST_SIM_HAS_ASAN)
check_sanitizer_available(undefined HOST_SIM_HAS_UBSAN)
if(HOST_SIM_HAS_ASAN)
  add_firmware_sanitizer_case(firmware_asan_cases address)
  message(STATUS "AddressSanitizer firmware witness: enabled")
else()
  message(STATUS "AddressSanitizer firmware witness: unavailable")
endif()
if(HOST_SIM_HAS_UBSAN)
  add_firmware_sanitizer_case(firmware_ubsan_cases undefined)
  message(STATUS "UndefinedBehaviorSanitizer firmware witness: enabled")
else()
  message(STATUS "UndefinedBehaviorSanitizer firmware witness: unavailable")
endif()

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

add_executable(uart_cases
  simulator/tests/uart_cases.cpp
)
target_link_libraries(uart_cases PRIVATE host_simulator)
target_compile_definitions(uart_cases PRIVATE
  HOST_SIM_ROOT="${CMAKE_CURRENT_SOURCE_DIR}"
)
target_compile_options(uart_cases PRIVATE
  -Wall
  -Wextra
  -Wpedantic
)

add_executable(timing_cases
  simulator/tests/timing_cases.cpp
  firmware/CY8C9560.cpp
)
target_link_libraries(timing_cases PRIVATE host_simulator)
target_compile_definitions(timing_cases PRIVATE
  HOST_SIM_ROOT="${CMAKE_CURRENT_SOURCE_DIR}"
)
target_compile_options(timing_cases PRIVATE
  -Wall
  -Wextra
  -Wpedantic
)

add_executable(i2c_cases
  simulator/tests/i2c_cases.cpp
)
target_link_libraries(i2c_cases PRIVATE host_simulator)
target_compile_definitions(i2c_cases PRIVATE
  HOST_SIM_ROOT="${CMAKE_CURRENT_SOURCE_DIR}"
)
target_compile_options(i2c_cases PRIVATE
  -Wall
  -Wextra
  -Wpedantic
)

add_executable(scenario_cases
  simulator/tests/scenario_cases.cpp
)
target_link_libraries(scenario_cases PRIVATE host_simulator)
target_compile_definitions(scenario_cases PRIVATE
  HOST_SIM_ROOT="${CMAKE_CURRENT_SOURCE_DIR}"
)

find_program(NGSPICE_EXECUTABLE NAMES ngspice)
if(NGSPICE_EXECUTABLE)
  message(STATUS "ngspice analog backend: ${NGSPICE_EXECUTABLE}")
else()
  set(NGSPICE_EXECUTABLE "ngspice")
  message(STATUS "ngspice not found; analog tests will be reported as skipped")
endif()

add_executable(analog_cases
  simulator/tests/analog_cases.cpp
)
target_link_libraries(analog_cases PRIVATE host_simulator)
target_compile_definitions(analog_cases PRIVATE
  HOST_SIM_ROOT="${CMAKE_CURRENT_SOURCE_DIR}"
  HOST_SIM_NGSPICE="${NGSPICE_EXECUTABLE}"
)
target_compile_options(analog_cases PRIVATE
  -Wall
  -Wextra
  -Wpedantic
)

enable_testing()
add_test(NAME pcb_source_parity COMMAND board_model_cases)
add_test(NAME runtime_defaults COMMAND runtime_cases)
add_test(NAME uart_timing_and_topology COMMAND uart_cases)
add_test(NAME button_and_reset_timing COMMAND timing_cases)
add_test(NAME i2c_timing_and_faults COMMAND i2c_cases)
add_test(NAME scenario_a05_narrow_masks COMMAND scenario_cases a05_narrow_masks)
add_test(NAME scenario_a10_uart_same_direction COMMAND scenario_cases a10_uart_same_direction)
add_test(NAME scenario_a11_sda_pull_down COMMAND scenario_cases a11_sda_pull_down)
add_test(NAME scenario_a12_led_no_series_resistors COMMAND scenario_cases a12_led_no_series_resistors)
add_test(NAME scenario_a16_d3_anode_isolated COMMAND scenario_cases a16_d3_anode_isolated)
add_test(NAME scenario_a18_lna_vcc_rf COMMAND scenario_cases a18_lna_vcc_rf)
add_test(NAME scenario_a25_reverse_polarity_path COMMAND scenario_cases a25_reverse_polarity_path)
add_test(NAME scenario_a29_u4_wrong_footprint COMMAND scenario_cases a29_u4_wrong_footprint)
set(ANALOG_CASES
  nominal
  reverse_polarity
  transient_clamp
  open_circuit
  unexpected_short
  leakage
  elevated_contact_resistance
  intermittent_connection
  bad_pull_network
  led_overcurrent
  led_no_current
  uart_level_mismatch
  i2c_pull_failure
  runtime_control
)
foreach(analog_case IN LISTS ANALOG_CASES)
  add_test(NAME analog_${analog_case} COMMAND analog_cases ${analog_case})
  set_tests_properties(analog_${analog_case} PROPERTIES SKIP_RETURN_CODE 77)
endforeach()
add_test(NAME original_good COMMAND firmware_original_cases original-good)
add_test(NAME original_broken COMMAND firmware_original_cases original-broken)
add_test(NAME patched_good_schematic COMMAND firmware_unmasked_cases patched-good-schematic)
add_test(NAME patched_broken_schematic COMMAND firmware_unmasked_cases patched-broken-schematic)
add_test(NAME bug_a01_missing_begin COMMAND firmware_original_bug_cases a01_missing_begin)
add_test(NAME bug_a02_missing_output_modes COMMAND firmware_original_bug_cases a02_missing_output_modes)
add_test(NAME bug_a03_nmea_buffer_reaches_end_without_guard
  COMMAND firmware_original_bug_cases a03_nmea_buffer_reaches_end_without_guard)
add_test(NAME bug_a04_invalid_status_accepted COMMAND firmware_unmasked_bug_cases a04_invalid_status_accepted)
add_test(NAME bug_a06_all_outputs COMMAND firmware_original_bug_cases a06_all_outputs)
add_test(NAME bug_a07_selected_output_undone COMMAND firmware_original_bug_cases a07_selected_output_undone)
add_test(NAME bug_a08_one_matching_row_passes
  COMMAND firmware_unmasked_bug_cases a08_one_matching_row_passes)
add_test(NAME bug_a09_failed_status_overwritten COMMAND firmware_unmasked_bug_cases a09_failed_status_overwritten)
add_test(NAME bug_a17_pressed_button_does_not_run COMMAND firmware_unmasked_bug_cases a17_pressed_button_does_not_run)
add_test(NAME bug_a19_gnrmc_ignored COMMAND firmware_unmasked_bug_cases a19_gnrmc_ignored)
add_test(NAME bug_a20_logical_index_gap COMMAND firmware_unmasked_bug_cases a20_logical_index_gap)
add_test(NAME bug_a21_leds_never_become_outputs COMMAND firmware_original_bug_cases a21_leds_never_become_outputs)
add_test(NAME bug_a23_held_gate_relogs COMMAND firmware_unmasked_bug_cases a23_held_gate_relogs)
if(HOST_SIM_HAS_ASAN)
  add_test(NAME sanitizer_a03_nmea_overflow
    COMMAND ${CMAKE_COMMAND}
      -DPROGRAM=$<TARGET_FILE:firmware_asan_cases>
      -DCASE=nmea-overflow
      -DSANITIZER_ENV=ASAN_OPTIONS=halt_on_error=1:abort_on_error=1:detect_leaks=0
      -DEXPECTED_1=global-buffer-overflow
      -DEXPECTED_2=nmea_buf
      -P ${CMAKE_CURRENT_SOURCE_DIR}/simulator/tests/expect_sanitizer_failure.cmake)
endif()
if(HOST_SIM_HAS_UBSAN)
  add_test(NAME sanitizer_a05_narrow_shift
    COMMAND ${CMAKE_COMMAND}
      -DPROGRAM=$<TARGET_FILE:firmware_ubsan_cases>
      -DCASE=narrow-shift
      -DSANITIZER_ENV=UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=0
      "-DEXPECTED_1=runtime error:"
      -DEXPECTED_2=shift
      -P ${CMAKE_CURRENT_SOURCE_DIR}/simulator/tests/expect_sanitizer_failure.cmake)
endif()
