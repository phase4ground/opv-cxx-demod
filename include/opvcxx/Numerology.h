#pragma once

#include <array>
#include <cassert>

namespace mobilinkd
{
    const int opus_bitrate = 16000;         // target output bit rate from voice encoder

    const int audio_sample_rate = 48000;     // 16-bit PCM samples per second for audio signals

    const int audio_samples_per_opus_frame = audio_sample_rate * 0.02;  // PCM samples per codec frame
    const int audio_bytes_per_opus_frame = audio_samples_per_opus_frame * 2;    // bytes of PCM sample data per codec frame

    const int audio_samples_per_opv_frame = audio_sample_rate * 0.04;  // PCM samples per audio frame (two codec frames)
    const int audio_bytes_per_opv_frame = audio_samples_per_opv_frame * 2;  // 2 bytes per PCM sample

    // (converting to) COBS/RDP/UDP/IP Frame Format for OPUlent Voice
    const int fheader_size_bytes = 12;        // bytes in a frame header (multiple of 3 for Golay encoding)
    const int encoded_fheader_size = fheader_size_bytes * 8 * 2;    // bits in an encoded frame header

    const int opus_frame_size_bytes = 40;   // bytes in an encoded 20ms Opus frame
    const int opus_packet_size_bytes = 1 + 2 * opus_frame_size_bytes;   // one byte of overhead for Opus packet
    const int rtp_header_bytes = 12;
    const int udp_header_bytes = 8;
    const int ip_header_bytes = 20;
    const int cobs_overhead_bytes = 1;
    const int total_protocol_bytes = rtp_header_bytes + udp_header_bytes + ip_header_bytes + cobs_overhead_bytes;

    const int stream_frame_payload_bytes = total_protocol_bytes + opus_packet_size_bytes;   // All frames carry this much type1 payload data
    const int stream_frame_payload_size = 8 * stream_frame_payload_bytes;
    const int stream_frame_type1_size = stream_frame_payload_size + 4;  // add encoder tail bits
    const int stream_type2_payload_size = 2 * stream_frame_type1_size;  // Encoding type1 to type2 doubles the size, plus encoder tail
    const int stream_type3_payload_size = stream_type2_payload_size;    // no puncturing in OPUlent Voice
    const int stream_type4_size = encoded_fheader_size + stream_type3_payload_size;
    const int stream_type4_bytes = stream_type4_size / 8;
    
    const int baseband_frame_symbols = 16 / 2 + stream_type4_size / 2;   // dibits or symbols in sync+payload in a frame
    const int baseband_frame_packed_bytes = baseband_frame_symbols / 4;  // packed bytes in sync_payload in a frame
    
    const int bert_frame_total_size = stream_frame_payload_size;
    const int bert_frame_prime_size = 971;      // largest prime smaller than bert_frame_total_size

    const int symbol_rate = baseband_frame_symbols / 0.04;  // symbols per second
    const int sample_rate = symbol_rate * 10;               // sample rate
    const int samples_per_frame = sample_rate * 0.04;       // samples per 40ms frame

    static_assert((stream_type3_payload_size % 8) == 0, "Type3 payload size not an integer number of bytes");
    static_assert(bert_frame_prime_size < bert_frame_total_size, "BERT prime size not less than BERT total size");
    static_assert(bert_frame_prime_size % 2 != 0, "BERT prime size not prime");
    static_assert(bert_frame_prime_size % 3 != 0, "BERT prime size not prime");
    static_assert(bert_frame_prime_size % 5 != 0, "BERT prime size not prime");
    static_assert(bert_frame_prime_size % 7 != 0, "BERT prime size not prime");
    static_assert(bert_frame_prime_size % 11 != 0, "BERT prime size not prime");
    static_assert(bert_frame_prime_size % 13 != 0, "BERT prime size not prime");
    static_assert(bert_frame_prime_size % 17 != 0, "BERT prime size not prime");
    static_assert(bert_frame_prime_size % 19 != 0, "BERT prime size not prime");
    static_assert(bert_frame_prime_size % 23 != 0, "BERT prime size not prime");
    static_assert(bert_frame_prime_size % 29 != 0, "BERT prime size not prime");
    static_assert(bert_frame_prime_size % 31 != 0, "BERT prime size not prime");
    static_assert(bert_frame_prime_size % 37 != 0, "BERT prime size not prime");
    static_assert(bert_frame_prime_size % 41 != 0, "BERT prime size not prime");

    // This interleaver polynomial was chosen by brute force search (see MATLAB
    // live script OpulentVoiceNumerology.mlx) and has a "minimum interleaving distance"
    // of 64 bits over a frame of 2152 bits (stream_type4_size). The theoretical
    // upper bound is 65 bits.
    const int PolynomialInterleaverX = 59;
    const int PolynomialInterleaverX2 = 1076;

    // This convolutional coder was adopted from M17.
    const int ConvolutionPolyA = 031;   // octal representation of taps
    const int ConvolutionPolyB = 027;   // octal representation of taps

    // Parameters for data communication
    const int ip_mtu = 1500;    // common Maximum Transmission Unit (MTU) value for Ethernet, in bytes
}