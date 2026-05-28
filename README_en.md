# AnyProtocolParser - Universal Protocol Parser

A lightweight, portable C99 protocol parser library designed for embedded systems.

## Features

- ✅ **Rule-Driven**: Define protocol structures via descriptors, no need to write parsing code manually
- ✅ **Zero-Copy Support**: Optional zero-copy mode for improved performance
- ✅ **Variable-Length Fields**: Supports TLV/LTV and other variable-length protocol formats
- ✅ **Reentrant Safety**: Supports multiple concurrent instances, thread-safe design
- ✅ **CRC Verification**: Built-in CRC-32 checksum functionality
- ✅ **Embedded Platform Optimized**: Memory alignment optimized for RISC-V32/ARM32, no bitfields
- ✅ **C99 Compatible**: Fully compliant with C99 standard, no C11 dependencies
- ✅ **Memory Control**: User-defined memory management callbacks
- ✅ **Cross-Platform**: Supports bare-metal, RTOS, Linux, Windows, and more

## Quick Start

### 1. Define Data Structure

```c
typedef struct {
    uint16_t cmd;
    uint32_t data;
    uint8_t payload[8];
} my_protocol_t;
```

### 2. Define Field Descriptors

```c
const protocol_field_descriptor_t fields[] = {
    FIELD_DESC_FIXED(my_protocol_t, cmd, FIELD_TYPE_UINT16, NULL),
    FIELD_DESC_FIXED(my_protocol_t, data, FIELD_TYPE_UINT32, NULL),
    FIELD_DESC_FIXED(my_protocol_t, payload, FIELD_TYPE_ARRARY, NULL),
};

const protocol_message_descriptor_t msg_desc = {
    .name = "my_protocol",
    .fields = fields,
    .num_fields = FIELD_ARR_SIZE(fields),
    .total_size = sizeof(my_protocol_t),
};
```

### 3. Parse Data

```c
// Initialize memory management
app_memCall_init(&memCalls);

// Parse message
parsing_user_data_t user = {NULL, 0};
protocol_err_t ret = app_parse_message(&user, &msg_desc, raw_data);
```

## Core Concepts

### Field Descriptor Macros

- `FIELD_DESC_FIXED` - Fixed-length field (automatically calculates element count)
- `FIELD_DESC_FIXED_WITH_FLAGS` - Fixed-length field with flags
- `FIELD_DESC_VAR` - Variable-length field
- `FIELD_DESC_VAR_WITH_FLAGS` - Variable-length field with flags

### Field Flags

- `FIELD_FLAG_NONE` - Default behavior (copy data to heap memory)
- `FIELD_FLAG_ZERO_COPY` - Zero-copy mode (directly pass source pointer)
- `FIELD_FLAG_BIG_ENDIAN` - Big-endian (reserved, not implemented)
- `FIELD_FLAG_LITTLE_ENDIAN` - Little-endian (reserved, not implemented)

### Variable-Length Field Modes

- `FIELD_LEN_SYMBOL` (-2) - Length specified by previous field
- `FIELD_END_SYMBOL` (-1) - Terminated by end marker (e.g., \0)
- `FIELD_ALL_REMAINING` (-3) - All remaining data

## Important Notes

### Embedded Platform Optimization

**This library is optimized for embedded platforms with strict memory alignment requirements (RISC-V32, ARM32).**

#### Key Optimizations:
- **No Bitfields**: Replaced bitfield members with full `uint8_t` types to avoid compiler compatibility issues on embedded platforms
- **Field Order Optimization**: Structured as pointers → integers → small types to minimize padding
- **Cross-Platform Packed**: Supports GCC/Clang (`__attribute__((packed))`) and MSVC (`#pragma pack(1)`)
- **Compile-Time Assertions**: Automatically verifies struct sizes at compile time

#### Memory Layout Guarantees:
| Structure | 32-bit Platform | 64-bit Platform |
|-----------|----------------|----------------|
| `protocol_field_descriptor_t` | 16 bytes (4+4+2+2+2+1+1) | 24 bytes (8+8+2+2+2+1+1) |
| `protocol_message_descriptor_t` | 22 bytes (4+4+4+4+2+4) | 38 bytes (8+8+8+8+2+4) |

