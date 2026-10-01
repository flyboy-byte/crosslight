add_executable(VendorLookupTest
  VendorLookupTest.cpp
)

target_include_directories(VendorLookupTest PRIVATE
  ${REPO_ROOT}/src
)

target_link_libraries(VendorLookupTest PRIVATE
  crosspoint_test_common
  GTest::gtest_main
)

gtest_discover_tests(VendorLookupTest)
