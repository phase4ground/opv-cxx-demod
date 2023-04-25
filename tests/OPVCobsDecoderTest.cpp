#include "OPVCobsDecoder.h"
#include "cobs.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <stdlib.h>

using namespace mobilinkd;

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

static const int max_packets = 1000;
int packet_count;
int packet_length[max_packets];
uint8_t last_packet[ip_mtu+2];

void reset(void)
{
    packet_count = 0;
}


void packet_callback(const uint8_t *packet, int len)
{
    packet_length[packet_count] = len;
    memcpy(last_packet, packet, std::min(len, ip_mtu));
    packet_count++;
}

class OPVCobsDecoderTest : public ::testing::Test {
 protected:

  OPVCobsDecoder cobs_decoder;

  void SetUp() override
  {
    cobs_decoder.set_packet_callback(packet_callback);
    packet_count = 0;
  }

  // void TearDown() override {}

};

TEST_F(OPVCobsDecoderTest, all_zeroes_no_output)
{
    uint8_t frame[stream_frame_payload_bytes];

    memset(frame, 0, stream_frame_payload_bytes);   // fill frame with zero

    cobs_decoder(frame, stream_frame_payload_bytes);

    EXPECT_EQ(packet_count, 0) << "Failed to ignore zeroes."; // there are no packets here
}

TEST_F(OPVCobsDecoderTest, one_easy_packet)
{
    uint8_t frame[stream_frame_payload_bytes];

    uint8_t data[] = "123456789012345678901234567890";

    memset(frame, 0, stream_frame_payload_bytes);   // fill frame with zero
    frame[50] = sizeof(data); // +1 is taken care of by the string-terminating 0
    memcpy(frame+51, data, sizeof(data)-1);
    frame[51+sizeof(data)] = 0;

    cobs_decoder(frame, stream_frame_payload_bytes);

    EXPECT_EQ(packet_count, 1) << "Did not find exactly one packet.";
    EXPECT_EQ(packet_length[0], 30) << "Packet length " << packet_length[0] << " is not 30";
    EXPECT_EQ(memcmp(last_packet, data, 30), 0) << "Packet data did not match.";
}

TEST_F(OPVCobsDecoderTest, one_packet_exactly_20)
{
    uint8_t frame[stream_frame_payload_bytes];

    uint8_t data[] = "12345678901234567890";

    memset(frame, 0, stream_frame_payload_bytes);   // fill frame with zero
    frame[50] = sizeof(data); // +1 is taken care of by the string-terminating 0
    memcpy(frame+51, data, sizeof(data)-1);
    frame[51+sizeof(data)] = 0;

    cobs_decoder(frame, stream_frame_payload_bytes);

    EXPECT_EQ(packet_count, 1) << "Did not find exactly one packet.";
    EXPECT_EQ(packet_length[0], 20) << "Packet length " << packet_length[0] << " is not 20";
    EXPECT_EQ(memcmp(last_packet, data, 20), 0) << "Packet data did not match.";
}

TEST_F(OPVCobsDecoderTest, one_packet_1_too_short)
{
    uint8_t frame[stream_frame_payload_bytes];

    uint8_t data[] = "1234567890123456789";

    memset(frame, 0, stream_frame_payload_bytes);   // fill frame with zero
    frame[50] = sizeof(data); // +1 is taken care of by the string-terminating 0
    memcpy(frame+51, data, sizeof(data)-1);
    frame[51+sizeof(data)] = 0;

    cobs_decoder(frame, stream_frame_payload_bytes);

    EXPECT_EQ(packet_count, 0) << "Failed to ignore 19-byte packet.";
}

TEST_F(OPVCobsDecoderTest, one_packet_straddling)
{
    uint8_t frame[stream_frame_payload_bytes];

    uint8_t data[] = "123456789012345678901234567890";

    memset(frame, 0, stream_frame_payload_bytes);   // fill frame with zero
    frame[stream_frame_payload_bytes-11] = sizeof(data); // +1 is taken care of by the string-terminating 0
    memcpy(frame+stream_frame_payload_bytes-10, data, 10);   // first 10 bytes of the packet at the end of a frame
    cobs_decoder(frame, stream_frame_payload_bytes);

    EXPECT_EQ(packet_count, 0) << "Returned partial frame?"; // there are no complete packets here

    memset(frame, 0, stream_frame_payload_bytes);   // fill frame with zero
    memcpy(frame, data+10, 20); // last 20 bytes of the packet at the beginning of next frame
    frame[20] = 0;
    cobs_decoder(frame, stream_frame_payload_bytes);

    EXPECT_EQ(packet_count, 1) << "Did not find exactly one packet.";
    EXPECT_EQ(packet_length[0], 30) << "Packet length " << packet_length[0] << " is not 30";
    EXPECT_EQ(memcmp(last_packet, data, 30), 0) << "Packet data did not match.";
}

