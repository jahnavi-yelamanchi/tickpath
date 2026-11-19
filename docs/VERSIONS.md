# Versions

## Specification

| Item | Value |
|---|---|
| Feed | Nasdaq TotalView-ITCH |
| Version | 5.0 |
| Source | https://www.nasdaqtrader.com/content/technicalsupport/specifications/dataproducts/NQTVITCHspecification.pdf |
| Message types | 23: S R H Y L V W K J h A F E C X D U P Q B I N O |
| Byte order | Big-endian (network order) for all integer fields |
| Integer widths | 2, 4, 6 (timestamp), 8 bytes, unsigned |
| Timestamp | 6 bytes, nanoseconds since midnight |
| Price(4) | 4-byte unsigned, 4 implied decimals |
| Price(8) | 8-byte unsigned, 8 implied decimals (MWCB decline levels only) |
| Alpha | Left-justified, space-padded |
| Offsets | Counted from the message type byte; header is type(1) locate(2) tracking(2) timestamp(6), body starts at 11 |

Field offsets and lengths are in `include/tickpath/itch/messages.hpp`. Each message has a
static_assert that its last field ends at the spec length, and `tests/unit/test_messages.cpp`
checks the length table and golden bytes.

### Verification status

Verified against the official PDF, revision log entry **April 28, 2023** (latest), on
2026-11-19. All 23 message tables match `messages.hpp`: type character, every field offset,
length and type, and total length. Byte order (big-endian), alpha padding (left-justified,
space-padded) and timestamp unit (ns since midnight) match the Data Types section.

Spec notes that affect this project:

| Item | Spec | Effect here |
|---|---|---|
| Price(4) maximum | 200,000.0000 = 2,000,000,000 (0x77359400) | `itch::max_price4`; the edge feed uses it. The book contract still requires correct handling up to 2^32-1 (corrupt input). |
| `K` IPO Quoting Period Update | Stock Locate always 0 | generator fixed to send 0 |
| `S`, `V`, `W` | Stock Locate always 0 | generator already sends 0 |
| `P` Trade (non-cross) | Order Reference Number is 0 since 2010-12-06; Buy/Sell is always `B` since 2014-07-14 | parser still validates `B`/`S`; books ignore `P` |
| `N` RPII | RPI program ceased 2014-12-31 (section shown greyed out) | message kept; real days may contain none |
| `I` NOII | Imbalance Direction `P` (Paused) added 2023-04-28; Cross Type `A` (Extended Trading Close) added 2022-01-07 | value changes only, no layout change |
| `O` DLCR | added 2023-04-28 | included |
| `h` Operational Halt | added 2018-03-03 | included |
| Transport | SoupBinTCP, compressed SoupBinTCP, MoldUDP64 | the PDF does not describe the sample-file framing; see below |

## Sample-file framing

| Item | Value |
|---|---|
| Location | https://emi.nasdaq.com/ITCH/Nasdaq%20ITCH/ (listing checked 2026-11-19) |
| File naming | `MMDDYYYY.NASDAQ_ITCH50.gz` with `.md5sum`; newer `SMMDDYY-v50.txt.gz` with `.done` |
| Compression | gzip |
| Framing | Each message is preceded by a 2-byte big-endian length; no MoldUDP64 or SoupBinTCP headers |

The framing is not verified byte-for-byte in this repository yet (the sample file was not
downloaded here). `scripts/get_data.sh` checks the first frame of the downloaded file: length 12,
type `S` (System Event).

## Dependencies

| Library | Pin | Notes |
|---|---|---|
| GoogleTest | v1.17.0 | gmock off |
| RapidCheck | 2c3c4365aca21ef4e612768fdc95b1ce8b39a651 | no release tags upstream |
| Google Benchmark | v1.9.4 | |
| HdrHistogram_c | 0.11.8 | static library, log support (zlib) off |

## Toolchain used for development

| Tool | Version |
|---|---|
| clang / clang-tidy / clang-format | 18.1.3 |
| GCC | 13.3.0 |
| CMake | 3.28.3 |
| Ninja | 1.11.1 |
