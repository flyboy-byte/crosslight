add_executable(WifiAuditTest
  WifiFrameTest.cpp
  ThreatDetectTest.cpp
  PcapTest.cpp
  EapolTest.cpp
  FrameBuilderTest.cpp
  ActiveAuditGateTest.cpp
  RandomMacTest.cpp
  ${REPO_ROOT}/src/wifiaudit/WifiFrame.cpp
  ${REPO_ROOT}/src/offensive/ThreatDetect.cpp
  ${REPO_ROOT}/src/offensive/Pcap.cpp
  ${REPO_ROOT}/src/offensive/Eapol.cpp
  ${REPO_ROOT}/src/offensive/FrameBuilder.cpp
  ${REPO_ROOT}/src/offensive/ActiveAuditGate.cpp
)

target_include_directories(WifiAuditTest PRIVATE
  ${REPO_ROOT}/src
)

target_link_libraries(WifiAuditTest PRIVATE
  crosspoint_test_common
  GTest::gtest_main
)

gtest_discover_tests(WifiAuditTest)
