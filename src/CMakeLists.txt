add_library(xputil)

# GSL is installed to /usr/local/include by macports.
target_include_directories(xputil PUBLIC ${CMAKE_CURRENT_LIST_DIR}/include /usr/local/include)

target_sources(xputil
    PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/impl_intfs.cpp

    PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/include/xputil/class_attr.h
    ${CMAKE_CURRENT_LIST_DIR}/include/xputil/on_exit.h
    ${CMAKE_CURRENT_LIST_DIR}/include/xputil/intf_defs.h
    ${CMAKE_CURRENT_LIST_DIR}/include/xputil/impl_intfs.h
)

# target_link_libraries(xputil)