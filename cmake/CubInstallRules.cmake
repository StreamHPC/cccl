# Thrust manages its own copy of these rules. Update ThrustInstallRules.cmake
# if modifying this file.
if (CUB_IN_THRUST)
  return()
endif()

# Bring in CMAKE_INSTALL_LIBDIR
include(GNUInstallDirs)

# CUB is a header library; no need to build anything before installing:
set(CMAKE_SKIP_INSTALL_ALL_DEPENDENCY TRUE)

install(DIRECTORY "${CUB_SOURCE_DIR}/cub"
  TYPE INCLUDE
  DESTINATION include/${CUB_INSTALL_HEADER_INFIX}
  FILES_MATCHING
    PATTERN "*.cuh"
)

install(DIRECTORY "${CUB_SOURCE_DIR}/cub/cmake/"
  DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/cub"
  PATTERN cub-header-search EXCLUDE
)
configure_file("${CUB_SOURCE_DIR}/cub/cmake/cub-header-search.cmake.in"
  "${CUB_BINARY_DIR}/cub/cmake/cub-header-search.cmake"
  @ONLY)
install(FILE "${CUB_BINARY_DIR}/cub/cmake/cub-header-search.cmake"
  DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/cub")
