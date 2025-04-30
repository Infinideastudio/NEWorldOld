string (TOUPPER ${PROJECT_NAME} PREFIX)

# All options provided through this file.
option (${PREFIX}_ENABLE_CODE_ANALYSIS "Run code analysis with cppcheck" OFF)

# See: https://stackoverflow.com/questions/56957172/cmake-conditionally-run-cppcheck
if (${PREFIX}_ENABLE_CODE_ANALYSIS)
  find_program (cppcheck cppcheck)
  if (cppcheck MATCHES "NOTFOUND")
    message (STATUS "Cppcheck executable not found.")
  else ()
    message (STATUS "Cppcheck executable found at ${cppcheck}.")
    # Enable cppcheck.
    set (CMAKE_CXX_CPPCHECK "${cppcheck}"
      "--enable=all"
      "--inconclusive"
      "--inline-suppr"
      "--quiet"
      "--suppress=unmatchedSuppression"
      "--suppress=unusedFunction"
      "--template='{file}:{line}: warning: {id} ({severity}): {message}'")
  endif ()
endif ()
