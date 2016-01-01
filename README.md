# PicoHorus Binary
This repository contains the payload code, and the supporting Octave, C, and Python utilities for the PicoHorus Binary test flight, conducted on the 2nd of Jan 2016.

The following modem files have been 'snapshotted' from David Rowe's Codec2 SVN repository, at http://svn.code.sf.net/p/freetel/code/codec2-dev/, at SVN Revision 2600:
* src/fsk_horus.m - The main 'fsk_horus' modem software (Octave).
* src/fsk_horus_stream.m - A wrapper for 'fsk_horus' allowing demodulation of audio passed in via stdin.
* src/crc16.m - CRC16-CCITT library
* src/horus_l2.c/h - Horus telemetry layer 2 processing (Golay encoding/decoding)
The latest versions of these files will always be located in the codec2-dev SVN repository. The files copied to this git repository are intended to show what was used for this particular test flight.

## Hardware Used
The Arduino code (PicoPayloadGPS) is designed to run on a Project Horus "PicoHorus" payload, but will probably run on other devices with modification. Note that the RFM22B radio module is now obsolete. There are also newer, lower power, versions of the uBlox MAX GPS unit.
This code was very much a hack-up of an older codebase, and uses some fairly ancient Arduino libraries which I've modified to work with Arduino 1.6. The main point of this flight was to test the binary telemetry, so this served the purpose fine.

## Software Dependencies
* Arduino 1.6.5 (for the PicoHorusGPS Arduino Firmware)
* Telemetry Uploader Python code was written in Python 2.7 and needs the following additional libraries:
  * crcmod  (pip install crcmod)
* GNU Octave
* GCC (To compile horus_l2.c)

## Demodulator Compilation and Configuration
First compile the horus_l2 Golay encoder/decoder.
```
$ cd src
$ gcc horus_l2.c -o horus_l2 -Wall -DDEC_RX_BITS -DHORUS_L2_RX
```
If you intend to upload demodulated data to the HABitat online tracker (http://tracker.habhub.org/), you will need to modify lines 30 and 32 in fsk_horus_stream.m to the following

```
% Upload Telemetry to Habitat (http://tracker.habhub.org/)
telem_upload_enabled = true;
% Update this command with your own callsign.
telem_upload_command = "python telem_upload.py -c YOURCALLSIGNHERE_Octave";
```

## Usage - Demodulator & Telemetry Upload
fsk_horus_stream.m takes 8KHz sample rate, 16-bit audio samples on stdin, and outputs results to the terminal and Habitat (if enabled). Under linux, you can get audio samples using 'rec' or 'arecord'.

First, chmod fsk_horus_stream.m so it can be executed:
```$ chmod +x fsk_horus_stream.m```

Under Linux, use one of the following to record samples and pipe into fsk_horus_stream:
```$ rec -t raw -r 8000 -s -2 -c 1 - -q | ./fsk_horus_stream.m ```
or
```$ arecord -D pulse -r 8000 -c 1 -f S16_LE - | ./fsk_horus_stream.m ```

Under OSX, you should be able to obtain sox (which contains the 'rec' utility), via macports.