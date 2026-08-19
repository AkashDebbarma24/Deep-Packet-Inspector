# DPI Engine

A multi-threaded Deep Packet Inspection engine written in C++17. Reads network traffic from a
PCAP file, identifies which application generated each connection — even over encrypted HTTPS —
and can block, forward, and log traffic based on configurable rules.

> 📖 For a full walkthrough of the networking concepts, architecture, and code — written as a
> teaching document — see **[DOCS.md](DOCS.md)**.

---

## What It Does

```
PCAP file → [DPI Engine] → Filtered PCAP + Reports
                 │
                 ├── Identifies apps (YouTube, GitHub, Discord, etc.) from TLS SNI / HTTP Host
                 ├── Blocks or forwards traffic based on rules
                 ├── Logs every domain seen to a CSV, with counts and timestamps
                 └── Prints a live processing report
```

Even fully encrypted HTTPS traffic reveals its destination domain during the TLS handshake (the
Server Name Indication field) — this engine extracts that, classifies the traffic, and acts on it,
without ever decrypting the connection.

## Features

- **Application identification** from TLS SNI and HTTP Host headers — YouTube, Facebook, GitHub,
  Discord, Spotify, Zoom, and more, easily extensible
- **Rule-based blocking** of specific apps or domains
- **Multi-threaded pipeline**: reader → load balancers → fast-path workers → writer, scaling
  across configurable thread counts
- **Thread-safe SNI logging** — every domain seen is tallied and flushed to CSV periodically by a
  background thread, without blocking packet processing
- **Two engine modes**: a simple single-threaded version for learning the flow, and a
  multi-threaded version for production-scale captures

## Architecture

```
Reader Thread → Load Balancer(s) → Fast Path Worker(s) → Output Writer
                                          │
                                          └──> SNILogger (background flush thread)
```

Packets are read once, hashed onto load balancer queues, and processed in parallel by fast-path
workers that do protocol parsing, SNI extraction, classification, and rule matching. Domain hits
are reported to a single shared `SNILogger` instance, which batches writes to disk so no worker
thread ever blocks on file I/O.

## Quick Start

**Prerequisites:** a C++17 compiler (g++ or clang++) and pthreads.

```bash
# Build the multi-threaded engine
g++ -std=c++17 -pthread -O2 -I include -o dpi_engine \
    src/dpi_mt.cpp \
    src/pcap_reader.cpp \
    src/packet_parser.cpp \
    src/sni_extractor.cpp \
    src/types.cpp \
    src/sni_logger.cpp

# Generate sample traffic (optional — a test_dpi.pcap is included)
python3 generate_test_pcap.py

# Run it
./dpi_engine test_dpi.pcap output.pcap
```

This produces a console report and an `sni_counts.csv` file with every domain seen, its count,
and the last time it appeared:

```
domain,count,last_seen
www.youtube.com,1,2023-11-14 22:13:27
github.com,1,2023-11-14 22:13:51
```

Windows users: see [WINDOWS_SETUP.md](WINDOWS_SETUP.md) for getting a compiler set up via MSYS2.

## Project Structure

```
include/          # Class declarations
src/
  main.cpp          # Simple entry point (CMake target)
  main_working.cpp  # Single-threaded reference implementation
  dpi_mt.cpp        # ★ Multi-threaded production engine ★
  sni_extractor.*   # TLS SNI / HTTP Host parsing
  sni_logger.*      # Thread-safe domain frequency logging
  ...
CMakeLists.txt    # Builds the single-threaded target
```

The multi-threaded engine (`dpi_mt.cpp`) isn't yet wired into CMake — build it with the manual
command above.

## Learn More

- **[DOCS.md](DOCS.md)** — full technical deep-dive: networking background, packet structure,
  thread architecture, TLS handshake parsing, and a complete walk-through of every component
- **[WINDOWS_SETUP.md](WINDOWS_SETUP.md)** — compiler setup for Windows via MSYS2

## Ideas for Extending

- Add more app signatures (Twitch, WhatsApp, etc.)
- QUIC / HTTP3 support
- Persistent rule sets loaded from file
- Live stats dashboard

---

Built as an exploration of deep packet inspection, multi-threaded C++ design, and TLS traffic
analysis. See [DOCS.md](DOCS.md) for the full story of how it works.
