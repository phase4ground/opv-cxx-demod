// Copyright 2023 Open Research Institute, Inc.

#pragma once

#include "Numerology.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <functional>
#include <iostream>

static const int minimum_packet_length = 20;    // smallest valid IP packet (header and no data)

struct OPVCobsDecoder
{
    enum class State { RESET, PACKET_TOO_LONG, CHUNK, CASE255 };
    State state_ = State::RESET;
    unsigned int decoded_count = 0;     // number of bytes already in decoded output packet buffer
    unsigned int remaining_count = 0;   // countdown of bytes expected in COBS-encoded input block

    // packet assembly buffer
    uint8_t packet[mobilinkd::ip_mtu+3];    // allow mtu+1 at loop end to accommodate virtual zero, plus up to 2 bytes added per iteration

    // Packet callback functions must copy the data before returning.
    using packet_callback_t = std::function<void(const uint8_t *, unsigned int)>;
    packet_callback_t packet_callback;


    /**
     * Reset COBS decoder.
    */
    void reset()
    {
        // std::cerr << "Resetting COBS" << std::endl;
        state_ = State::RESET;
        decoded_count = 0;
        remaining_count = 0;
    }


    /**
     * Submit a decoded packet to the callback, if there is one.
    */
    void submit_decoded_packet(const uint8_t *packet, int packet_length)
    {
        if (packet_callback)
        {
            packet_callback(packet, packet_length);
        }
        else
        {
            std::cerr << "Discarding " << packet_length << " byte packet: no callback registered" << std::endl;
        }

    }


    /**
     * Process a frame's worth of COBS data. For each decoded packet that
     * is completed and of valid length, we call submit_decoded_packet.
     * 
     * A packet may be entirely contained within a frame, or it may straddle
     * either or both frame boundaries. We must keep track of the COBS decoder
     * state across frame boundaries.
     * 
     * A packet that ends abnormally (i.e., an unexpected 0 byte arrives
     * within the COBS-encoded data) is also discarded and COBS decoding
     * begins afresh. Correctly-received data can never be lost due to
     * COBS synchronization failure.
     * 
     * Decoded packets that are shorter than a valid IP packet (20 bytes)
     * or longer than our maximum packet size (ip_mtu) are also discarded.
     * Any additional validity checks are left for higher layers to take
     * care of.
     * 
     * For standard voice data, a constant-rate Opus packet (with its
     * IP+UDP+RTP overhead, plus one zero-valued byte of COBS overhead
     * at the end) exactly fills a frame. Because the frame length is
     * shorter than 254 bytes, there is never more COBS overhead than that
     * in a single voice packet.
     * 
     * See: "Consistent Overhead Byte Stuffing"
     *      http://www.stuartcheshire.org/papers/cobsforton.pdf
     *      by Stuart Cheshire and Mary Baker.
    */
    void process_cobs_data(const uint8_t *cobs_data, int buffer_length)
    {
        for (int index = 0; index < buffer_length; index++)
        {
            uint8_t byte = cobs_data[index];

            if (state_ == State::PACKET_TOO_LONG)
            {
                if (byte == 0)  // Finally, the too-long packet has ended!
                {
                    std::cerr << "Discarded a too-long packet." << std::endl;
                    reset();
                }
                else
                {
                // skip over non-zero bytes that would make the too-long packet even longer
                }

            }
            else if (byte == 0)
            {
                if (remaining_count > 0)    // unexpected zero byte within a chunk
                {
                    std::cerr << "Unexpected 0 in COBS data" << std::endl;
                    reset();
                }
                else if (decoded_count > 0) // we have a packet, and here's the end of it
                {
                    if (packet[decoded_count-1] == 0)   // check for extra "virtual zero" at the end
                    {
                        decoded_count--;    // trim it off
                    }

                    if (decoded_count >= minimum_packet_length && decoded_count <= mobilinkd::ip_mtu)
                    {
                        submit_decoded_packet(packet, decoded_count);
                    }
                    reset();    // ready for the next packet
                }
                else
                {
                    // if we got a 0 byte when expecting a new packet, that's just
                    // filler between packets. Do nothing.
                }
            }
            else if (remaining_count > 0)   // This is a data byte within a chunk
            {
                packet[decoded_count++] = byte;
                remaining_count--;
                if (remaining_count == 0)
                {
                    if (state_ != State::CASE255)
                    {
                        packet[decoded_count++] = 0;    // insert implied 0 at end of chunk
                    }
                    state_ = State::CHUNK;
                }
            }
            else if (byte == 0xff)  // this is a new chunk count, in the special case
            {
                remaining_count = 254;
                state_ = State::CASE255;
            }
            else    // this is a new chunk count, common cases
            {
                remaining_count = byte - 1;
                if (byte == 0x01)
                {
                    packet[decoded_count++] = 0;
                    state_ = State::RESET;
                }
                else if (byte == 0xFF)
                {
                    state_ = State::CASE255;
                }
                else
                {
                    state_ = State::CHUNK;
                }
            }

            if (decoded_count > mobilinkd::ip_mtu+1)    // packet length exceeds MTU (with possible extra virtual 0); discard additional bytes
            {
                state_ = State::PACKET_TOO_LONG;
            }
        }    
    }


    /**
     * Register a callback to accept a decoded packet.
    */
   void set_packet_callback(packet_callback_t callback)
   {
        packet_callback = callback;
   }


    /**
     * Receive a sequence of bytes (generally a received frame payload)
     * to be decoded according to OPV COBS rules, and process it.
     */
    void operator()(const uint8_t * buffer, size_t buffer_length)
    {

        // std::cerr << "Processing " << buffer_length << " COBS bytes" << std::endl;

        process_cobs_data(buffer, buffer_length);
    }
};
