# SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
# SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.0

# Artifact probes validate producer output, not native runtime behavior. Read
# bytes as hexadecimal text so this check handles SQLite headers, NUL bytes,
# and ordinary text dumps without depending on an instrumented helper process.
if(NOT DEFINED INPUT OR NOT DEFINED EXPECTED_HEX)
  message(FATAL_ERROR "INPUT and EXPECTED_HEX are required")
endif()
if(NOT EXISTS "${INPUT}")
  message(FATAL_ERROR "Artifact does not exist: ${INPUT}")
endif()

file(READ "${INPUT}" artifact_hex HEX)
string(TOLOWER "${EXPECTED_HEX}" expected_hex)
string(FIND "${artifact_hex}" "${expected_hex}" expected_offset)
if(expected_offset EQUAL -1)
  message(FATAL_ERROR
    "Artifact does not contain the expected byte sequence: ${INPUT}"
  )
endif()
