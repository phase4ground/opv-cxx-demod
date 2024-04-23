# opv-cxx-demod

Opulent Voice Modulator and Demodulator in C++ (GPL)

## opv-demod
This program is the core of an Opulent Voice receiver, but it does not directly
implement either the FM radio receiver front end or the audio output back end.
Instead, it delegates these functions to companion programs which interface with
`opv-demod` through the standard input (stdin) and standard output (stdout) streams
respectively. This architecture is convenient for development and portability, and
also reflects its origin as a prototype for a digital voice implementation that
might be dropped into an existing FM transceiver design.

The front-end receiver program, typically `rtl_fm`, tunes the radio receiver circuits
to the desired reception frequency, and demodulates any frequency modulation at that
frequency. That is to say, at a fixed sampling rate of 271,000 samples per second,
it measures the instantaneous frequency as an offset from the desired center frequency,
and provides that number as a signed 16-bit binary integer as two bytes (little-endian)
on `opv-demod`'s standard input.

`opv-demod` then completes the process of demodulating the received signal as 4-FSK
at a symbol rate of 27,100 symbols per second (one symbol per 10 samples). It then
attempts to detect frame headers in that data, dividing the data stream up into
40ms frames.

If the frames contain properly formatted Opulent Voice packets, `opv-demod` extracts
them and decodes them with the Opus codec. That produces a single-channel stream of
48,000 audio samples per second, each of which is a 16-bit signed integer. These are
written as two bytes each (little-endian) to `opv-demod`'s standard output.

The back-end audio output program, typically `play` or `aplay`, accepts these samples
as "raw" audio and takes care of playing them through a speaker.

Or, if the frames contain _bit error rate test_ (BERT) pseudo-random data instead of
voice, the data is checked and the results displayed. No back-end program is used in
this case.

Or, if the frames contain other data formats, this version of `opv-demod` does not
handle them.

Optionally, some diagnostic information is written to `stderr` while the demodulator is
running.

## opv-mod
Similarly, this program is the core of an Opulent Voice transmitter. It does not
directly implement the audio input front end or the FM radio transmitter back end.
Instead, it delegates these functions to companion programs which interface with 
`opv-mod` through the standard input (stdin) and standard output (stdout) streams
respectively.

The front-end audio input program must provide 48,000 samples per second of a single
channel of 16-bit signed integers, as two bytes (little-endian) on `opv-mod`'s
standard input. This can be as simple as the standard `cat` program replaying a
"raw" audio file. It can also be an audio conversion program like `sox` or `ffmpeg`
playing back an audio file in any common format and converting it on-the-fly into
"raw" audio. Future versions of this program will also support intermittent
"push-to-talk" audio input, exact interface TBD.

If Opulent Voice audio is to be transmitted, `opv-mod` uses the Opus codec to
convert the input audio sample stream into 40-ms Opus packets, which are wrapped
into COBS-delimited IP/UDP/RTP packets according to the Opulent Voice protocol.
In this version, most of the protocol fields are largely dummied out.

In place of audio, `opv-mod` can also generate a _bit error rate test_ (BERT)
data stream of pseudo-random bytes and pack them into 40-ms frames.

In future versions, `opv-mod` will accept IP packets (interface TBD), wraps them
in COBS framing, and chop them up into 40-ms frames.

Regardless of the data source, `opv-mod` then pre-modulates the frames into a
baseband stream of samples at 271,000 samples per second, 16-bit signed integers,
single channel, and writes them out as two bytes each (little-endian) to `stdout`.
These go to the back-end program, which is responsible for FM modulating a radio
transmission at the desired frequency. Typically, the companion program
`pluto-tx-fm` is used with an ADALM PLUTO SDR.

The output of the Pluto can be used directly on the bench, either by loosely
coupling tiny antennas or through coaxial cable plus an attenuator (70 dB is good).
If you wish to connect an antenna and attempt real communications, you will need
to add filtering and probably a power amplifier.

`opv-mod` can also skip the modulation entirely and just pack the symbols
(at 27,100 per second) into a bitstream, four to a byte (at 6,775 bytes per
second). This can be useful for simulation, or if the modulation is being
performed separately. In this mode, the bitstream can optionally be sent to
a UDP network port instead of to `stdout` by using the `--network` flag with
the `--ip` and `--port` arguments on the `opv-mod` command line.

## About the Frame Format

This version of opv-mod and opv-demod implements a complete version of the frame
format, but with most protocol fields dummied out. In this version, the voice
payload is a single 40ms Opus frame.

Each frame starts with a frame header, which contains a source callsign (encoded),
an authentication token (dummied out), and a flag indicating whether the frame
contains IP data or BERT data. This header is 12 bytes, and each run of 3 bytes
is protected by a Golay code.

The remainder of the frame is interleaved and protected with a convolutional code.
In BERT mode, this part of the frame is pseudo-random data used for a simple bit
error rate test. Otherwise, this part of the frame is part of a COBS-framed stream
of IP packets. In the common case of Opulent Voice transmission, this consists of
an IP header, a UDP header, an RTP header, and a single 40-ms Opus packet encoded
at 16,000 bits per second, all wrapped in COBS framing. This format fits exactly
into the frame, so that exactly one whole voice packet is sent in each frame. In
this version, the IP, UDP, and RTP headers are largely dummied out.

In most other cases, including general purpose data transmission, the IP packets
will not coincide with frame boundaries. The contents of successive frames are
concatenated to form a byte stream, with packet boundaries defined by COBS encoding.
In this version, this mode is not yet implemented.

