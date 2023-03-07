find_package(Catch2)

add_executable(xp_tests 
  intf_id_tests.cpp
  intf_tests.cpp
  cls_util_tests.cpp
)
enable_testing()

target_include_directories(xp_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src/include )

target_link_directories(xp_tests PRIVATE xputil Catch2)

add_test(xp_tests xp_tests)