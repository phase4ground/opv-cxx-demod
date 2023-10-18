# opv-cxx-demod

Opulent Voice Modulator and Demodulator in C++ (GPL)

## opv-demod
This program reads a 187k samples per second 16-bit signed integer little-endian, single
channel, Opulent Voice 4-FSK baseband stream from `stdin` and demodulates it. If the
stream contains voice data, it is decoded with Opus, and the resulting audio is written
to `stdout` as a 16-bit signed single channel audio stream at 48k samples per second.
If the stream contains _bit error rate test_ (BERT) pseudo-random data instead of voice,
the data is checked and the results displayed.

Some diagnostic information is written to `stderr` while the demodulator is
running.

## opv-mod
This program reads in an 48k sample per second, 16-bit signed integer, 1 channel raw audio
stream from `stdin` (or generates BERT data itself) and writes out an Opulent Voice 4-FSK
baseband stream at 187k samples per second, 16-bit signed integer, 1 channel to `stdout`.

It can also skip the modulation and output a bitstream of symbols, packed four per byte.
This can be useful for simulation, or if the modulation is being performed separately.
In this mode, the bitstream can optionally be sent to a UDP network port instead of
to `stdout`.

The input can also be generated pseudo-randomly in _bit error rate test_ (BERT) mode
instead of from voice audio. 

## About the Frame Format

This version of opv-mod and opv-demod implements a simplified version of the frame
format, used for initial testing. In this version, the voice payload is simply two
20ms Opus frames concatenated. The two Opus frames are not combined into a proper
Opus packet, or wrapped in any additional protocol layers.

Each frame starts with a frame header, which contains a source callsign (encoded),
an authentication token (dummied out), and a flag indicating whether the frame
contains voice data or BERT data. This header is followed by the voice data or
by BERT data.

Each transmission begins with a special preamble frame and ends with a special
partial frame indicating _end of transmission_ (EOT). This version of opv_mod
makes a single transmission that starts when the program is run and ends when the
input stream ends, at which time the program also exits. This version of opv_demod
can handle multiple transmissions, but doesn't do anything with the EOT. It exits
when the input stream of baseband samples ends.

## Build

### Prerequisites

This code requires the libopus-dev, boost-devel, gtest-devel, and cmake packages be installed.

On some systems, boost-devel is called libboost-dev, and you also need to install
libboost-program-optionsX.XX-dev, where X.XX matches the version number of libboost-dev.
When you run `sudo apt install libboost-dev`, watch the output for suggested packages
to find the exact name.

On some systems, gtest-devel is called libgtest-dev.

It also requires a modern C++17 compiler (GCC 8 minimum).

For Raspberry Pi OS as of version 5 (bookworm), try this:
```
sudo apt install libopus-dev
sudo apt install libboost-dev libboost-program-options-dev
sudo apt install libgtest-dev
sudo apt install cmake
```

## Build Steps (Linux etc.)
```
    mkdir build
    cd build
    cmake ..
    make
    make test
    sudo make install
```

## Running opv-demod on the air

The opv_demod program was designed to be used with RTL-SDR, specifically rtl-fm.
This works but is not fully documented at this time. The command line below
should be a good start.

```
    rtl_fm -E offset -f 905.05M -s 187k | opv-demod | play -b 16 -r 48000 -c1 -t s16 -
```

You should run this in a terminal window that is 132 characters wide. It
will output diagnostic information on a single line in the window.

## Streaming opv-mod samples directly to opv-demod for testing

    sox ~/audio/brain.wav -t raw - | opv-mod -S KB5MU | opv-demod -d | play -q -b 16 -r 48000 -c1 -t s16 -

The input audio stream must be 1 channel, 16-bit signed integer, 48000 samples per second.

Use `-S <callsign>` to set your source (callsign), which is mandatory.

Do not use `-b` to output a bitstream, since opv_demod only accepts baseband samples.

Use `-h` to see the full help.

The output of the modulator is 187ksps, 16-bit signed integer, 1 channel raw samples.

## Recording a Bitstream File

To output a bitstream file:
```
    sox ~/audio/brain.wav -t raw - | ./opv-mod -S KB5MU -b > opv.bin
```

The -b flag tells opv-mod to output a bitstream of the unmodulated symbols.

