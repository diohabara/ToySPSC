add_executable(sanity_test sanity_test.cc)
target_link_libraries(sanity_test PRIVATE GTest::gtest_main toyspsc_lib)
gtest_discover_tests(sanity_test)
