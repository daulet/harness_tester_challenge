cmake_minimum_required(VERSION 3.16)

project(harness_tester_simulator LANGUAGES CXX)

include(CheckCXXSourceCompiles)
find_package(Python3 REQUIRED COMPONENTS Interpreter)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(host_simulator
  simulator/src/analog.cpp
  simulator/src/board.cpp
  simulator/src/gps.cpp
  simulator/src/oracle.cpp
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

set(BLOCKER_PEELING_OUTPUT_DIR "${CMAKE_BINARY_DIR}/blocker_peeling")
set(BLOCKER_PEELING_GENERATOR
  "${CMAKE_CURRENT_SOURCE_DIR}/simulator/blocker_peeling/generate_variants.py"
)
set(BLOCKER_PEELING_CATALOGS
  "${CMAKE_CURRENT_SOURCE_DIR}/simulator/blocker_peeling/repair_catalog.yaml"
  "${CMAKE_CURRENT_SOURCE_DIR}/simulator/blocker_peeling/dependency_graph.yaml"
  "${CMAKE_CURRENT_SOURCE_DIR}/simulator/blocker_peeling/scenario_catalog.yaml"
)
set(BLOCKER_PEELING_VARIANTS
  P0
  P1
  P1_without_FW_EXPANDER_BEGIN
  P2
  P2_without_PCB_I2C_SDA_PULLUP
  P3
  P3_without_FW_EXPANDER_BEGIN
  P3_without_PCB_I2C_SDA_PULLUP
  P3_without_PCB_UART_CROSS
  P3_without_FW_ACCEPT_GNRMC
  P3_without_FW_NMEA_BOUNDS
  P3_rmc_status_only
  P3_rmc_checksum_only
  P4
  P4_without_DRIVER_PROBE_DIRECTIONS
  P4_without_FW_64BIT_MASKS
  P4_output_mask_only
  P5
  P5_without_FW_HARNESS_MAPPING
  P6
  P6_without_FW_ALL_ROWS_VERDICT
  P6_without_FW_EXPECTED_MATRIX_CLOSURE
  P7
  P7_without_FW_BUTTON_SESSION_EDGE
  P8
  P8_without_FW_LED_OUTPUT_MODES
  P8_without_BOARD_LED_POWER_AND_RESISTORS
  P8_without_BOARD_LED_COLOR_MAPPING
  P8_without_FW_RESULT_STATUS_PERSISTENCE
  P9
  P9_without_FW_SD_WRITE_CHECKS
  P9_RMC_VALIDATED
  P9_RMC_VALIDATED_without_FW_RMC_VALIDATE_ATOMIC
)

set(BLOCKER_PEELING_BYPRODUCTS
  "${BLOCKER_PEELING_OUTPUT_DIR}/variant_manifest.jsonl"
)
foreach(variant IN LISTS BLOCKER_PEELING_VARIANTS)
  list(APPEND BLOCKER_PEELING_BYPRODUCTS
    "${BLOCKER_PEELING_OUTPUT_DIR}/variants/${variant}/firmware/firmware.ino"
    "${BLOCKER_PEELING_OUTPUT_DIR}/variants/${variant}/firmware/CY8C9560.cpp"
    "${BLOCKER_PEELING_OUTPUT_DIR}/variants/${variant}/kicad_files/hardware_challenge.kicad_pcb"
    "${BLOCKER_PEELING_OUTPUT_DIR}/variants/${variant}/simulator/blocker_peeling/board_electrical_overlay.fixture"
    "${BLOCKER_PEELING_OUTPUT_DIR}/variants/${variant}/variant.generated.json"
  )
endforeach()

add_custom_command(
  OUTPUT "${BLOCKER_PEELING_OUTPUT_DIR}/.generated.stamp"
  COMMAND
    "${Python3_EXECUTABLE}" "${BLOCKER_PEELING_GENERATOR}"
    --root "${CMAKE_CURRENT_SOURCE_DIR}"
    --output "${BLOCKER_PEELING_OUTPUT_DIR}"
  DEPENDS
    "${BLOCKER_PEELING_GENERATOR}"
    ${BLOCKER_PEELING_CATALOGS}
    "${CMAKE_CURRENT_SOURCE_DIR}/firmware/firmware.ino"
    "${CMAKE_CURRENT_SOURCE_DIR}/firmware/CY8C9560.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/firmware/CY8C9560.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/kicad_files/hardware_challenge.kicad_pcb"
    "${CMAKE_CURRENT_SOURCE_DIR}/kicad_files/hardware_challenge.kicad_sch"
    "${CMAKE_CURRENT_SOURCE_DIR}/simulator/blocker_peeling/board_electrical_overlay.fixture"
  BYPRODUCTS ${BLOCKER_PEELING_BYPRODUCTS}
  VERBATIM
)
add_custom_target(generate_blocker_peeling_variants
  DEPENDS "${BLOCKER_PEELING_OUTPUT_DIR}/.generated.stamp"
)

function(add_blocker_peeling_case target variant)
  set(firmware_source
    "${BLOCKER_PEELING_OUTPUT_DIR}/variants/${variant}/firmware/firmware.ino"
  )
  set(driver_source
    "${BLOCKER_PEELING_OUTPUT_DIR}/variants/${variant}/firmware/CY8C9560.cpp"
  )
  set_source_files_properties(${firmware_source} PROPERTIES
    GENERATED TRUE
    LANGUAGE CXX
    COMPILE_OPTIONS "-x;c++"
  )
  set_source_files_properties(${driver_source} PROPERTIES GENERATED TRUE)
  add_executable(${target}
    simulator/tests/blocker_peeling_cases.cpp
    ${firmware_source}
    ${driver_source}
  )
  add_dependencies(${target} generate_blocker_peeling_variants)
  target_link_libraries(${target} PRIVATE host_simulator)
  target_compile_definitions(${target} PRIVATE
    HOST_SIM_ROOT="${CMAKE_CURRENT_SOURCE_DIR}"
    BLOCKER_VARIANT_DIR="${BLOCKER_PEELING_OUTPUT_DIR}/variants/${variant}"
    BLOCKER_VARIANT_ID="${variant}"
  )
  target_compile_options(${target} PRIVATE
    -Wall
    -Wextra
    -Wpedantic
  )
endfunction()

add_blocker_peeling_case(blocker_p0 P0)
add_blocker_peeling_case(blocker_p1 P1)
add_blocker_peeling_case(
  blocker_p1_without_begin P1_without_FW_EXPANDER_BEGIN)
add_blocker_peeling_case(blocker_p2 P2)
add_blocker_peeling_case(
  blocker_p2_without_pullup P2_without_PCB_I2C_SDA_PULLUP)
add_blocker_peeling_case(blocker_p3 P3)
add_blocker_peeling_case(
  blocker_p3_without_begin P3_without_FW_EXPANDER_BEGIN)
add_blocker_peeling_case(
  blocker_p3_without_pullup P3_without_PCB_I2C_SDA_PULLUP)
add_blocker_peeling_case(
  blocker_p3_without_uart P3_without_PCB_UART_CROSS)
add_blocker_peeling_case(
  blocker_p3_without_gnrmc P3_without_FW_ACCEPT_GNRMC)

function(add_blocker_peeling_workflow_case target variant)
  set(firmware_source
    "${BLOCKER_PEELING_OUTPUT_DIR}/variants/${variant}/firmware/firmware.ino"
  )
  set(driver_source
    "${BLOCKER_PEELING_OUTPUT_DIR}/variants/${variant}/firmware/CY8C9560.cpp"
  )
  set_source_files_properties(${firmware_source} PROPERTIES
    GENERATED TRUE
    LANGUAGE CXX
    COMPILE_OPTIONS "-x;c++"
  )
  set_source_files_properties(${driver_source} PROPERTIES GENERATED TRUE)
  add_executable(${target}
    simulator/tests/blocker_peeling_workflow_cases.cpp
    ${firmware_source}
    ${driver_source}
  )
  add_dependencies(${target} generate_blocker_peeling_variants)
  target_link_libraries(${target} PRIVATE host_simulator)
  target_compile_definitions(${target} PRIVATE
    HOST_SIM_ROOT="${CMAKE_CURRENT_SOURCE_DIR}"
    BLOCKER_VARIANT_DIR="${BLOCKER_PEELING_OUTPUT_DIR}/variants/${variant}"
    BLOCKER_VARIANT_ID="${variant}"
  )
  target_compile_options(${target} PRIVATE
    -Wall
    -Wextra
    -Wpedantic
  )
endfunction()

add_blocker_peeling_workflow_case(blocker_p4 P4)
add_blocker_peeling_workflow_case(
  blocker_p4_without_directions P4_without_DRIVER_PROBE_DIRECTIONS)
add_blocker_peeling_workflow_case(blocker_p5 P5)
add_blocker_peeling_workflow_case(
  blocker_p5_without_mapping P5_without_FW_HARNESS_MAPPING)
add_blocker_peeling_workflow_case(blocker_p6 P6)
add_blocker_peeling_workflow_case(
  blocker_p6_without_verdict P6_without_FW_ALL_ROWS_VERDICT)
add_blocker_peeling_workflow_case(
  blocker_p6_without_matrix P6_without_FW_EXPECTED_MATRIX_CLOSURE)
add_blocker_peeling_workflow_case(blocker_p7 P7)
add_blocker_peeling_workflow_case(
  blocker_p7_without_button P7_without_FW_BUTTON_SESSION_EDGE)
add_blocker_peeling_workflow_case(blocker_p9 P9)
add_blocker_peeling_workflow_case(
  blocker_p9_without_sd_checks P9_without_FW_SD_WRITE_CHECKS)

set(blocker_p9_campaign_firmware
  "${BLOCKER_PEELING_OUTPUT_DIR}/variants/P9/firmware/firmware.ino"
)
set(blocker_p9_campaign_driver
  "${BLOCKER_PEELING_OUTPUT_DIR}/variants/P9/firmware/CY8C9560.cpp"
)
set_source_files_properties(${blocker_p9_campaign_firmware} PROPERTIES
  GENERATED TRUE
  LANGUAGE CXX
  COMPILE_OPTIONS "-x;c++"
)
set_source_files_properties(${blocker_p9_campaign_driver}
  PROPERTIES GENERATED TRUE)
add_executable(blocker_p9_campaign
  simulator/tests/blocker_peeling_campaign.cpp
  ${blocker_p9_campaign_firmware}
  ${blocker_p9_campaign_driver}
)
add_dependencies(blocker_p9_campaign generate_blocker_peeling_variants)
target_link_libraries(blocker_p9_campaign PRIVATE host_simulator)
target_compile_definitions(blocker_p9_campaign PRIVATE
  HOST_SIM_ROOT="${CMAKE_CURRENT_SOURCE_DIR}"
  BLOCKER_VARIANT_DIR="${BLOCKER_PEELING_OUTPUT_DIR}/variants/P9"
)
target_compile_options(blocker_p9_campaign PRIVATE
  -Wall
  -Wextra
  -Wpedantic
)

function(add_blocker_peeling_event_campaign target variant rmc_validated)
  set(firmware_source
    "${BLOCKER_PEELING_OUTPUT_DIR}/variants/${variant}/firmware/firmware.ino"
  )
  set(driver_source
    "${BLOCKER_PEELING_OUTPUT_DIR}/variants/${variant}/firmware/CY8C9560.cpp"
  )
  set_source_files_properties(${firmware_source} PROPERTIES
    GENERATED TRUE
    LANGUAGE CXX
    COMPILE_OPTIONS "-x;c++"
  )
  set_source_files_properties(${driver_source} PROPERTIES GENERATED TRUE)
  add_executable(${target}
    simulator/tests/blocker_peeling_event_campaign.cpp
    ${firmware_source}
    ${driver_source}
  )
  add_dependencies(${target} generate_blocker_peeling_variants)
  target_link_libraries(${target} PRIVATE host_simulator)
  target_compile_definitions(${target} PRIVATE
    HOST_SIM_ROOT="${CMAKE_CURRENT_SOURCE_DIR}"
    BLOCKER_VARIANT_DIR="${BLOCKER_PEELING_OUTPUT_DIR}/variants/${variant}"
    BLOCKER_VARIANT_ID="${variant}"
    BLOCKER_RMC_VALIDATED=${rmc_validated}
  )
  target_compile_options(${target} PRIVATE
    -Wall
    -Wextra
    -Wpedantic
  )
endfunction()

add_blocker_peeling_event_campaign(
  blocker_p9_rmc_validated_campaign P9_RMC_VALIDATED 1)
add_blocker_peeling_event_campaign(
  blocker_p9_rmc_counterfactual_campaign
  P9_RMC_VALIDATED_without_FW_RMC_VALIDATE_ATOMIC 0)

function(add_blocker_peeling_persisted_corpus target sanitizer)
  set(variant P9_RMC_VALIDATED)
  set(firmware_source
    "${BLOCKER_PEELING_OUTPUT_DIR}/variants/${variant}/firmware/firmware.ino"
  )
  set(driver_source
    "${BLOCKER_PEELING_OUTPUT_DIR}/variants/${variant}/firmware/CY8C9560.cpp"
  )
  set_source_files_properties(${firmware_source} PROPERTIES
    GENERATED TRUE
    LANGUAGE CXX
    COMPILE_OPTIONS "-x;c++"
  )
  set_source_files_properties(${driver_source} PROPERTIES GENERATED TRUE)
  add_executable(${target}
    simulator/tests/blocker_peeling_persisted_corpus.cpp
    ${firmware_source}
    ${driver_source}
  )
  add_dependencies(${target} generate_blocker_peeling_variants)
  target_link_libraries(${target} PRIVATE host_simulator)
  target_compile_definitions(${target} PRIVATE
    HOST_SIM_ROOT="${CMAKE_CURRENT_SOURCE_DIR}"
    BLOCKER_VARIANT_DIR="${BLOCKER_PEELING_OUTPUT_DIR}/variants/${variant}"
  )
  target_compile_options(${target} PRIVATE
    -Wall
    -Wextra
    -Wpedantic
  )
  if(NOT sanitizer STREQUAL "none")
    target_compile_options(${target} PRIVATE
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
  endif()
endfunction()

add_blocker_peeling_persisted_corpus(
  blocker_p9_rmc_persisted_corpus none)

function(add_rmc_split_case target variant)
  set(firmware_source
    "${BLOCKER_PEELING_OUTPUT_DIR}/variants/${variant}/firmware/firmware.ino"
  )
  set(driver_source
    "${BLOCKER_PEELING_OUTPUT_DIR}/variants/${variant}/firmware/CY8C9560.cpp"
  )
  set_source_files_properties(${firmware_source} PROPERTIES
    GENERATED TRUE
    LANGUAGE CXX
    COMPILE_OPTIONS "-x;c++"
  )
  set_source_files_properties(${driver_source} PROPERTIES GENERATED TRUE)
  add_executable(${target}
    simulator/tests/rmc_split_cases.cpp
    ${firmware_source}
    ${driver_source}
  )
  add_dependencies(${target} generate_blocker_peeling_variants)
  target_link_libraries(${target} PRIVATE host_simulator)
  target_compile_definitions(${target} PRIVATE
    HOST_SIM_ROOT="${CMAKE_CURRENT_SOURCE_DIR}"
  )
  target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
endfunction()

add_rmc_split_case(rmc_status_only_cases P3_rmc_status_only)
add_rmc_split_case(rmc_checksum_only_cases P3_rmc_checksum_only)
add_test(NAME experimental_rmc_status_only
  COMMAND rmc_status_only_cases status-only)
add_test(NAME experimental_rmc_checksum_only
  COMMAND rmc_checksum_only_cases checksum-only)

set(blocker_p9_metamorphic_firmware
  "${BLOCKER_PEELING_OUTPUT_DIR}/variants/P9_RMC_VALIDATED/firmware/firmware.ino"
)
set(blocker_p9_metamorphic_driver
  "${BLOCKER_PEELING_OUTPUT_DIR}/variants/P9_RMC_VALIDATED/firmware/CY8C9560.cpp"
)
set_source_files_properties(${blocker_p9_metamorphic_firmware} PROPERTIES
  GENERATED TRUE
  LANGUAGE CXX
  COMPILE_OPTIONS "-x;c++"
)
set_source_files_properties(${blocker_p9_metamorphic_driver}
  PROPERTIES GENERATED TRUE)
add_executable(blocker_p9_metamorphic_campaign
  simulator/tests/blocker_peeling_metamorphic_campaign.cpp
  ${blocker_p9_metamorphic_firmware}
  ${blocker_p9_metamorphic_driver}
)
add_dependencies(
  blocker_p9_metamorphic_campaign generate_blocker_peeling_variants)
target_link_libraries(blocker_p9_metamorphic_campaign PRIVATE host_simulator)
target_compile_definitions(blocker_p9_metamorphic_campaign PRIVATE
  HOST_SIM_ROOT="${CMAKE_CURRENT_SOURCE_DIR}"
  BLOCKER_VARIANT_DIR="${BLOCKER_PEELING_OUTPUT_DIR}/variants/P9_RMC_VALIDATED"
)
target_compile_options(blocker_p9_metamorphic_campaign PRIVATE
  -Wall
  -Wextra
  -Wpedantic
)

function(add_blocker_peeling_led_case target variant)
  set(firmware_source
    "${BLOCKER_PEELING_OUTPUT_DIR}/variants/${variant}/firmware/firmware.ino"
  )
  set(driver_source
    "${BLOCKER_PEELING_OUTPUT_DIR}/variants/${variant}/firmware/CY8C9560.cpp"
  )
  set_source_files_properties(${firmware_source} PROPERTIES
    GENERATED TRUE
    LANGUAGE CXX
    COMPILE_OPTIONS "-x;c++"
  )
  set_source_files_properties(${driver_source} PROPERTIES GENERATED TRUE)
  add_executable(${target}
    simulator/tests/blocker_peeling_led_cases.cpp
    ${firmware_source}
    ${driver_source}
  )
  add_dependencies(${target} generate_blocker_peeling_variants)
  target_link_libraries(${target} PRIVATE host_simulator)
  target_compile_definitions(${target} PRIVATE
    HOST_SIM_ROOT="${CMAKE_CURRENT_SOURCE_DIR}"
    HOST_SIM_NGSPICE="${NGSPICE_EXECUTABLE}"
    BLOCKER_VARIANT_DIR="${BLOCKER_PEELING_OUTPUT_DIR}/variants/${variant}"
    BLOCKER_VARIANT_ID="${variant}"
  )
  target_compile_options(${target} PRIVATE
    -Wall
    -Wextra
    -Wpedantic
  )
endfunction()

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
  add_blocker_peeling_persisted_corpus(
    blocker_p9_rmc_persisted_corpus_asan address)
  message(STATUS "AddressSanitizer firmware witness: enabled")
else()
  message(STATUS "AddressSanitizer firmware witness: unavailable")
endif()
if(HOST_SIM_HAS_UBSAN)
  add_firmware_sanitizer_case(firmware_ubsan_cases undefined)
  add_blocker_peeling_persisted_corpus(
    blocker_p9_rmc_persisted_corpus_ubsan undefined)
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

add_executable(oracle_cases
  simulator/tests/oracle_cases.cpp
)
target_link_libraries(oracle_cases PRIVATE host_simulator)
target_compile_options(oracle_cases PRIVATE
  -Wall
  -Wextra
  -Wpedantic
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

add_executable(gps_cases
  simulator/tests/gps_cases.cpp
)
target_link_libraries(gps_cases PRIVATE host_simulator)
target_compile_definitions(gps_cases PRIVATE
  HOST_SIM_ROOT="${CMAKE_CURRENT_SOURCE_DIR}"
)
target_compile_options(gps_cases PRIVATE
  -Wall
  -Wextra
  -Wpedantic
)

add_executable(sd_cases
  simulator/tests/sd_cases.cpp
)
target_link_libraries(sd_cases PRIVATE host_simulator)
target_compile_definitions(sd_cases PRIVATE
  HOST_SIM_ROOT="${CMAKE_CURRENT_SOURCE_DIR}"
)
target_compile_options(sd_cases PRIVATE
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

add_blocker_peeling_led_case(blocker_p8 P8)
add_blocker_peeling_led_case(
  blocker_p8_without_modes P8_without_FW_LED_OUTPUT_MODES)
add_blocker_peeling_led_case(
  blocker_p8_without_power P8_without_BOARD_LED_POWER_AND_RESISTORS)
add_blocker_peeling_led_case(
  blocker_p8_without_mapping P8_without_BOARD_LED_COLOR_MAPPING)
add_blocker_peeling_led_case(
  blocker_p8_without_status P8_without_FW_RESULT_STATUS_PERSISTENCE)

enable_testing()
add_test(NAME pcb_source_parity COMMAND board_model_cases)
add_test(NAME runtime_defaults COMMAND runtime_cases)
add_test(NAME blocker_peeling_oracle COMMAND oracle_cases)
add_test(
  NAME blocker_peeling_generator_integrity
  COMMAND
    "${Python3_EXECUTABLE}"
    "${CMAKE_CURRENT_SOURCE_DIR}/simulator/blocker_peeling/test_generator.py"
    --root "${CMAKE_CURRENT_SOURCE_DIR}"
)
add_test(
  NAME blocker_peeling_manifest_verify
  COMMAND
    "${Python3_EXECUTABLE}" "${BLOCKER_PEELING_GENERATOR}"
    --root "${CMAKE_CURRENT_SOURCE_DIR}"
    --output "${BLOCKER_PEELING_OUTPUT_DIR}"
    --verify
)
add_test(NAME blocker_peeling_p0 COMMAND blocker_p0)
add_test(NAME blocker_peeling_p1 COMMAND blocker_p1)
add_test(
  NAME blocker_peeling_p1_without_begin
  COMMAND blocker_p1_without_begin
)
add_test(NAME blocker_peeling_p2 COMMAND blocker_p2)
add_test(
  NAME blocker_peeling_p2_without_pullup
  COMMAND blocker_p2_without_pullup
)
add_test(NAME blocker_peeling_p3 COMMAND blocker_p3)
add_test(
  NAME blocker_peeling_p3_without_begin
  COMMAND blocker_p3_without_begin
)
add_test(
  NAME blocker_peeling_p3_without_pullup
  COMMAND blocker_p3_without_pullup
)
add_test(
  NAME blocker_peeling_p3_without_uart
  COMMAND blocker_p3_without_uart
)
add_test(
  NAME blocker_peeling_p3_without_gnrmc
  COMMAND blocker_p3_without_gnrmc
)
add_test(NAME blocker_peeling_p4
  COMMAND blocker_p4 probe-repaired)
add_test(NAME blocker_peeling_p4_without_directions
  COMMAND blocker_p4_without_directions probe-counterfactual)
add_test(NAME blocker_peeling_p4_set_output
  COMMAND blocker_p4 set-output-repaired)
add_test(NAME blocker_peeling_p4_set_output_without_directions
  COMMAND blocker_p4_without_directions set-output-counterfactual)
add_test(NAME blocker_peeling_p5
  COMMAND blocker_p5 mapping-repaired)
add_test(NAME blocker_peeling_p5_without_mapping
  COMMAND blocker_p5_without_mapping mapping-counterfactual)
add_test(NAME blocker_peeling_p6_rejects_one_row
  COMMAND blocker_p6 verdict-one-repaired)
add_test(NAME blocker_peeling_p6_accepts_all_rows
  COMMAND blocker_p6 verdict-all-repaired)
add_test(NAME blocker_peeling_p6_without_verdict
  COMMAND blocker_p6_without_verdict verdict-one-counterfactual)
add_test(NAME blocker_peeling_p6_without_verdict_accepts_all_rows
  COMMAND blocker_p6_without_verdict verdict-all-repaired)
add_test(NAME blocker_peeling_p6_matrix_closure
  COMMAND blocker_p6 matrix-closure-repaired)
add_test(NAME blocker_peeling_p6_without_matrix_closure
  COMMAND blocker_p6_without_matrix matrix-closure-counterfactual)
add_test(NAME blocker_peeling_p7
  COMMAND blocker_p7 button-repaired)
add_test(NAME blocker_peeling_p7_without_button
  COMMAND blocker_p7_without_button button-counterfactual)
add_test(NAME blocker_peeling_p8
  COMMAND blocker_p8 repaired)
set_tests_properties(blocker_peeling_p8 PROPERTIES SKIP_RETURN_CODE 77)
add_test(NAME blocker_peeling_p8_without_modes
  COMMAND blocker_p8_without_modes without-modes)
set_tests_properties(blocker_peeling_p8_without_modes
  PROPERTIES SKIP_RETURN_CODE 77)
add_test(NAME blocker_peeling_p8_without_power
  COMMAND blocker_p8_without_power without-power)
set_tests_properties(blocker_peeling_p8_without_power
  PROPERTIES SKIP_RETURN_CODE 77)
add_test(NAME blocker_peeling_p8_without_mapping
  COMMAND blocker_p8_without_mapping without-mapping)
set_tests_properties(blocker_peeling_p8_without_mapping
  PROPERTIES SKIP_RETURN_CODE 77)
add_test(NAME blocker_peeling_p8_status_persistence
  COMMAND blocker_p8 status-persistence)
set_tests_properties(blocker_peeling_p8_status_persistence
  PROPERTIES SKIP_RETURN_CODE 77)
add_test(NAME blocker_peeling_p8_without_status_persistence
  COMMAND blocker_p8_without_status without-status-persistence)
set_tests_properties(blocker_peeling_p8_without_status_persistence
  PROPERTIES SKIP_RETURN_CODE 77)
add_test(NAME blocker_peeling_p9
  COMMAND blocker_p9 sd-repaired)
add_test(NAME blocker_peeling_p9_line_ending_short
  COMMAND blocker_p9 sd-line-ending-repaired)
add_test(NAME blocker_peeling_p9_full_record
  COMMAND blocker_p9 sd-full-repaired)
add_test(NAME production_sd_open_result_divergence
  COMMAND blocker_p9 sd-open-result-divergence)
add_test(NAME production_scan_uart_loss
  COMMAND blocker_p9 scan-uart-loss)
add_test(NAME production_alternate_profile_fixed_oracle
  COMMAND blocker_p9 alternate-profile-fixed-oracle)
add_test(NAME production_alternate_profile_retargeted_oracle
  COMMAND blocker_p9 alternate-profile-retargeted-oracle)
add_test(NAME blocker_peeling_p9_without_sd_checks
  COMMAND blocker_p9_without_sd_checks sd-counterfactual)
add_test(NAME blocker_peeling_harness_campaign
  COMMAND blocker_p9_campaign
    "${BLOCKER_PEELING_OUTPUT_DIR}/exposure_matrix.csv")
add_test(NAME blocker_peeling_peripheral_event_campaign
  COMMAND
    "${Python3_EXECUTABLE}"
    "${CMAKE_CURRENT_SOURCE_DIR}/simulator/blocker_peeling/test_event_campaign.py"
    --root "${CMAKE_CURRENT_SOURCE_DIR}"
    --repaired "$<TARGET_FILE:blocker_p9_rmc_validated_campaign>"
    --counterfactual "$<TARGET_FILE:blocker_p9_rmc_counterfactual_campaign>"
    --output
      "${BLOCKER_PEELING_OUTPUT_DIR}/peripheral_exposure_matrix.csv")
set(BLOCKER_PEELING_PERSISTED_CORPUS
  "${CMAKE_CURRENT_SOURCE_DIR}/simulator/blocker_peeling/corpus/nmea_peripheral_regression.json"
)
set(BLOCKER_PEELING_PERSISTED_CORPUS_GENERATOR
  "${CMAKE_CURRENT_SOURCE_DIR}/simulator/blocker_peeling/generate_persisted_corpus.py"
)
set(BLOCKER_PEELING_PERSISTED_CORPUS_TEST
  "${CMAKE_CURRENT_SOURCE_DIR}/simulator/blocker_peeling/test_persisted_corpus.py"
)
add_test(NAME blocker_peeling_persisted_corpus_campaign
  COMMAND
    "${Python3_EXECUTABLE}" "${BLOCKER_PEELING_PERSISTED_CORPUS_TEST}"
    --root "${CMAKE_CURRENT_SOURCE_DIR}"
    --corpus "${BLOCKER_PEELING_PERSISTED_CORPUS}"
    --generator "${BLOCKER_PEELING_PERSISTED_CORPUS_GENERATOR}"
    --runner-label baseline
    --runner "$<TARGET_FILE:blocker_p9_rmc_persisted_corpus>"
    --output "${BLOCKER_PEELING_OUTPUT_DIR}/persisted_corpus_matrix.csv")
if(HOST_SIM_HAS_ASAN)
  add_test(NAME blocker_peeling_persisted_corpus_asan
    COMMAND
      "${Python3_EXECUTABLE}" "${BLOCKER_PEELING_PERSISTED_CORPUS_TEST}"
      --root "${CMAKE_CURRENT_SOURCE_DIR}"
      --corpus "${BLOCKER_PEELING_PERSISTED_CORPUS}"
      --generator "${BLOCKER_PEELING_PERSISTED_CORPUS_GENERATOR}"
      --runner-label asan
      --runner "$<TARGET_FILE:blocker_p9_rmc_persisted_corpus_asan>"
      --output "${BLOCKER_PEELING_OUTPUT_DIR}/persisted_corpus_asan_matrix.csv")
else()
  add_test(NAME blocker_peeling_persisted_corpus_asan
    COMMAND "${Python3_EXECUTABLE}" -c
      "import sys; print('AddressSanitizer unavailable'); sys.exit(77)")
  set_tests_properties(blocker_peeling_persisted_corpus_asan
    PROPERTIES SKIP_RETURN_CODE 77)
endif()
if(HOST_SIM_HAS_UBSAN)
  add_test(NAME blocker_peeling_persisted_corpus_ubsan
    COMMAND
      "${Python3_EXECUTABLE}" "${BLOCKER_PEELING_PERSISTED_CORPUS_TEST}"
      --root "${CMAKE_CURRENT_SOURCE_DIR}"
      --corpus "${BLOCKER_PEELING_PERSISTED_CORPUS}"
      --generator "${BLOCKER_PEELING_PERSISTED_CORPUS_GENERATOR}"
      --runner-label ubsan
      --runner "$<TARGET_FILE:blocker_p9_rmc_persisted_corpus_ubsan>"
      --output "${BLOCKER_PEELING_OUTPUT_DIR}/persisted_corpus_ubsan_matrix.csv")
else()
  add_test(NAME blocker_peeling_persisted_corpus_ubsan
    COMMAND "${Python3_EXECUTABLE}" -c
      "import sys; print('UndefinedBehaviorSanitizer unavailable'); sys.exit(77)")
  set_tests_properties(blocker_peeling_persisted_corpus_ubsan
    PROPERTIES SKIP_RETURN_CODE 77)
endif()
add_test(NAME blocker_peeling_metamorphic_campaign_c004
  COMMAND
    "${Python3_EXECUTABLE}"
    "${CMAKE_CURRENT_SOURCE_DIR}/simulator/blocker_peeling/test_metamorphic_campaign.py"
    --root "${CMAKE_CURRENT_SOURCE_DIR}"
    --executable "$<TARGET_FILE:blocker_p9_metamorphic_campaign>"
    --output "${BLOCKER_PEELING_OUTPUT_DIR}/metamorphic_matrix_c004.csv"
    --seed 1296389185)
add_test(NAME blocker_peeling_metamorphic_campaign_c005
  COMMAND
    "${Python3_EXECUTABLE}"
    "${CMAKE_CURRENT_SOURCE_DIR}/simulator/blocker_peeling/test_metamorphic_campaign.py"
    --root "${CMAKE_CURRENT_SOURCE_DIR}"
    --executable "$<TARGET_FILE:blocker_p9_metamorphic_campaign>"
    --output "${BLOCKER_PEELING_OUTPUT_DIR}/metamorphic_matrix_c005.csv"
    --seed 1296389186
    --different-seed 1296389185)
add_test(NAME uart_timing_and_topology COMMAND uart_cases)
add_test(NAME button_and_reset_timing COMMAND timing_cases)
add_test(NAME i2c_timing_and_faults COMMAND i2c_cases)
add_test(NAME gps_startup_cadence_and_checksum COMMAND gps_cases)
add_test(NAME sd_timing_capacity_and_removal COMMAND sd_cases)
add_test(NAME scenario_a05_narrow_masks COMMAND scenario_cases a05_narrow_masks)
add_test(NAME scenario_a10_uart_same_direction COMMAND scenario_cases a10_uart_same_direction)
add_test(NAME scenario_a11_sda_pull_down COMMAND scenario_cases a11_sda_pull_down)
add_test(NAME scenario_a12_led_no_series_resistors COMMAND scenario_cases a12_led_no_series_resistors)
add_test(NAME scenario_a16_d3_anode_isolated COMMAND scenario_cases a16_d3_anode_isolated)
add_test(NAME scenario_a18_lna_vcc_rf COMMAND scenario_cases a18_lna_vcc_rf)
add_test(NAME scenario_a25_reverse_polarity_path COMMAND scenario_cases a25_reverse_polarity_path)
add_test(NAME scenario_a29_u4_wrong_footprint COMMAND scenario_cases a29_u4_wrong_footprint)
add_test(NAME scenario_c7_10u_0402_on_12v COMMAND scenario_cases c7_10u_0402_on_12v)
add_test(NAME scenario_max2679_rfin_missing_input_network
  COMMAND scenario_cases max2679_rfin_missing_input_network)
add_test(NAME scenario_max2679_rfout_series_inductor
  COMMAND scenario_cases max2679_rfout_series_inductor)
add_test(NAME scenario_antenna_feed_reference_void
  COMMAND scenario_cases antenna_feed_reference_void)
add_test(NAME scenario_antenna_patch_counterpoise_absent
  COMMAND scenario_cases antenna_patch_counterpoise_absent)
add_test(NAME scenario_teensy_vin_vusb_unisolated
  COMMAND scenario_cases teensy_vin_vusb_unisolated)
add_test(NAME scenario_neo_m8_safeboot_reserved_pin_connected
  COMMAND scenario_cases neo_m8_safeboot_reserved_pin_connected)
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
  source_derived_i2c
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
add_test(NAME bug_a03_uart_overrun_path
  COMMAND firmware_unmasked_bug_cases a03_uart_overrun_path)
add_test(NAME firmware_uart_polling_control
  COMMAND firmware_unmasked_bug_cases uart_polling_control)
add_test(NAME bug_a04_invalid_status_accepted COMMAND firmware_unmasked_bug_cases a04_invalid_status_accepted)
add_test(NAME bug_a04_checksum_ignored
  COMMAND firmware_unmasked_bug_cases a04_checksum_ignored)
add_test(NAME bug_post_lock_timestamp_tearing
  COMMAND firmware_original_bug_cases post_lock_timestamp_tearing)
add_test(NAME firmware_nmea_crlf_empty_pass
  COMMAND firmware_unmasked_bug_cases nmea_crlf_empty_pass)
add_test(NAME bug_sd_partial_log_accepted
  COMMAND firmware_original_bug_cases sd_partial_log_accepted)
add_test(NAME firmware_sd_removed_before_open
  COMMAND firmware_unmasked_bug_cases sd_removed_before_open)
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
  add_blocker_peeling_case(blocker_p3_asan P3)
  target_compile_options(blocker_p3_asan PRIVATE
    -fno-omit-frame-pointer
    -fsanitize=address
  )
  target_link_options(blocker_p3_asan PRIVATE
    -fno-omit-frame-pointer
    -fsanitize=address
  )
  add_test(NAME blocker_peeling_p3_asan
    COMMAND blocker_p3_asan normal-default-gps)
  add_blocker_peeling_case(
    blocker_p3_without_bounds_asan P3_without_FW_NMEA_BOUNDS)
  target_compile_options(blocker_p3_without_bounds_asan PRIVATE
    -fno-omit-frame-pointer
    -fsanitize=address
  )
  target_link_options(blocker_p3_without_bounds_asan PRIVATE
    -fno-omit-frame-pointer
    -fsanitize=address
  )
  add_test(NAME blocker_peeling_p3_without_bounds
    COMMAND ${CMAKE_COMMAND}
      -DPROGRAM=$<TARGET_FILE:blocker_p3_without_bounds_asan>
      -DCASE=normal-default-gps
      -DSANITIZER_ENV=ASAN_OPTIONS=halt_on_error=1:abort_on_error=1:detect_leaks=0
      -DEXPECTED_1=global-buffer-overflow
      -DEXPECTED_2=nmea_buf
      -P ${CMAKE_CURRENT_SOURCE_DIR}/simulator/tests/expect_sanitizer_failure.cmake)
  add_test(NAME sanitizer_a03_nmea_overflow
    COMMAND ${CMAKE_COMMAND}
      -DPROGRAM=$<TARGET_FILE:firmware_asan_cases>
      -DCASE=nmea-append-overflow
      -DSANITIZER_ENV=ASAN_OPTIONS=halt_on_error=1:abort_on_error=1:detect_leaks=0
      -DEXPECTED_1=global-buffer-overflow
      -DEXPECTED_2=nmea_buf
      -P ${CMAKE_CURRENT_SOURCE_DIR}/simulator/tests/expect_sanitizer_failure.cmake)
  add_test(NAME sanitizer_a03_nmea_terminator_overflow
    COMMAND ${CMAKE_COMMAND}
      -DPROGRAM=$<TARGET_FILE:firmware_asan_cases>
      -DCASE=nmea-terminator-overflow
      -DSANITIZER_ENV=ASAN_OPTIONS=halt_on_error=1:abort_on_error=1:detect_leaks=0
      -DEXPECTED_1=global-buffer-overflow
      -DEXPECTED_2=nmea_buf
      -P ${CMAKE_CURRENT_SOURCE_DIR}/simulator/tests/expect_sanitizer_failure.cmake)
  add_test(NAME sanitizer_a03_default_gps_sentence_overflow
    COMMAND ${CMAKE_COMMAND}
      -DPROGRAM=$<TARGET_FILE:firmware_asan_cases>
      -DCASE=default-gps-overflow
      -DSANITIZER_ENV=ASAN_OPTIONS=halt_on_error=1:abort_on_error=1:detect_leaks=0
      -DEXPECTED_1=global-buffer-overflow
      -DEXPECTED_2=nmea_buf
      -P ${CMAKE_CURRENT_SOURCE_DIR}/simulator/tests/expect_sanitizer_failure.cmake)
else()
  add_test(NAME blocker_peeling_p3_asan
    COMMAND "${Python3_EXECUTABLE}" -c
      "import sys; print('AddressSanitizer unavailable'); sys.exit(77)")
  set_tests_properties(blocker_peeling_p3_asan PROPERTIES SKIP_RETURN_CODE 77)
  add_test(NAME blocker_peeling_p3_without_bounds
    COMMAND "${Python3_EXECUTABLE}" -c
      "import sys; print('AddressSanitizer unavailable'); sys.exit(77)")
  set_tests_properties(blocker_peeling_p3_without_bounds
    PROPERTIES SKIP_RETURN_CODE 77)
  add_test(NAME sanitizer_a03_nmea_overflow
    COMMAND "${Python3_EXECUTABLE}" -c
      "import sys; print('AddressSanitizer unavailable'); sys.exit(77)")
  set_tests_properties(sanitizer_a03_nmea_overflow
    PROPERTIES SKIP_RETURN_CODE 77)
  add_test(NAME sanitizer_a03_nmea_terminator_overflow
    COMMAND "${Python3_EXECUTABLE}" -c
      "import sys; print('AddressSanitizer unavailable'); sys.exit(77)")
  set_tests_properties(sanitizer_a03_nmea_terminator_overflow
    PROPERTIES SKIP_RETURN_CODE 77)
  add_test(NAME sanitizer_a03_default_gps_sentence_overflow
    COMMAND "${Python3_EXECUTABLE}" -c
      "import sys; print('AddressSanitizer unavailable'); sys.exit(77)")
  set_tests_properties(sanitizer_a03_default_gps_sentence_overflow
    PROPERTIES SKIP_RETURN_CODE 77)
endif()
if(HOST_SIM_HAS_UBSAN)
  add_blocker_peeling_workflow_case(blocker_p4_ubsan P4)
  target_compile_options(blocker_p4_ubsan PRIVATE
    -fno-omit-frame-pointer
    -fsanitize=undefined
    -fno-sanitize-recover=undefined
  )
  target_link_options(blocker_p4_ubsan PRIVATE
    -fno-omit-frame-pointer
    -fsanitize=undefined
    -fno-sanitize-recover=undefined
  )
  add_test(NAME blocker_peeling_p4_ubsan
    COMMAND blocker_p4_ubsan probe-repaired)

  add_blocker_peeling_workflow_case(
    blocker_p4_without_masks_ubsan P4_without_FW_64BIT_MASKS)
  target_compile_options(blocker_p4_without_masks_ubsan PRIVATE
    -fno-omit-frame-pointer
    -fsanitize=undefined
    -fno-sanitize-recover=undefined
  )
  target_link_options(blocker_p4_without_masks_ubsan PRIVATE
    -fno-omit-frame-pointer
    -fsanitize=undefined
    -fno-sanitize-recover=undefined
  )
  add_test(NAME blocker_peeling_p4_without_masks
    COMMAND ${CMAKE_COMMAND}
      -DPROGRAM=$<TARGET_FILE:blocker_p4_without_masks_ubsan>
      -DCASE=probe-repaired
      -DSANITIZER_ENV=UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=0
      "-DEXPECTED_1=runtime error:"
      "-DEXPECTED_2=shift exponent 32"
      -P ${CMAKE_CURRENT_SOURCE_DIR}/simulator/tests/expect_sanitizer_failure.cmake)

  add_blocker_peeling_workflow_case(
    blocker_p4_output_mask_only_ubsan P4_output_mask_only)
  target_compile_options(blocker_p4_output_mask_only_ubsan PRIVATE
    -fno-omit-frame-pointer
    -fsanitize=undefined
    -fno-sanitize-recover=undefined
  )
  target_link_options(blocker_p4_output_mask_only_ubsan PRIVATE
    -fno-omit-frame-pointer
    -fsanitize=undefined
    -fno-sanitize-recover=undefined
  )
  add_test(NAME blocker_peeling_p4_debug_shift
    COMMAND ${CMAKE_COMMAND}
      -DPROGRAM=$<TARGET_FILE:blocker_p4_output_mask_only_ubsan>
      -DCASE=probe-repaired
      -DSANITIZER_ENV=UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=0
      "-DEXPECTED_1=runtime error:"
      "-DEXPECTED_2=shift exponent 32"
      -P ${CMAKE_CURRENT_SOURCE_DIR}/simulator/tests/expect_sanitizer_failure.cmake)

  add_test(NAME sanitizer_a05_narrow_shift
    COMMAND ${CMAKE_COMMAND}
      -DPROGRAM=$<TARGET_FILE:firmware_ubsan_cases>
      -DCASE=narrow-shift
      -DSANITIZER_ENV=UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=0
      "-DEXPECTED_1=runtime error:"
      -DEXPECTED_2=shift
      -P ${CMAKE_CURRENT_SOURCE_DIR}/simulator/tests/expect_sanitizer_failure.cmake)
else()
  add_test(NAME blocker_peeling_p4_ubsan
    COMMAND "${Python3_EXECUTABLE}" -c
      "import sys; print('UndefinedBehaviorSanitizer unavailable'); sys.exit(77)")
  set_tests_properties(blocker_peeling_p4_ubsan
    PROPERTIES SKIP_RETURN_CODE 77)
  add_test(NAME blocker_peeling_p4_without_masks
    COMMAND "${Python3_EXECUTABLE}" -c
      "import sys; print('UndefinedBehaviorSanitizer unavailable'); sys.exit(77)")
  set_tests_properties(blocker_peeling_p4_without_masks
    PROPERTIES SKIP_RETURN_CODE 77)
  add_test(NAME blocker_peeling_p4_debug_shift
    COMMAND "${Python3_EXECUTABLE}" -c
      "import sys; print('UndefinedBehaviorSanitizer unavailable'); sys.exit(77)")
  set_tests_properties(blocker_peeling_p4_debug_shift
    PROPERTIES SKIP_RETURN_CODE 77)
  add_test(NAME sanitizer_a05_narrow_shift
    COMMAND "${Python3_EXECUTABLE}" -c
      "import sys; print('UndefinedBehaviorSanitizer unavailable'); sys.exit(77)")
  set_tests_properties(sanitizer_a05_narrow_shift
    PROPERTIES SKIP_RETURN_CODE 77)
endif()
