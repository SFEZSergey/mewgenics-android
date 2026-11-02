# app.elf: initial ARM64 migration analysis

Analysis date: 2026-11-02. Read-only inspection of the local relinked ELF.

## Binary inventory

- ELF64, little endian, x86-64, ET_DYN; 42,043,958 bytes.
- SHA256: `d3546fa1588b7e71fef3d16d31ef1e99d9237d2fe77190a989269225f96bf378`.
- Main executable PT_LOAD: file offset `0x4000`, guest VA `0`, length `0x13bd7fc`.
- 51,782 function-start entries in GNU_EH_FRAME_HDR. These are unwind entries,
  not a count of fully identified functions or a complete source inventory.
- 230 diagnostic strings mentioning glaiel, LZ4FILE or LinearAllocator;
  RIP-relative LEA byte scanning associates them with 235 candidate functions.
  Diagnostic references require instruction disassembly before treating them as names.
- The `.text` section describes only the relinker entry stub. Disassembling only
  `.text` misses the original game code: use executable PT_LOAD ranges.

All addresses below are guest virtual addresses. In the original executable
PT_LOAD, the file offset is VA + `0x4000`.

## Candidates located

| Guest VA | Diagnostic identity | Next work before interception |
| --- | --- | --- |
| `0x1101390` | LZ4FILE::readLZ4Stream | Preserve guest heap ownership and failure/assertion behavior; intercept inner decoder first |
| `0x1073e50` | Decoder called by readLZ4Stream | ARM64 safe block decoder is installed behind a build-specific code signature; retain the guest wrapper and allocation |
| `0xf84cb0` | glaiel::ByteStream::Decompress_LZ4 | Calls the same `0x1073e50` block decoder at `0xf84e63`; covered by the existing ARM64 decoder hook. Keep the guest wrapper and its allocation/state updates |
| `0xf84a10` | glaiel::ByteStream::Compress_LZ4 | Identify encoder version and flags; compressed output may differ between versions |
| `0xf08280` | glaiel::swf::Tesselator::tesselate | Recover data structures, callbacks and numerical behavior before considering native libtess2 |
| `0xeffd10` | DisplayObjectContainer_Batched::OptimizeEnd | Recover container layout, ownership and ordering semantics |
| `0xf54400` | diagnostic reference to glaiel::BitStream::readF16 | This entry is a cold diagnostic/assertion path, not a confirmed normal reader body; do not hook by this label |
| `0xf54680` | diagnostic reference to glaiel::BitStream::readEncodedU32 | This entry is a cold diagnostic/assertion path, not a confirmed normal reader body; do not hook by this label |

The LZ4FILE wrapper was disassembled. It rejects input sizes below four bytes,
reads the first little-endian u32 as output size, optionally writes that size to
the caller, allocates the output through a guest call, and calls `0x1073e50` with
`(src+4, output, input_size-4, output_size)`. It compares the returned size with
the header and invokes diagnostic handling on mismatch. The inner decoder is now
replaced by a bounds-checked ARM64 C implementation, while the wrapper, guest
allocation and mismatch handling remain untouched. The hook activates only when
the first 64 bytes at the expected guest address match the recorded FNV-1a
signature. Its portable decoder compiled for AArch64 and passed literal, match,
overlap and malformed-input vectors. The target has not been proven to be a
particular LZ4 version, and no device performance gain has been measured yet.

`ByteStream::Decompress_LZ4` also calls that exact decoder at `0xf84e63` with
`(stream_data+4, output, input_size-4, output_size)`. The same signature-checked
ARM64 hook therefore covers both game decompression paths without replacing
their distinct guest-side buffer and state management. Disassembly of the
`BitStream` diagnostic references confirms they are not safe hook addresses;
the nearby normal read helpers have no profile evidence yet, so they remain in
the guest dynarec.

## Profile limits

The existing 10-second 8 Elite combat report assigns 52.24% of CPU cycles to
app.elf's translated cache and 16.23% to the native Freedreno driver. This does
not establish the cost of the candidate functions above, or their cost on 720G.

`report-self.txt` supplies only 24 app-cache rows. Correlating its file offsets
with captured process mappings and the available perf.map resolved zero rows.
Consequently, low per-instruction percentages cannot establish that no single
guest function dominates. A complete cache-aware block map is still needed.
The collector's blockmap step failed because Termux Python was absent.

## Reproducible inventory

`tools/analyze_app_elf.py` uses Python 3 standard-library modules, reads the ELF,
and writes an inventory plus optional profile correlations. Reports should be
written outside the repository, as they may contain game diagnostic strings.

```sh
python3 tools/analyze_app_elf.py /path/to/app.elf \
  --profile /path/to/capture --output /tmp/elf-analysis.json
```

The profile directory should contain maps.txt, perf.map, and report-self.txt.
No game binary or assets were added by this analysis. The decoder replacement
is enabled only for the matching executable signature; no FPS improvement has
been measured from it yet.

## Whole-program ARM conversion

The available input is a compiled x86-64 executable with incomplete identities,
not C++ source that can be compiled with an ARM target. Whole-program migration
would require recovering the remaining code, relocations, indirect calls,
exception behavior and ABI boundaries. This inventory supports incremental
function replacement through the existing Box64 bridge; it does not establish
that a complete native ARM build is currently achievable.