TEST_F(OPVCobsDecoderTest, whole_frame_packets)
{
    uint8_t frame[stream_frame_payload_bytes];

    memset(frame, 'A', stream_frame_payload_bytes);   // fill frame with 'AAAA...'
    frame[0] = stream_frame_payload_bytes-1;
    frame[stream_frame_payload_bytes-1] = 0;
    cobs_decoder(frame, stream_frame_payload_bytes);

    EXPECT_EQ(packet_count, 1) << "Did not find first packet.";
    EXPECT_EQ(packet_length[0], stream_frame_payload_bytes-2) << "Packet length " << packet_length[0] << " is not " << stream_frame_payload_bytes-2;
    EXPECT_EQ(memcmp(last_packet, frame+1, stream_frame_payload_bytes-2), 0) << "Packet data did not match.";

    cobs_decoder(frame, stream_frame_payload_bytes);

    EXPECT_EQ(packet_count, 2) << "Did not find second packet.";
    EXPECT_EQ(packet_length[0], stream_frame_payload_bytes-2) << "Packet length " << packet_length[0] << " is not " << stream_frame_payload_bytes-2;
    EXPECT_EQ(memcmp(last_packet, frame+1, stream_frame_payload_bytes-2), 0) << "Packet data did not match.";

    cobs_decoder(frame, stream_frame_payload_bytes);

    EXPECT_EQ(packet_count, 3) << "Did not find third packet.";
    EXPECT_EQ(packet_length[0], stream_frame_payload_bytes-2) << "Packet length " << packet_length[0] << " is not " << stream_frame_payload_bytes-2;
    EXPECT_EQ(memcmp(last_packet, frame+1, stream_frame_payload_bytes-2), 0) << "Packet data did not match.";
}

TEST_F(OPVCobsDecoderTest, mtu_minus_one)
{
  const int pkt_len = ip_mtu-1;
  uint8_t pkt[pkt_len];
  const int cobs_len = pkt_len + int(std::ceil(pkt_len/254.0)) + 1;
  uint8_t cobs_pkt[cobs_len];

  // Fill up the packet buffer with (crappy) pseudo-random bytes
  srand(1); // make the random numbers repeatable
  for (int i=0; i < pkt_len; i++)
  {
    pkt[i] = rand() % 0x100;  // crappy random numbers but we don't care!
  }

  cobs_encode_result result = cobs_encode(cobs_pkt, cobs_len, pkt, pkt_len);
  ASSERT_EQ(result.status, COBS_ENCODE_OK) << "Failure in COBS encoder";

  int remaining = result.out_len;
  uint8_t *ptr = cobs_pkt;

  while (remaining >= stream_frame_payload_bytes)
    {
      cobs_decoder(ptr, stream_frame_payload_bytes);
      remaining -= stream_frame_payload_bytes;
      ptr += stream_frame_payload_bytes;
    }

  uint8_t frame[stream_frame_payload_bytes];

  memset(frame, 0, stream_frame_payload_bytes);
  memcpy(frame, ptr, remaining);
  cobs_decoder(frame, stream_frame_payload_bytes);
  remaining = 0;

  EXPECT_EQ(packet_count, 1) << "Failed to find exactly one packet";
  EXPECT_EQ(packet_length[0], pkt_len) << "Length " << packet_length[0] << " is not " << pkt_len;
}

TEST_F(OPVCobsDecoderTest, mtu_exactly)
{
  const int pkt_len = ip_mtu;
  uint8_t pkt[pkt_len];
  const int cobs_len = pkt_len + int(std::ceil(pkt_len/254.0)) + 1;
  uint8_t cobs_pkt[cobs_len];

  // Fill up the packet buffer with (crappy) pseudo-random bytes
  srand(1); // make the random numbers repeatable
  for (int i=0; i < pkt_len; i++)
  {
    pkt[i] = rand() % 0x100;  // crappy random numbers but we don't care!
  }

  cobs_encode_result result = cobs_encode(cobs_pkt, cobs_len, pkt, pkt_len);
  ASSERT_EQ(result.status, COBS_ENCODE_OK) << "Failure in COBS encoder";

  int remaining = result.out_len;
  uint8_t *ptr = cobs_pkt;

  while (remaining >= stream_frame_payload_bytes)
    {
      cobs_decoder(ptr, stream_frame_payload_bytes);
      remaining -= stream_frame_payload_bytes;
      ptr += stream_frame_payload_bytes;
    }

  uint8_t frame[stream_frame_payload_bytes];

  memset(frame, 0, stream_frame_payload_bytes);
  memcpy(frame, ptr, remaining);
  cobs_decoder(frame, stream_frame_payload_bytes);
  remaining = 0;

  EXPECT_EQ(packet_count, 1) << "Failed to find exactly one packet";
  EXPECT_EQ(packet_length[0], pkt_len) << "Length " << packet_length[0] << " is not " << pkt_len;
}

TEST_F(OPVCobsDecoderTest, mtu_plus_one)
{
  const int pkt_len = ip_mtu+1;
  uint8_t pkt[pkt_len];
  const int cobs_len = pkt_len + int(std::ceil(pkt_len/254.0)) + 1;
  uint8_t cobs_pkt[cobs_len];

  // Fill up the packet buffer with (crappy) pseudo-random bytes
  srand(1); // make the random numbers repeatable
  for (int i=0; i < pkt_len; i++)
  {
    pkt[i] = rand() % 0x100;  // crappy random numbers but we don't care!
  }

  cobs_encode_result result = cobs_encode(cobs_pkt, cobs_len, pkt, pkt_len);
  ASSERT_EQ(result.status, COBS_ENCODE_OK) << "Failure in COBS encoder";

  int remaining = result.out_len;
  uint8_t *ptr = cobs_pkt;

  while (remaining >= stream_frame_payload_bytes)
    {
      cobs_decoder(ptr, stream_frame_payload_bytes);
      remaining -= stream_frame_payload_bytes;
      ptr += stream_frame_payload_bytes;
    }

  uint8_t frame[stream_frame_payload_bytes];

  memset(frame, 0, stream_frame_payload_bytes);
  memcpy(frame, ptr, remaining);
  cobs_decoder(frame, stream_frame_payload_bytes);
  remaining = 0;

  EXPECT_EQ(packet_count, 0) << "Failed to discard long packet";
}