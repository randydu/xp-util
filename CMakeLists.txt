cmake_minimum_required(VERSION 3.10)

set (CMAKE_CXX_STANDARD 20)
set (CXX_STANDARD_REQUIRED YES) 

include(GNUInstallDirs)

project(xputil 
    VERSION 2.1 
    DESCRIPTION "cross platform utility library"
    LANGUAGES CXX
)

find_package(Microsoft.GSL)

add_subdirectory(src)

### Options
option(XPUTIL_TEST "Build unit tests" NO)


if (XPUTIL_TEST)
    add_subdirectory(test)
endif()
