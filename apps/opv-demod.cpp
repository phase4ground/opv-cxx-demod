// Copyright 2020 Mobilinkd LLC.
// Copyright 2022 Open Research Institute, Inc.

#include "OPVCobsDecoder.h"
#include "OPVDemodulator.h"
#include "FirFilter.h"

#include "Numerology.h"
#include <opus/opus.h>

#include <boost/program_options.hpp>
#include <boost/optional.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <vector>

const char VERSION[] = "0.2";

using namespace mobilinkd;

uint32_t debug_sample_count = 0;

OpusDecoder* opus_decoder;
OPVCobsDecoder cobs_decoder;

PRBS9 prbs;

struct Config
{
    bool verbose = false;
    bool debug = false;
    bool quiet = false;
    bool invert = false;
    bool noise_blanker = false;

    static std::optional<Config> parse(int argc, char* argv[])
    {
        namespace po = boost::program_options;

        Config result;

        // Declare the supported options.
        po::options_description desc(
            "Program options");
        desc.add_options()
            ("help,h", "Print this help message and exit.")
            ("version,V", "Print the application version and exit.")
            ("invert,i", po::bool_switch(&result.invert), "invert the received baseband")
            ("noise-blanker,b", po::bool_switch(&result.noise_blanker), "noise blanker -- silence likely corrupt audio")
            ("verbose,v", po::bool_switch(&result.verbose), "verbose output")
            ("debug,d", po::bool_switch(&result.debug), "debug-level output")
            ("quiet,q", po::bool_switch(&result.quiet), "silence all output -- no BERT output")
            ;

        po::variables_map vm;
        po::store(po::parse_command_line(argc, argv, desc), vm);

        if (vm.count("help"))
        {
            std::cout << "Read OPV baseband from STDIN and write audio to STDOUT\n"
                << desc << std::endl;

            return std::nullopt;
        }

        if (vm.count("version"))
        {
            std::cout << argv[0] << ": " << VERSION << std::endl;
            std::cout << opus_get_version_string() << std::endl;
            return std::nullopt;
        }

        try {
            po::notify(vm);
        } catch (std::exception& ex)
        {
            std::cerr << ex.what() << std::endl;
            std::cout << desc << std::endl;
            return std::nullopt;
        }

        if (result.debug + result.verbose + result.quiet > 1)
        {
            std::cerr << "Only one of quiet, verbose or debug may be chosen." << std::endl;
            return std::nullopt;
        }

        return result;
    }
};

std::optional<Config> config;

void decode_and_output_audio(const uint8_t *encoded_audio, int encoded_len, int viterbi_cost)
{
    std::array<int16_t, audio_samples_per_opv_frame> buf;
    opus_int32 count;

    if (config->noise_blanker && viterbi_cost > 80)
    {
        buf.fill(0);
        if (config->verbose)
        {
            std::cerr << "Frameout blanked" << std::endl;
        }
    }
    else
    {
        // opus_decode can take the whole packet at once, no need to split out the frames, if any.
        count = opus_decode(opus_decoder, encoded_audio, opus_packet_size_bytes, buf.data(), audio_samples_per_opv_frame, 0);
    }

    std::cout.write((const char*)buf.data(), audio_bytes_per_opv_frame);

    if (config->verbose && count != audio_samples_per_opv_frame)
    {
            std::cerr << "Opus decode error, " << count << " samples, expected " << audio_samples_per_opv_frame << std::endl;
    }
}

bool decode_bert(OPVFrameDecoder::stream_type1_bytes_t const& bert_data)
{
    size_t count = 0;

    for (auto b: bert_data)
    {
        for (int i = 0; i != 8; ++i) {
            prbs.validate(b & 0x80);
            b <<= 1;
            count++;
            if (count >= bert_frame_prime_size)
            {
                return true;    // ignore any extra/repeated bits at the end of the frame
            }
        }
    }

    return true;
}