Each transmission begins with a special preamble frame and ends with a special
partial frame indicating _end of transmission_ (EOT). This version of `opv_mod`
makes a single transmission that starts when the program is run and ends when the
input stream ends, or when interrupted by the user, at which time the program
also exits. This version of `opv_demod` can handle multiple transmissions, but it
doesn't do anything with the EOT. It exits when the input stream of baseband
samples ends, or when interrupted by the user.

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

## Running `opv-demod` on the air

As explained above, `opv-demod` is designed to have its standard input and
output handled by helper programs. Here is a typical command line for receiving
Opulent Voice transmissions with `opv-demod`:

```
rtl_fm -E offset -f 436.5M -M fm -s 271k | /path/to/opv-demod -d | tee received.raw | aplay -t raw -r 48000 -f S16_LE -c 1
```

The key elements for the front end are command line arguments to `rtl_fm` setting
the desired radio frequency (here 436.5 MHz) and required sample rate (271k samples/second).
The required data format is `rtl_fm`'s default, so no command line arguments are needed.

The `tee` part of the command line is optional; it just copies the received output
audio in raw format to a file, for later reference.

Then the back end program (here `aplay`) is configured to accept the audio sample
rate (48,000 samples per second) and data format ("raw" and "S16_LE", single channel)
required by `opv-demod`.

If you include the `-d` flag to `opv-demod`, diagnostic information about the
demodulation process is output to the terminal. You will want to use a terminal
window at least 132 characters wide for this display.

For bit error rate testing purposes, you can leave off the back end, and you will
probably want to skip the diagnostic display too. Like this:

```
rtl_fm -E offset -f 436.5M -M fm -s 271k | /path/to/opv-demod
```

## Running `opv-mod` on the air

As explained above, `opv-mod` is designed to have its standard input and
output handled by helper programs. Here is a typical command line for transmitting
Opulent Voice from a WAV-format audio file with `opv-mod`:

```
sox yourfile.wav -t raw -c 1 -b 16 - | /path/to/opv-mod -S KB5MU | /path/to/pluto-tx-fm -f 436500500 -s 271000 -d 6000
```

The front end program, here `sox`, takes the input filename (in any understood format)
and converts it as specified by the other command line arguments.

`opv-mod` needs a callsign to put into the frame header. Use yours, not mine (here KB5MU).

The back end program, here `pluto-tx-fm`, needs the desired RF frequency (here 436.5005 MHz),
the required sample rate (271,000 samples per second), and the standard Opulent Voice
deviation amplitude (here plus and minus 6000 Hz for inputs of 0x7FFF to 0x8001).

Note the difference between the frequency used here (436.5005 MHz) and the frequency used
in the `opv-demod` example (436.5 MHz). This is a result of manually adjusting the transmit
frequency to obtain a low frequency error on `opv-demod`'s diagnostic output (below 0.01),
with the particular set of hardware (RTL SDR and ADALM PLUTO SDR) used for development.
A future goal for `opv-demod` is to make this adjustment automatically in the receiver.

If you want to transmit for a bit error rate test only, you can omit the front end and
use the `-B` command line argument to `opv-mod` with a number of 40ms frames to send:

```
/path/to/opv-mod -S KB5MU -B 7500 | /path/to/pluto-tx-fm -f 436500500 -s 271000 -d 6000
```


## Streaming opv-mod samples directly to opv-demod for testing

If you want to leave the RF out of the picture, you can stream samples directly from
`opv-mod` to `opv-demod`. Be aware that doing that can conceal certain kinds of errors
in the programs.

```
sox yourfile.wav -t raw -c 1 -b 16 - | /path/to/opv-mod -S KB5MU | /path/to/opv-demod -d | tee received.raw | aplay -t raw -r 48000 -f S16_LE -c 1
```

Do not use `-b` to output a bitstream, since opv_demod only accepts baseband samples.


## Recording a Bitstream File for Later Playback with GNU Radio 

To output a bitstream file:
```
sox yourfile.wav -t raw -c 1 -b 16 - | /path/to/opv-mod -S KB5MU  -b > youroutfile.bin
```

The -b flag tells `opv-mod` to output a bitstream of the unmodulated symbols.

This bitstream file can be fed into a modified version of 
[m17-gnuradio](https://github.com/mobilinkd/m17-gnuradio) such as the `OPV_Impaired.grc`
flowgraph to transmit Opulent Voice using a PlutoSDR (or any SDR with an
appropriate GNU Radio sink), or perhaps loaded into a vector signal generator such as
an ESG-D Series signal generator.

## Creating Suitable Input Audio

To show off the voice quality of Opulent Voice to best effect, start with clean, high-quality
audio recordings. Make your own, or download something from the internet. You'll probably end up
with a file in a common format such as MP3 or WAV. Any such file can be converted into a suitable
raw audio stream and fed to the modulator using `sox` to convert on the fly, as shown in the
examples above. Or you can use `ffmpeg`, as shown in this example:

```
    ffmpeg -i somefile.mp3 -ar 48000 -ac 1 -f s16le -acodec pcm_s16le - | opv-mod -b -S W5NYV > somefile.bin
```

`somefile.bin` can then be used with the the `OPV_Impaired.grc` GNU Radio flow graph.

### Command Line Options

Run the programs with `--help` to see all the command-line options.

    -d causes demodulator diagnostics to be streamed to the terminal on `stderr`.

### A Note about Clock Accuracy

Note that the oscillators on the PlutoSDR and on most RTL-SDR dongles are
rather inaccurate.  You will need to have both tuned to the same frequency,
correcting for clock inaccuracies on one or both devices.

A future goal for `opv-demod` is to adjust the frequency automatically over
some range.

### A Note about Offset Tuning

Also note that you may need to use `-E offset` to decode the data well,
even though this appears in the `rtl_fm` output:

```
WARNING: Failed to set offset tuning.
```

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

