add_library(xputil INTERFACE)

add_subdirectory(include)

### Install

install(
    TARGETS xputil
    EXPORT xputilConfig
)

install(
    DIRECTORY include/xputil
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

# Export
install(
    EXPORT xputilConfig
    DESTINATION lib/xputil
)