This bitstream file can be fed into a modified version of 
[m17-gnuradio](https://github.com/mobilinkd/m17-gnuradio) such as the `M17_Impaired.grc`
flowgraph to transmit (simplified) Opulent Voice using a PlutoSDR (or any SDR with an
appropriate GNU Radio sink), or perhaps loaded into a vector signal generator such as
an ESG-D Series signal generator.

## Creating Suitable Input Audio

To show off the voice quality of Opulent Voice to best effect, start with clean, high-quality
audio recordings. Make your own, or download something from the internet. You'll probably end up
with a file in a common format such as MP3 or WAV. Any such file can be converted into a suitable
raw audio stream and fed to the modulator with a command line like this:

```
    ffmpeg -i somefile.mp3 -ar 48000 -ac 1 -f s16le -acodec pcm_s16le - | opv-mod -b -S W5NYV > somefile.bin
```

This can then be used with the the `M17_Impaired.grc` GNU Radio flow graph.

### Command Line Options

Run the programs with `--help` to see all the command-line options.

    -d causes demodulator diagnostics to be streamed to the terminal on `stderr`.

### A Note about Clock Accuracy

Note that the oscillators on the PlutoSDR and on most RTL-SDR dongles are
rather inaccurate.  You will need to have both tuned to the same frequency,
correcting for clock inaccuracies on one or both devices.

### A Note about Offset Tuning

Also note that I need to use `-E offset` to decode the data well, even though
I see this in the `rtl_fm` output:

    WARNING: Failed to set offset tuning.

This was tested using the [m17-gnuradio](https://github.com/mobilinkd/m17-gnuradio)
GNU Radio block feeding an Analog Devices 
[ADALM Pluto SDR](https://www.analog.com/en/design-center/evaluation-hardware-and-software/evaluation-boards-kits/adalm-pluto.html),
modulating the m17.bin file from the
[m17-demodulator](https://github.com/mobilinkd/m17-demodulator) repo.

## Demodulator Diagnostic

```
THIS SECTION APPLIES TO THE M17 VERSION AND HAS NOT BEEN UPDATED
```

The demodulator diagnostics are calibrated for the following command:

    rtl_fm -F 9 -f 144.91M -s 18k | sox -t s16 -r 18k -c1 - -t raw - gain 9 rate -v -s 48k | ./apps/m17-demod -d -l | play -b 16 -r 8000 -c1 -t s16 -

Specifically, the initial rate (18k samples per second) and the conversion rate and gain (gain of 9,
output rate 48k samples per second) are important for deviation and frequency offset.

The demodulator produces diagnostic output which looks like:

    SRC: BROADCAST, DEST: MBLKDTNC3, TYPE: 0002, NONCE: 0000000000000000000000000000, CRC: bb9b
    dcd: 1, evm:    13.27%, deviation:   0.9857, freq offset:  0.03534, locked:   true, clock:        1, sample: 0, 0, 0, cost: 9

The first line shows the received link information.  The second line contains the following diagnostics.

 - **DCD** -- data carrier detect -- uses a DFT to detect baseband energy.  Very good at detecting whether data may be there.
 - **EVM** -- error vector magnitude -- measure the Std Dev of the offset from ideal for each symbol.
 - **Deviation** -- normalized to 1.0 to mean 2400Hz deviation for the input from the following command:
    `rtl_fm -F 9 -f 144.91M -s 18k | sox -t s16 -r 18k -c1 - -t raw - gain 9 rate -v -s 48k` -- the rates and gain are the important part.
 - **Frequency Offset** -- the estimated frequency offset from 0 based on the DC level of each symbol.  The magnitude has
    not been measured but should be around 1.0 == 2kHz using the same normalized input as for deviation.  Anything > 0.1
    or less than -0.1 requires review/calibration of the TX and RX frequencies.
 - **Locked** -- sync word detected. 
 - **Clock** -- estimated difference between TX and RX clock.  Will not work if > 500ppm.  Normalized to 1.0 -- meaning the clocks are equal.
 - **Sample** -- estimated sample point based on 2 clock recovery methods.  Should agree to within 1 always.  There are
    10 samples per symbol.  Should never jump by more than 1 per frame.  The third number is the "winner" based on
    certain heuristics.
 - **Cost** -- the normalized Viterbi cost estimate for decoding the frame.  < 5 great, < 15 good, < 30 OK, < 50 bad, > 80 you're hosed.

## BER Testing

When transmitting a BER test, the diagnostics line will show additional information.

    dcd: 1, evm:    2.626%, deviation:   0.9318, freq offset:   -0.154, locked:   true, clock:        1, sample: 2, 2, 2, cost: 2, BER: 0.000000 (53190 bits)

The BER rate and number of bits received are also displayed.

## Thanks

Thanks to [Rob Riggs of Mobilinkd LLC](https://github.com/mobilinkd) for the M17 implementation upon which this code is based.

