add_executable(BleAuditTest
  BleAdTest.cpp
  BleMatcherTest.cpp
  BleBeaconTest.cpp
  ${REPO_ROOT}/src/bleaudit/BleAd.cpp
  ${REPO_ROOT}/src/bleaudit/BleMatcher.cpp
)

target_include_directories(BleAuditTest PRIVATE
  ${REPO_ROOT}/src
)

target_link_libraries(BleAuditTest PRIVATE
  crosspoint_test_common
  GTest::gtest_main
)

gtest_discover_tests(BleAuditTest)
