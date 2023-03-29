// Copyright 2023 Open Research Institute, Inc.

#pragma once

#include "Numerology.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <functional>
#include <iostream>

struct OPVCobsDecoder
{

    // make objects

    // define array types

    enum class State { FOO, BAR, BAZ };


    void reset()
    {
        std::cout << "Resetting COBS" << std::endl;
    }


    /**
     * Decode a sequence of bytes (generally a received frame payload) according
     * to OPV COBS rules.
     */
    void operator()(const uint8_t * buffer, size_t buffer_length)
    {
        std::cout << "Processing " << buffer_length << " COBS bytes" << std::endl;
    }
};
