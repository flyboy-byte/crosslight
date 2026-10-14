add_executable(LibreBlePacketTest
  LibreBlePacketTest.cpp
  ${REPO_ROOT}/src/libre/LibreBlePacket.cpp
)

target_include_directories(LibreBlePacketTest PRIVATE
  ${REPO_ROOT}/src
)

target_link_libraries(LibreBlePacketTest PRIVATE
  crosspoint_test_common
  GTest::gtest_main
)

gtest_discover_tests(LibreBlePacketTest)
