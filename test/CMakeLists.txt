add_executable(sanity_test sanity_test.cc)
target_link_libraries(sanity_test PRIVATE GTest::gtest_main toyspsc_lib)
gtest_discover_tests(sanity_test)

add_executable(spsc_queue_test spsc_queue_test.cc)
target_link_libraries(spsc_queue_test PRIVATE GTest::gtest_main toyspsc_lib)
gtest_discover_tests(spsc_queue_test)
