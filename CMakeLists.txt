cmake_minimum_required(VERSION 3.16)

project(ZKLEP LANGUAGES CXX)

#set(CMAKE_CXX_STANDARD 98)

if(CMAKE_CXX_COMPILER MATCHES "cosmoc\\+\\+")
    set(CMAKE_CXX_STANDARD 17)
else()
    set(CMAKE_CXX_STANDARD 98)
endif()

set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CORE_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/)
set(CORE_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/)

set(CORE_SOURCES
    ${CORE_SRC_DIR}/main.cpp
)

add_executable(ZKLEP ${CORE_SOURCES}
)

target_include_directories(ZKLEP PRIVATE
    ${CORE_INCLUDE_DIR}
)

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(ZKLEP PRIVATE
        -Os
        -fno-ident
        -fno-exceptions
        -fno-rtti
        -ffunction-sections
        -fdata-sections
        -flto
        -Wno-deprecated
    )
    
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        target_compile_options(ZKLEP PRIVATE -Oz)
    endif()

    target_link_options(ZKLEP PRIVATE
        -s
        -Wl,--gc-sections
        -flto
    )

    if(WIN32)
        target_link_options(ZKLEP PRIVATE -static-libgcc -static-libstdc++)

        if(CMAKE_SIZEOF_VOID_P EQUAL 4)
            target_link_options(ZKLEP PRIVATE -Wl,--subsystem,console:5.01)
            set_target_properties(ZKLEP PROPERTIES OUTPUT_NAME "ZKLEP_Win_i686")
        else()
            target_link_options(ZKLEP PRIVATE -Wl,--subsystem,console:5.02)
            set_target_properties(ZKLEP PROPERTIES OUTPUT_NAME "ZKLEP_Win_x86-64")
        endif()
    endif()

    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        target_link_options(ZKLEP PRIVATE -static-libstdc++)

        if(CMAKE_SIZEOF_VOID_P EQUAL 4)
            set_target_properties(ZKLEP PROPERTIES OUTPUT_NAME "ZKLEP_Linux_i686")
        else()
            set_target_properties(ZKLEP PROPERTIES OUTPUT_NAME "ZKLEP_Linux_x86-64")
        endif()
    endif()

endif()

include(GNUInstallDirs)
install(TARGETS ZKLEP
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
