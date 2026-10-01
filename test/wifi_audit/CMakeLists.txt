add_executable(WifiAuditTest
  WifiFrameTest.cpp
  ThreatDetectTest.cpp
  ${REPO_ROOT}/src/wifiaudit/WifiFrame.cpp
  ${REPO_ROOT}/src/wifiaudit/ThreatDetect.cpp
)

target_include_directories(WifiAuditTest PRIVATE
  ${REPO_ROOT}/src
)

target_link_libraries(WifiAuditTest PRIVATE
  crosspoint_test_common
  GTest::gtest_main
)

gtest_discover_tests(WifiAuditTest)
