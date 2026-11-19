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

- Not yet checked against the PDF. The PDF host was blocked in the session that wrote the
  definitions, so the layouts were transcribed from the 5.0 field tables without the
  document open. Revision date of the PDF: TODO(owner) record it when checked.
- Check: open the PDF, compare every row of `messages.hpp` (offset, length, type), then
  replace this section with "Verified against revision <date> on <date>".

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
