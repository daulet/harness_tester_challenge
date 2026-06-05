foreach(required PROGRAM CASE SANITIZER_ENV EXPECTED_1 EXPECTED_2)
  if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
    message(FATAL_ERROR "missing required argument: ${required}")
  endif()
endforeach()

execute_process(
  COMMAND "${CMAKE_COMMAND}" -E env "${SANITIZER_ENV}" "${PROGRAM}" "${CASE}"
  RESULT_VARIABLE result
  OUTPUT_VARIABLE stdout
  ERROR_VARIABLE stderr
)

set(output "${stdout}\n${stderr}")
if("${result}" STREQUAL "0")
  message(FATAL_ERROR
    "sanitizer case exited successfully instead of detecting a defect:\n${output}")
endif()
string(FIND "${output}" "${EXPECTED_1}" expected_1_offset)
if(expected_1_offset EQUAL -1)
  message(FATAL_ERROR
    "sanitizer output did not contain '${EXPECTED_1}':\n${output}")
endif()
string(FIND "${output}" "${EXPECTED_2}" expected_2_offset)
if(expected_2_offset EQUAL -1)
  message(FATAL_ERROR
    "sanitizer output did not contain '${EXPECTED_2}':\n${output}")
endif()

message(STATUS
  "sanitizer case '${CASE}' failed with the expected diagnostics")
