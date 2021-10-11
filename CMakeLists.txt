PROJECT(ndicapi)
CMAKE_MINIMUM_REQUIRED(VERSION 3.3.0)

SET(ndicapi_VERSION 1.6.0)

# --------------------------------------------------------------------------
# Configure output paths for libraries and executables.
IF(NOT DEFINED CMAKE_RUNTIME_OUTPUT_DIRECTORY)
  SET(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/bin")
ENDIF()

IF(NOT DEFINED CMAKE_LIBRARY_OUTPUT_DIRECTORY)
  SET(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/lib")
ENDIF()

IF(NOT DEFINED CMAKE_ARCHIVE_OUTPUT_DIRECTORY)
  SET(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/share")
ENDIF()

IF(NOT DEFINED BUILD_SHARED_LIBS)
  SET(BUILD_SHARED_LIBS ON)
ENDIF()
OPTION(BUILD_SHARED_LIBS "Build shared libraries" ${BUILD_SHARED_LIBS})

# --------------------------------------------------------------------------
# Export variables
SET(_targets ndicapi)

# --------------------------------------------------------------------------
# Configure options
OPTION(ndicapi_BUILD_APPLICATIONS "Build applications." OFF)

# --------------------------------------------------------------------------
# Configure library
SET(${PROJECT_NAME}_INCLUDE_DIRS
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_BINARY_DIR}
  )

SET(${PROJECT_NAME}_SRCS
  ndicapi.cxx
  ndicapi_math.cxx
  ndicapi_serial.cxx
  ndicapi_thread.cxx
  ndicapi_socket.cxx
  )

CONFIGURE_FILE(ndicapiExport.h.in "${CMAKE_CURRENT_BINARY_DIR}/ndicapiExport.h" @ONLY)

SET(${PROJECT_NAME}_HDRS
  ndicapi_math.h
  ndicapi_thread.h
  ndicapi_serial.h
  ndicapi.h
  ndicapi_socket.h
  ${CMAKE_CURRENT_BINARY_DIR}/ndicapiExport.h
  )

OPTION(BUILD_PYTHON "Ensure the python module will build." OFF)
IF(BUILD_PYTHON)
  IF(NOT BUILD_SHARED_LIBS)
    MESSAGE(FATAL_ERROR
      "The Python extension only works with shared libraries"
      " (DLLs on Windows). Please enable building as a"
      " shared library (CMake option BUILD_SHARED_LIBS)."
      )
  ENDIF()
  SET(SETUP_PY_FILEPATH ${CMAKE_CURRENT_SOURCE_DIR}/setup.py)
  CONFIGURE_FILE(
    ${CMAKE_CURRENT_SOURCE_DIR}/setup.py.in
    ${SETUP_PY_FILEPATH}
    )
  MESSAGE(
    "Successfully generated ${SETUP_PY_FILEPATH} for the Python"
    " extension module. To install the Python extension module:"
    "\n\t0) Install the C++ library: make && make install"
    " (might need admin access)"
    "\n\t1) Go to ${CMAKE_CURRENT_SOURCE_DIR}"
    "\n\t2) Build and install the Python extension:"
    " python setup.py install (might require admin access)"
    )
ENDIF()

FIND_PACKAGE(Threads)
LIST(APPEND ${PROJECT_NAME}_LIBS
  ${CMAKE_THREAD_LIBS_INIT}
  )

# macro for CXX_STANDARD
SET(NDICAPI_CXX_STANDARD 11)

ADD_LIBRARY(${PROJECT_NAME} ${${PROJECT_NAME}_SRCS} ${${PROJECT_NAME}_HDRS})
TARGET_LINK_LIBRARIES(${PROJECT_NAME} PUBLIC ${${PROJECT_NAME}_LIBS})
SET_PROPERTY(TARGET ${PROJECT_NAME} PROPERTY CXX_STANDARD ${NDICAPI_CXX_STANDARD})
IF(NOT WIN32)
  SET_TARGET_PROPERTIES(${PROJECT_NAME} PROPERTIES COMPILE_FLAGS "-fPIC")
ENDIF()
target_include_directories(${PROJECT_NAME} PUBLIC ${${PROJECT_NAME}_INCLUDE_DIRS})

IF(MSVC)
  target_link_libraries(${PROJECT_NAME} PUBLIC wsock32 ws2_32)
ENDIF()

INSTALL(TARGETS ${PROJECT_NAME} EXPORT ndicapi
  RUNTIME DESTINATION bin COMPONENT RuntimeLibraries
  LIBRARY DESTINATION lib COMPONENT RuntimeLibraries
  ARCHIVE DESTINATION lib COMPONENT Development
  )
INSTALL(FILES ${${PROJECT_NAME}_HDRS}
  DESTINATION include COMPONENT Development
  )
GET_TARGET_PROPERTY(_library_type ndicapi TYPE)
IF(${_library_type} STREQUAL SHARED_LIBRARY AND MSVC)
  INSTALL(FILES "$<TARGET_PDB_FILE:ndicapi>" OPTIONAL
    DESTINATION "bin" COMPONENT RuntimeLibraries
    )
ENDIF()

#-----------------------------------------------------------------------------
# CMake target details
SET(${PROJECT_NAME}_TARGETS_FILE "${CMAKE_CURRENT_BINARY_DIR}/ndicapiTargets.cmake")
SET(${PROJECT_NAME}_LIBRARIES ndicapi)
IF(BUILD_SHARED_LIBS)
  SET(CLEAR ${PROJECT_NAME}_STATIC)
ELSE()
  SET(${PROJECT_NAME}_STATIC ON)
ENDIF()

IF(ndicapi_BUILD_APPLICATIONS)
  ADD_EXECUTABLE(ndiBasicExample Applications/ndiBasicExample.cxx)
  TARGET_LINK_LIBRARIES(ndiBasicExample PUBLIC ndicapi)
  SET_PROPERTY(TARGET ndiBasicExample PROPERTY CXX_STANDARD ${NDICAPI_CXX_STANDARD})
  LIST(APPEND _targets ndiBasicExample)
ENDIF()

export(TARGETS ${_targets}
  FILE ${ndicapi_TARGETS_FILE}
  )

CONFIGURE_FILE(ndicapiConfig.cmake.in
  "${CMAKE_CURRENT_BINARY_DIR}/ndicapiConfig.cmake" @ONLY)

CONFIGURE_FILE(ndicapiConfigVersion.cmake.in
  "${CMAKE_CURRENT_BINARY_DIR}/ndicapiConfigVersion.cmake" @ONLY)

# Enable building of python module, configure the export file in the source directory as well
CONFIGURE_FILE(ndicapiExport.h.in
  "${CMAKE_CURRENT_SOURCE_DIR}/ndicapiExport.h" @ONLY)

# Export the package for use from the build-tree
# (this registers the build-tree with a global CMake-registry)
export(PACKAGE ndicapi)
