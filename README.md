# PicoHorus Binary
This repository contains the payload code, and the supporting Python utilities for the PicoHorus Binary test flight, conducted on the 2nd of Jan 2016.

The fsk_horus modem is located in David Rowe's SVN repository, at http://svn.code.sf.net/p/freetel/code/codec2-dev/octave/ , and is intended to be run in Octave, with some C to handle the payload decoding.

## Dependencies
* Arduino code was compiled in Arduino 1.6.5
* Python code was written in Python 2.7 and needs the following additional libraries:
  * crcmod  (pip install crcmod)

## Hardware
The Arduino code (PicoPayloadGPS) is designed to run on a Project Horus "PicoHorus" payload, but will probably run on other devices with modification. Note that the RFM22B radio module is now obsolete. There are also newer, lower power, versions of the uBlox MAX GPS unit.
This code was very much a hack-up of an older codebase, and uses some fairly ancient Arduino libraries which I've modified to work with Arduino 1.6. The main point of this flight was to test the binary telemetry, so this served the purpose fine.

## Usage - Telemetry Uploader
This is used to upload demodulated telemetry (from the fsk_horus modem) to Habitat. It will accept either a hexadecimal representation of the binary payload (refer PicoPayloadGPS/PicoPayloadGPS.ino for the format), or the 'classic' ASCII payload sentence (beginning with 'HORUS').

Call with:
* python telem_upload.py --callsign MYCALL <payload data here>
