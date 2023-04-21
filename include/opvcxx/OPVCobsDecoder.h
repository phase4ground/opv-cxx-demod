// Copyright 2023 Open Research Institute, Inc.

#pragma once

#include "Numerology.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <functional>
#include <iostream>

static const int ip_mtu_cobs = mobilinkd::ip_mtu + std::ceil(mobilinkd::ip_mtu / 254.0); // maximum packet size after COBS encoding

struct OPVCobsDecoder
{

    // make objects

    // define array types

    enum class State { RESET, FOO, BAR, BAZ };
    State state_ = State::RESET;
    unsigned int decoded_count = 0;

    using packet_callback_t = std::function<void(const uint8_t *, unsigned int)>;
    packet_callback_t packet_callback;


    /**
     * Reset COBS decoder.
    */
    void reset()
    {
        std::cout << "Resetting COBS" << std::endl;
        state_ = State::RESET;
        decoded_count = 0;
    }


    /**
     * Register a callback to accept a decoded packet.
    */
   void set_packet_callback(packet_callback_t callback)
   {
        packet_callback = callback;
   }


    /**
     * Decode a sequence of bytes (generally a received frame payload) according
     * to OPV COBS rules.
     */
    void operator()(const uint8_t * buffer, size_t buffer_length)
    {
        std::cerr << "Processing " << buffer_length << " COBS bytes" << std::endl;

        // this is not really where the callback goes, this is just for test
        if (packet_callback)
        {
            packet_callback(buffer, buffer_length);
        }
    }
};