bool handle_frame(OPVFrameDecoder::output_buffer_t const& frame, int viterbi_cost)
{
    using FrameType = OPVFrameDecoder::FrameType;

    bool result = true;

    switch (frame.type)
    {
        case FrameType::OPV_COBS:
            cobs_decoder(frame.data.data(), stream_frame_payload_bytes);
            break;
        case FrameType::OPV_BERT:
            result = decode_bert(frame.data);
            break;
    }

    return result;
}

template <typename FloatType>
void diagnostic_callback(bool dcd, FloatType evm, FloatType deviation, FloatType offset, bool locked,
    FloatType clock, int sample_index, int sync_index, int clock_index, int viterbi_cost)
{
    if (config->debug) {
        std::cerr << "dcd: " << std::setw(1) << int(dcd)
            << ", evm: " << std::setfill(' ') << std::setprecision(4) << std::setw(8) << evm * 100 <<"%"
            << ", deviation: " << std::setprecision(4) << std::setw(8) << deviation
            << ", freq offset: " << std::setprecision(4) << std::setw(8) << offset
            << ", locked: " << std::boolalpha << std::setw(6) << locked << std::dec
            << ", clock: " << std::setprecision(7) << std::setw(8) << clock
            << ", sample: " << std::setw(1) << sample_index << ", "  << sync_index << ", " << clock_index
            << ", cost: " << viterbi_cost
            << " at sample " << debug_sample_count
            << " (" << float(debug_sample_count)/samples_per_frame << " frames)"
            << std::endl;
    }
        
    if (!dcd && prbs.sync()) { // Seems like there should be a better way to do this.
        prbs.reset();
    }

    if (prbs.sync() && !config->quiet) {
        if (!config->debug) {
            std::cerr << '\r';
        } else {
            std::cerr << ", ";
        }
    
        auto ber = double(prbs.errors()) / double(prbs.bits());
        char buffer[40];
        snprintf(buffer, 40, "BER: %-1.6lf (%lu bits)\r", ber, (long unsigned int)prbs.bits());
        std::cerr << buffer;
    }
    std::cerr << std::flush;
}


/**
 * Packet handling callback. Very simple version for now.
 * 
 * Just prints out the packet size, for debug purposes, then assumes it must be voice data.
 * 
 * This should be processing IP, UDP, and RTP and dispatching accordingly. !!!
*/
void dummy_packet_callback(const uint8_t *buf, unsigned int len)
{
    if (len == ip_v4_header_bytes+udp_header_bytes+rtp_header_bytes+opus_packet_size_bytes)
    {
        decode_and_output_audio(buf+ip_v4_header_bytes+udp_header_bytes+rtp_header_bytes, opus_packet_size_bytes, 0);
        if (config->verbose)
        {
            std::cerr << "Opus [" << len << "]" << std::endl;
        }
    }
    else
    {
        std::cerr << "Unknown packet length " << len << std::endl;
    }
}


int main(int argc, char* argv[])
{
    config = Config::parse(argc, argv);
    if (!config) return 0;

    int opus_decoder_err;    // return code from Opus function calls

    opus_decoder = ::opus_decoder_create(audio_sample_rate, 1, &opus_decoder_err);
    if (opus_decoder_err != OPUS_OK)
    {
        std::cerr << "Failed to create Opus decoder" << std::endl;
        return EXIT_FAILURE;
    }

    using FloatType = float;

    OPVDemodulator<FloatType> demod(handle_frame);
    cobs_decoder.set_packet_callback(dummy_packet_callback);

    demod.diagnostics(diagnostic_callback<FloatType>);

    while (std::cin)
    {
        int16_t sample;
        std::cin.read(reinterpret_cast<char*>(&sample), 2);
        if (std::cin.eof())
        {
            std::cerr << "Input EOF at sample " << debug_sample_count << std::endl;
            break;
        }
        if (config->invert) sample *= -1;
        demod(sample / 44000.0);    // scale 16-bit sample to [-0.74472727,0.744704545] and process
        debug_sample_count++;
    }

    std::cerr << std::endl;

    opus_decoder_destroy(opus_decoder);

    return EXIT_SUCCESS;
}
