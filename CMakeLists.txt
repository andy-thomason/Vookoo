cmake_minimum_required(VERSION 3.1.3 FATAL_ERROR)
cmake_policy(VERSION 3.1.3)

project(Vookoo VERSION 2.0.0)

add_subdirectory(external/glfw)
add_subdirectory(examples)

add_library(vookoo_interface INTERFACE )
target_sources(vookoo_interface INTERFACE )
target_include_directories(vookoo_interface INTERFACE
        $<INSTALL_INTERFACE:include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include> )

if (CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
    include(GNUInstallDirs)
    include(CMakePackageConfigHelpers)
    write_basic_package_version_file(
            VookooConfigVersion.cmake
            VERSION ${PACKAGE_VERSION}
            COMPATIBILITY AnyNewerVersion
    )

    install(TARGETS vookoo_interface
            EXPORT vookoo-export
            LIBRARY DESTINATION lib
            ARCHIVE DESTINATION lib
            INCLUDES DESTINATION include)

    install(DIRECTORY ${PROJECT_SOURCE_DIR}/include/vku
            DESTINATION include
            FILES_MATCHING PATTERN "*.hpp"
            PERMISSIONS OWNER_READ  GROUP_READ WORLD_READ)

    install(EXPORT vookoo-export
            FILE
            VookooTargets.cmake
            NAMESPACE
            Vookoo::
            DESTINATION
            lib/cmake/Vookoo
            )

    include(CMakePackageConfigHelpers)
    configure_package_config_file(
            "Config.cmake.in"
            "VookooConfig.cmake"
            INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/Vookoo
            PATH_VARS
            CMAKE_INSTALL_LIBDIR
    )

    write_basic_package_version_file(
            ${CMAKE_CURRENT_BINARY_DIR}/VookooConfigVersion.cmake
            VERSION 1.0.0
            COMPATIBILITY SameMajorVersion
    )
    ### Install Config and ConfigVersion files
    install(
            FILES "${CMAKE_CURRENT_BINARY_DIR}/VookooConfig.cmake"
            "${CMAKE_CURRENT_BINARY_DIR}/VookooConfigVersion.cmake"
            DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/Vookoo"
    )

    message ("CMAKE_INSTALL_LIBDIR ${CMAKE_INSTALL_LIBDIR}")
endif(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)