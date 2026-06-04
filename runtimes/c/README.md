# ads_runtime_c

Pure-C runtime library for ADS-generated ACARS decoder plugins.

Consumed by `acars-decoder-c` via CMake `add_subdirectory()` on the
`airframes-decoder` submodule:

```cmake
add_subdirectory(vendor/airframes-decoder/runtimes/c)
target_link_libraries(acars_decoder PRIVATE ads_runtime_c)
```

## Layout

```
runtimes/c/
├── CMakeLists.txt
├── README.md
├── include/
│   ├── ads_runtime.h        # core types (Message, Options, DecodeResult, Qualifiers, plugin descriptor, value/bytes/string_list)
│   ├── ads_helpers.h        # decode-fn + formatter + split/regex/substring helpers
│   └── ads_escape_hatches.h # placeholder header for per-plugin custom fns
└── src/
    ├── plugin.c             # ads_result_* lifecycle + ads_value_*
    ├── helpers.c            # decode-fn helpers (integer, float, timestamp, callsign, airport, ...)
    ├── coordinate.c         # combined NS/EW + decimal-minutes parsers
    ├── ascii85.c            # ASCII85 + base64 + hex decoders, inflate
    ├── crc.c                # CRC-16 IBM-SDLC reversed + GENIBUS
    ├── result_formatter.c   # ads_fmt_* item push helpers
    ├── string_list.c        # ads_split, ads_substring, ads_str_in
    └── regex.c              # POSIX-regex based ads_regex_* (Stage 2: replace with PCRE2 for named groups)
```

## Dependencies

- `cjson` — used internally for the raw-fields bag and JSON-encoded args parsing.
- `zlib` (optional, gates ads_inflate behind ADS_HAVE_ZLIB)
- POSIX regex (POSIX-2008+ libc). Stage 2 may replace with PCRE2 for named-capture support.

## Build

```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build .
```

For sanitizers:

```bash
cmake .. -DCMAKE_BUILD_TYPE=Asan
cmake --build .
```

## Generated plugin shape

```c
#include "ads_runtime.h"
#include "ads_helpers.h"
#include "ads_escape_hatches.h"

ads_decode_result_t *label_10_pos_decode(const ads_message_t *msg, const ads_options_t *opts);
ads_qualifiers_t label_10_pos_qualifiers(void);
extern const ads_plugin_descriptor_t label_10_pos_descriptor;
```

## Caveats / Stage 2 follow-ups

- POSIX regex doesn't support PCRE-style named groups (`(?<name>...)`). For
  the regex-using specs (Label_44_POS), `ads_regex_group("name")` currently
  resolves numeric indices only. Plan to swap in PCRE2 in Stage 2.
- Args still flow as JSON strings (`JSON.stringify(args)`); typed-args
  codegen is a v1.1 win for hot paths.
- CRC lookup tables emitted from `spec/shared/crc_tables.yaml` are pending;
  current bitwise impl is correct and table-free.
- Memory-management: `ads_value_t *` returned by decode helpers transfers
  ownership to the result on `ads_result_raw_set` (which frees the wrapper).
  Be careful in escape hatches not to double-free.