#### Printf Format Compatibility:
All `printf` format specifiers use `%u` with `(unsigned int)` casts instead of `%zu`, ensuring compatibility with simplified C standard libraries commonly found in embedded environments.

---

### Byte Order Handling

**This protocol parser does NOT perform byte order conversion!**

- The parser copies raw data from the source buffer to safe heap memory, then passes it directly to user callbacks
- Byte order (big-endian/little-endian) must be pre-agreed between communicating parties
- If byte order conversion is needed, handle it in your user callback

Example:
```c
static protocol_err_t field_callback(parsing_user_data_t *_user, 
                                      const parsing_raw_data_t *_raw) {
    // Assume protocol uses big-endian, but host is little-endian
    uint16_t value = *(uint16_t*)_raw->rawStream;
    value = __builtin_bswap16(value);  // Byte order conversion
    // Use converted value
    return PROTOCOL_OK;
}
```

### Buffer Safety

**Callers MUST ensure that the input buffer is large enough to hold the entire message!**

- The parser accesses data in the buffer based on `msg_desc->total_size` or field definitions
- If the buffer length is less than the actual message length, it will cause **buffer overflow** and **undefined behavior**
- For variable-length messages, it's recommended to verify buffer length before calling the parser

Example:
```c
// Wrong approach: buffer may be incomplete
uint8_t partial_buffer[10];  // Only received partial data
app_parse_message_ex(&parser, &user, &msg_desc, partial_buffer);  // Dangerous!

// Correct approach: ensure buffer is complete
if (received_length >= msg_desc->total_size) {
    app_parse_message_ex(&parser, &user, &msg_desc, complete_buffer);
} else {
    // Wait for more data to arrive
}
```

### Memory Management

- **Copy Mode** (default): Parser allocates heap memory for each field and copies data, ensuring data won't be overwritten by subsequently received data
- **Zero-Copy Mode**: Directly passes source buffer pointer; users must ensure source data remains valid during callback and don't modify it

### Reentrancy Safety

- Legacy API (`app_parse_message`) uses global variables, doesn't support concurrency
- New API (`app_parse_message_ex`) supports multiple instance concurrency, recommended for use

```c
app_parser_instance_t parser;
app_parser_init(&parser, &memCalls);
app_parse_message_ex(&parser, &user, &msg_desc, raw_data);
app_parser_deinit(&parser);
```

### CRC Verification

```c
// Configure CRC-32
app_crc_config_t crc_cfg = {
    .calc_crc = NULL,      // Use default CRC-32
    .crc_offset = 0,       // CRC start position
    .crc_size = 4          // CRC occupies 4 bytes
};
app_parser_set_crc_config(&parser, &crc_cfg);
app_parser_enable_crc(&parser, true);

// Automatically verify CRC during parsing
protocol_err_t ret = app_parse_message_ex(&parser, &user, &msg_desc, data);
if (ret == PROTOCOL_ERR_CRC) {
    // CRC verification failed, handle error
}
```

## API Reference

### Initialization & Deinitialization

```c
protocol_err_t app_parser_init(app_parser_instance_t *parser, 
                                const parsing_memCall_t *memCalls);
protocol_err_t app_parser_deinit(app_parser_instance_t *parser);
```

### Parsing Functions

```c
// Legacy API (backward compatible)
protocol_err_t app_parse_message(parsing_user_data_t *user,
                                 const protocol_message_descriptor_t *msg_desc,
                                 const uint8_t *raw_data);

// New API (reentrant safe, recommended)
protocol_err_t app_parse_message_ex(app_parser_instance_t *parser,
                                    parsing_user_data_t *user,
                                    const protocol_message_descriptor_t *msg_desc,
                                    const uint8_t *raw_data);
```

### CRC Configuration

```c
void app_parser_set_crc_config(app_parser_instance_t *parser, 
                                const app_crc_config_t *crc_config);
void app_parser_enable_crc(app_parser_instance_t *parser, bool enable);
```

## Error Codes

