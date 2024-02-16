set(CMAKE_INCLUDE_CURRENT_DIR ON)

#defines.hpp header with #cmakedefine 
configure_file(${PROJECT_SOURCE_DIR}/src/defines.hpp.in ${PROJECT_SOURCE_DIR}/src/defines.hpp)

set(SOURCES
    Topology.cpp
    DataPath.cpp
    CAT_aware.cpp
    cpuinfo.cpp
    nvidia_mig.cpp
    xml_dump.cpp
    parsers/hwloc.cpp
    parsers/caps-numa-benchmark.cpp
    parsers/gpu-topo.cpp
    parsers/cccbench.cpp
    )

set(HEADERS
    sys-sage.hpp
    defines.hpp
    Topology.hpp
    DataPath.hpp
    xml_dump.hpp
    parsers/hwloc.hpp
    parsers/caps-numa-benchmark.hpp
    parsers/gpu-topo.hpp
    parsers/cccbench.cpp
    )

if(PAPI_METRICS)
	list(APPEND SOURCES papi/Logging.cpp papi/SystemInfo.cpp papi/Metrics.cpp papi/Utility.cpp papi/Storage.cpp papi/Binding.cpp)
	list(APPEND HEADERS papi/Metrics.hpp papi/Logging.hpp papi/SystemInfo.hpp papi/Utility.hpp papi/Storage.hpp papi/Binding.hpp papi/Statistics.hpp)
endif()

add_library(sys-sage SHARED ${SOURCES} ${HEADERS})

target_include_directories(sys-sage PUBLIC  
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>  
    $<INSTALL_INTERFACE:inc>
    $<INSTALL_INTERFACE:lib>
)
install(
    TARGETS sys-sage
    EXPORT sys-sage-targets
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib/cmake/lib
    ARCHIVE DESTINATION lib/cmake/lib
)
#for spack
install(
    EXPORT sys-sage-targets
    FILE sys-sage-targets.cmake
    DESTINATION lib/cmake/sys-sage
    NAMESPACE sys-sage::
)
install(DIRECTORY "."
    DESTINATION lib/cmake/inc
    FILES_MATCHING PATTERN "*.hpp")

install(
    TARGETS sys-sage
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)
install(DIRECTORY "."
    DESTINATION inc
    FILES_MATCHING PATTERN "*.hpp")
