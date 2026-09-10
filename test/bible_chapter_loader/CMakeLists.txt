add_executable(BibleChapterLoaderTest
  BibleChapterLoaderTest.cpp
  ${REPO_ROOT}/src/bible/BibleChapterLoader.cpp
  ${REPO_ROOT}/lib/JsonParser/StreamingJsonParser.cpp
)

target_include_directories(BibleChapterLoaderTest PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/stubs
  ${REPO_ROOT}/src/bible
  ${REPO_ROOT}/lib/JsonParser
)

target_compile_definitions(BibleChapterLoaderTest PRIVATE
  BIBLE_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures"
)

target_link_libraries(BibleChapterLoaderTest PRIVATE
  crosspoint_test_common
  GTest::gtest_main
)

gtest_discover_tests(BibleChapterLoaderTest)
