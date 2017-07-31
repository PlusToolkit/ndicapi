PROJECT(ndicapi)
CMAKE_MINIMUM_REQUIRED(VERSION 3.6.0)

SET(ndicapi_VERSION 1.6.0)

# --------------------------------------------------------------------------
# Configure output paths for libraries and executables.
IF(NOT DEFINED CMAKE_RUNTIME_OUTPUT_DIRECTORY)
  SET(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
ENDIF()

IF(NOT DEFINED CMAKE_LIBRARY_OUTPUT_DIRECTORY)
  SET(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
ENDIF()

IF(NOT DEFINED CMAKE_ARCHIVE_OUTPUT_DIRECTORY)
  SET(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/share")
ENDIF()

IF(NOT DEFINED BUILD_SHARED_LIBS)
  SET(BUILD_SHARED_LIBS OFF)
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

IF(MSVC OR ${CMAKE_GENERATOR} MATCHES "Xcode")
  SET(${PROJECT_NAME}_HDRS
    ndicapi_math.h
    ndicapi_thread.h
    ndicapi_serial.h
    ndicapi.h
    ndicapi_socket.h
  )
ENDIF()

ADD_LIBRARY(${PROJECT_NAME} ${${PROJECT_NAME}_SRCS} ${${PROJECT_NAME}_HDRS})
IF(NOT WIN32)
  SET_TARGET_PROPERTIES(${PROJECT_NAME} PROPERTIES COMPILE_FLAGS "-fPIC")
ENDIF()
target_include_directories(${PROJECT_NAME} PUBLIC ${${PROJECT_NAME}_INCLUDE_DIRS})

IF(MSVC)
  target_link_libraries(${PROJECT_NAME} PUBLIC wsock32 ws2_32)
ELSE()

ENDIF()

INSTALL(TARGETS ${PROJECT_NAME} EXPORT ndicapi
  RUNTIME DESTINATION bin CONFIGURATIONS Release COMPONENT RuntimeLibraries
  LIBRARY DESTINATION lib CONFIGURATIONS Release COMPONENT RuntimeLibraries
  ARCHIVE DESTINATION lib CONFIGURATIONS Release COMPONENT Development
  )
INSTALL(FILES ${${PROJECT_NAME}_HDRS}
  DESTINATION include CONFIGURATIONS Release COMPONENT Development
  )

#-----------------------------------------------------------------------------
# CMake target details
SET(${PROJECT_NAME}_TARGETS_FILE "${CMAKE_BINARY_DIR}/ndicapiTargets.cmake")
SET(${PROJECT_NAME}_LIBRARIES ndicapi)
IF(BUILD_SHARED_LIBS)
  SET(CLEAR ${PROJECT_NAME}_STATIC)
ELSE()
  SET(${PROJECT_NAME}_STATIC ON)
ENDIF()

IF(ndicapi_BUILD_APPLICATIONS)
  ADD_EXECUTABLE(ndiBasicExample Applications/ndiBasicExample.cxx)
  TARGET_LINK_LIBRARIES(ndiBasicExample PUBLIC ndicapi)
  LIST(APPEND _targets ndiBasicExample)
ENDIF()

export(TARGETS ${_targets}
  FILE ${ndicapi_TARGETS_FILE}
  )

CONFIGURE_FILE(ndicapiConfig.cmake.in
  "${CMAKE_BINARY_DIR}/ndicapiConfig.cmake" @ONLY)

CONFIGURE_FILE(ndicapiConfigVersion.cmake.in
  "${CMAKE_BINARY_DIR}/ndicapiConfigVersion.cmake" @ONLY)

CONFIGURE_FILE(ndicapiExport.h.in
  "${CMAKE_BINARY_DIR}/ndicapiExport.h" @ONLY)

# Export the package for use from the build-tree
# (this registers the build-tree with a global CMake-registry)
export(PACKAGE ndicapi)