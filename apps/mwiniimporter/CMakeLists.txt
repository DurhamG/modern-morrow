set(MWINIIMPORT
    main.cpp
)

set(MWINIIMPORT_LIB
    importer.cpp
)

set(MWINIIMPORT_HEADER
    importer.hpp
)

source_group(launcher FILES ${MWINIIMPORT} ${MWINIIMPORT_LIB} ${MWINIIMPORT_HEADER})

# The importer's code is a library of its own, so components-tests can test it.
add_library(openmw-iniimporter-lib STATIC
    ${MWINIIMPORT_LIB}
    ${MWINIIMPORT_HEADER}
)

target_link_libraries(openmw-iniimporter-lib
    components
)

openmw_add_executable(openmw-iniimporter
    ${MWINIIMPORT}
)

target_link_libraries(openmw-iniimporter
    Boost::program_options
    openmw-iniimporter-lib
)

if (WIN32)
    target_sources(openmw-iniimporter PRIVATE ${CMAKE_SOURCE_DIR}/files/windows/other-apps.manifest)
endif()

if (NOT APPLE)
    install(TARGETS openmw-iniimporter ${runtime_dependency_arguments})
endif()

if (MINGW)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -municode")
endif()

if (BUILD_WITH_CODE_COVERAGE)
    target_compile_options(openmw-iniimporter-lib PRIVATE --coverage)
    target_compile_options(openmw-iniimporter PRIVATE --coverage)
    target_link_libraries(openmw-iniimporter gcov)
endif()

if (MSVC AND PRECOMPILE_HEADERS_WITH_MSVC)
    target_precompile_headers(openmw-iniimporter REUSE_FROM components)
endif()
