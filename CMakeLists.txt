cmake_minimum_required(VERSION 3.10)

set (CMAKE_CXX_STANDARD 20)
set (CXX_STANDARD_REQUIRED YES) 

include(GNUInstallDirs)

project(xputil 
    VERSION 2.1 
    DESCRIPTION "cross platform utility library"
    LANGUAGES CXX)

# Microsoft.GSL should be installed
# list(APPEND CMAKE_PREFIX_PATH "/usr/local/share/cmake/Microsoft.GSL")
# find_package(Microsoft.GSL REQUIRED)

add_subdirectory(src)