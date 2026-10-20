#include <gtest/gtest.h>

#include "libre/RelayFrame.h"

namespace {

relay::Message roundTrip(const relay::Message& in, const size_t maxFrame) {
  relay::Reassembler r;
  relay::Message out;
  bool done = false;
  for (const auto& f : relay::encode(in, maxFrame)) {
    EXPECT_LE(f.size(), maxFrame);
    if (r.feed(reinterpret_cast<const uint8_t*>(f.data()), f.size())) {
      out = r.take();
      done = true;
    }
  }
  EXPECT_TRUE(done);
  return out;
}

}  // namespace

TEST(RelayFrame, SmallPromptFitsOneFrame) {
  relay::Message m;
  m.type = relay::kPrompt;
  m.id = 7;
  m.payload = "hi";
  const auto frames = relay::encode(m, 100);
  ASSERT_EQ(frames.size(), 1u);
  EXPECT_EQ(frames[0].size(), 4u + 2u + 2u);
  EXPECT_EQ(static_cast<uint8_t>(frames[0][3]), 0);  // no MORE
  EXPECT_EQ(static_cast<uint8_t>(frames[0][4]), 2);  // total length LE
  EXPECT_EQ(static_cast<uint8_t>(frames[0][5]), 0);
}

TEST(RelayFrame, EmptyPayloadIsOneFrame) {
  relay::Message m;
  m.type = relay::kUsage;
  m.id = 1;
  const auto out = roundTrip(m, 20);
  EXPECT_EQ(out.type, relay::kUsage);
  EXPECT_TRUE(out.payload.empty());
}

TEST(RelayFrame, RoundTripsAcrossMtusIncludingTheMinimum) {
  relay::Message m;
  m.type = relay::kPrompt;
  m.id = 200;
  m.status = relay::kOk;
  m.payload.assign(4000, 'x');
  for (size_t i = 0; i < m.payload.size(); ++i) m.payload[i] = static_cast<char>('a' + i % 26);
  for (const size_t mtu : {20u, 23u, 100u, 244u, 514u}) {
    const auto out = roundTrip(m, mtu);
    EXPECT_EQ(out.payload, m.payload) << "maxFrame=" << mtu;
    EXPECT_EQ(out.id, 200);
    EXPECT_EQ(out.status, relay::kOk);
  }
}

TEST(RelayFrame, TruncatedFlagSurvives) {
  relay::Message m;
  m.type = relay::kPrompt;
  m.id = 3;
  m.flags = relay::kFlagTruncated;
  m.payload.assign(500, 'y');
  const auto out = roundTrip(m, 60);
  EXPECT_TRUE(out.truncated());
  EXPECT_EQ(out.payload.size(), 500u);
}

TEST(RelayFrame, LengthMismatchIsABadRequestNeverPartialText) {
  relay::Message m;
  m.type = relay::kPrompt;
  m.id = 9;
  m.payload.assign(100, 'z');
  auto frames = relay::encode(m, 40);
  ASSERT_GT(frames.size(), 2u);
  frames[0][4] = 120;  // declared total no longer matches
  relay::Reassembler r;
  relay::Message out;
  bool done = false;
  for (const auto& f : frames) {
    if (r.feed(reinterpret_cast<const uint8_t*>(f.data()), f.size())) {
      out = r.take();
      done = true;
    }
  }
  ASSERT_TRUE(done);
  EXPECT_EQ(out.status, relay::kBadRequest);
  EXPECT_TRUE(out.payload.empty());
}

TEST(RelayFrame, StrayContinuationAndShortFramesAreDropped) {
  relay::Reassembler r;
  const uint8_t tiny[3] = {1, 2, 3};
  EXPECT_FALSE(r.feed(tiny, 3));

  relay::Message m;
  m.type = relay::kPrompt;
  m.id = 5;
  m.payload.assign(100, 'q');
  auto frames = relay::encode(m, 40);
  ASSERT_GT(frames.size(), 2u);
  EXPECT_FALSE(r.feed(reinterpret_cast<const uint8_t*>(frames[0].data()), frames[0].size()));
  std::string stray = frames[1];
  stray[1] = 6;  // different request id
  EXPECT_FALSE(r.feed(reinterpret_cast<const uint8_t*>(stray.data()), stray.size()));
  // The real continuation still completes the original message.
  bool done = false;
  for (size_t i = 1; i < frames.size(); ++i) {
    done = r.feed(reinterpret_cast<const uint8_t*>(frames[i].data()), frames[i].size());
  }
  ASSERT_TRUE(done);
  EXPECT_EQ(r.take().payload.size(), 100u);
}