| Error Code | Value | Description |
|------------|-------|-------------|
| `PROTOCOL_OK` | 0 | Success |
| `PROTOCOL_ERR_FAIL` | 1 | General failure |
| `PROTOCOL_ERR_ARG` | 2 | Argument error |
| `PROTOCOL_ERR_MEM` | 3 | Memory error |
| `PROTOCOL_ERR_PARSE` | 4 | Parse error |
| `PROTOCOL_ERR_PASSMSG` | 5 | User skip message |
| `PROTOCOL_ERR_CALLS_INIT` | 6 | Callbacks not initialized |
| `PROTOCOL_ERR_FUNCS` | 7 | Function test failed |
| `PROTOCOL_ERR_CRC` | 8 | CRC verification failed |

## Testing

The project includes comprehensive test suites covering:

### 1. Embedded Platform Alignment Tests (New)
Verifies struct compatibility on RISC-V32/ARM32 embedded platforms:
- ✅ Structure size verification (32-bit: 16/22 bytes, 64-bit: 24/38 bytes)
- ✅ Field offset checks (no compiler padding)
- ✅ Enum value storage tests (`uint8_t` type compatibility)
- ✅ Unaligned pointer access tests
- ✅ Array layout continuity verification
- ✅ Cross-platform consistency checks

### 2. Functional Tests
1. **Fixed-length message parsing**: Basic field types (UINT8/16/32/64, pointers, arrays)
2. **TLV variable-length messages**: Type-Length-Value format
3. **LTV variable-length messages**: Length-Type-Value format
4. **Zero-copy mode**: Tests `FIELD_FLAG_ZERO_COPY` flag
5. **Reentrant API**: Multi-instance concurrent parsing
6. **CRC verification**: CRC-32 validation (success/failure scenarios)

Run tests:

```bash
./build/bin/outputFile.exe
```

Expected output:

```
========================================
  Embedded Platform Alignment Tests
  Target: RISC-V32 / ARM32
========================================
[PASS] protocol_field_descriptor_t size is 24 bytes (64-bit platform)
[PASS] protocol_message_descriptor_t size is 38 bytes (64-bit platform)
[PASS] All field offsets are correct (no padding detected on 64-bit platform)
...
========================================
  Test Summary: 6/6 passed
========================================
[SUCCESS] All alignment tests passed!
Structure design is compatible with RISC-V32/ARM32.
```

## Build Options

### Enable Debug Logging

```bash
gcc -DAPP_PARSER_ENABLE_DEBUG_LOG ...
```

Or add to CMakeLists.txt:
```cmake
target_compile_definitions(outputFile PRIVATE APP_PARSER_ENABLE_DEBUG_LOG)
```

## Examples

See `sources/main.c` for complete examples, including:
- Fixed-length field parsing
- TLV/LTV variable-length protocols
- Zero-copy mode
- Multi-instance concurrency
- CRC verification
- **Embedded platform alignment tests** (see `sources/test_alignment.c`)

## Version History

**v2.1** (2026-05-28) - Embedded Platform Optimization:
- ✅ Removed bitfield design, replaced with full `uint8_t` types for RISC-V32/ARM32 compatibility
- ✅ Optimized struct field ordering (pointers → integers → small types) to reduce padding
- ✅ Added cross-platform packed macro definitions (GCC/Clang/MSVC)
- ✅ Added compile-time assertions for automatic struct size verification
- ✅ New embedded platform memory alignment test suite (6 tests)
- ✅ Improved documentation for embedded platform compatibility guarantees

**v2.0** - Major Features:
- ✅ Zero-copy mode (`FIELD_FLAG_ZERO_COPY`)
- ✅ CRC verification support (built-in CRC-32 + custom algorithms)
- ✅ Reentrant safe API (`app_parser_instance_t` + `app_parse_message_ex`)
- ✅ Field descriptor macros with flags (`FIELD_DESC_FIXED_WITH_FLAGS` / `FIELD_DESC_VAR_WITH_FLAGS`)
- ✅ Improved memory management and thread safety
- ✅ Comprehensive test suite (7 functional test scenarios)

## License

MIT License
