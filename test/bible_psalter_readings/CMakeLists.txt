add_executable(BiblePsalterReadingsTest
  BiblePsalterReadingsTest.cpp
)

target_include_directories(BiblePsalterReadingsTest PRIVATE
  ${REPO_ROOT}/src
)

target_link_libraries(BiblePsalterReadingsTest PRIVATE
  crosspoint_test_common
  GTest::gtest_main
)

gtest_discover_tests(BiblePsalterReadingsTest)
