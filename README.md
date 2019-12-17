# quic-streaming

This repository contains the modified player code, the QUIC and the TCP clients for Packet Video 2018 submission

https://dl.acm.org/citation.cfm?id=3210426

## Modules

* QUIC Client and Server (C++)
* TCP Client (C++)
* AStreamPlayerQUIC (Python)
* CsvMerger (Java)


### QUIC Client and Server

To build the **quic_server** and **quic_server**, follow the instructions at
[Google's page](https://www.chromium.org/quic/playing-with-quic)

Then replace the following files below and build again

* To disable **quic_server**'s in-memory cache:
```
quic_http_response_cache.h
quic_http_response_cache.cc
quic_simple_server_bin.cc
```

* To keep **quic_client** running when downloading segment files with same QUIC connection:
```
quic_simple_client_bin.cc
```

### TCP Client
Use only this .cpp file to build the client
```
LibCurlCppConsole.cpp
```

### AStreamPlayerQUIC

Modified from original version:  https://github.com/pari685/AStream

AStream is a Python based emulated video player to evaluate the perfomance of the DASH bitrate adaptation algorithms.

#### Command line options

```
dash_client.py [-h] [-m MPD] [-l] [-p PLAYBACK] [-n SEGMENT_LIMIT] [-d]

Process Client parameters

optional arguments:
  -h, --help            show this help message and exit
  -m MPD, --MPD MPD     Url to the MPD File
  -l, --LIST            List all the representations and quit
  -p PLAYBACK, --PLAYBACK PLAYBACK Playback type ('basic', 'sara', 'netflix', or 'all')
  -n SEGMENT_LIMIT, --SEGMENT_LIMIT SEGMENT_LIMIT The Segment number limit
  -d, --DOWNLOAD        Keep the video files after playback
  -quic, --QUIC         Use QUIC client for downloading segments
  -curl, --CURL         Use TCP client for downloading segments  
  -host, --HOST			Host Ip for Quic server
  -jump, --JUMP			Jump feature enabled
  -js, --JUMP_SCENARIO  Jump Scenario Example: -js 40->100,150->200
```
### CsvMerger
A utility to merge the player's log files into a single CSV file

