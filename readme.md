# 自适应协议解析器 (Adaptive Protocol Parser)

[🇺🇸 English Version](README_en.md)

## 简介

本项目旨在设计并实现一个灵活、高效且易于扩展的自适应协议解析器。该解析器旨在解决传统硬编码解析方式在面对协议频繁变更或多种协议共存时的僵化问题，特别是在C语言等注重性能和资源控制的环境中。

**核心特性：**
- ✅ **规则驱动**：通过描述符宏定义协议结构，无需修改解析引擎
- ✅ **内存安全**：自动将 DMA/易失缓冲区数据拷贝到堆内存，防止数据覆盖
- ✅ **变长支持**：支持 TLV/LTV 等多种不定长协议格式
- ✅ **回调机制**：字段级回调函数，支持自定义处理逻辑
- ✅ **零拷贝优化**：可选零拷贝模式，提升高性能场景性能
- ✅ **CRC 校验**：内置 CRC-32 支持，可自定义校验算法
- ✅ **重入安全**：支持多实例并发解析，线程安全设计
- ✅ **零依赖**：纯 C99 实现，仅需标准库
- ✅ **跨平台**：支持 GCC、Clang、MSVC 等主流编译器

## 快速开始

### 1. 定义协议结构

```c
// 定义你的协议数据结构
typedef struct {
    uint8_t  type;      // 类型字段
    uint8_t  length;    // 长度字段
    uint8_t  data[];    // 柔性数组（不定长数据）
} my_protocol_t;
```

### 2. 配置字段描述符

```c
// 定长字段（默认拷贝模式）
FIELD_DESC_FIXED(my_protocol_t, type, FIELD_TYPE_UINT8, NULL),
FIELD_DESC_FIXED(my_protocol_t, length, FIELD_TYPE_UINT8, &length_callback),

// 定长字段（零拷贝模式）
FIELD_DESC_FIXED_WITH_FLAGS(my_protocol_t, id, FIELD_TYPE_UINT16, FIELD_FLAG_ZERO_COPY, &id_callback),

// 变长字段（关联前一个长度字段）
FIELD_DESC_VAR(my_protocol_t, data, FIELD_TYPE_UINT8, FIELD_LEN_SYMBOL, &data_callback),
```

### 3. 创建消息模板

```c
const protocol_message_descriptor_t msg_desc = {
    .name = "my_protocol",
    .fields = fields_array,
    .num_fields = FIELD_ARR_SIZE(fields_array),
    .total_size = -1,  // 变长消息
};
```

### 4. 解析数据

#### 方式 A：旧版 API（简单场景）
```c
parsing_user_data_t user_data = {NULL, 0};
protocol_err_t ret = app_parse_message(&user_data, &msg_desc, raw_buffer);
```

**特点：**
- ✅ 使用简单，无需管理实例生命周期
- ✅ 适合单线程、简单应用场景
- ❌ **不支持 CRC 校验**
- ❌ **使用全局变量，非线程安全**（多线程并发会导致数据竞争）

---

#### 方式 B：新版 API（复杂场景）
```c
// 初始化解析器实例
app_parser_instance_t parser;
app_parser_init(&parser, &mem_calls);

// 可选：配置 CRC 校验
app_crc_config_t crc_cfg = {
    .calc_crc = NULL,  // 使用默认 CRC-32
    .crc_offset = 0,
    .crc_size = 4
};
app_parser_set_crc_config(&parser, &crc_cfg);
app_parser_enable_crc(&parser, true);

// 解析消息
parsing_user_data_t user_data = {NULL, 0};
protocol_err_t ret = app_parse_message_ex(&parser, &user_data, &msg_desc, raw_buffer);

// 清理实例
app_parser_deinit(&parser);
```

**特点：**
- ✅ 支持 CRC 校验
- ✅ 基于实例，支持多线程并发解析
- ✅ 可配置多个独立解析器实例
- ❌ 需要手动管理实例生命周期
- ❌ 代码稍复杂

## 设计目标

*   **自适应性 (Adaptability):** 解析器能够根据外部定义的规则或配置动态调整其解析行为，无需重新编译核心代码即可适配协议的变化。
*   **高效性 (Efficiency):** 在保证灵活性的同时，尽可能维持接近硬编码的解析性能，适用于对性能敏感的场景。
*   **可扩展性 (Extensibility):** 设计应允许轻松添加新的协议类型、字段类型或解析规则。
*   **清晰性 (Clarity):** 解析规则的定义应当直观易懂，降低维护和修改的复杂度。

## 核心设计理念

### 1. 规则驱动 (Rule-Driven)
解析器的核心逻辑不再硬编码在程序中，而是由一组可读的**解析规则 (Parsing Rules)** 驱动。这些规则定义了：
*   协议消息的起始标志 (Start Delimiter)。
*   消息的长度字段位置及解码方式 (Length Field)。
*   消息体内各字段的类型、偏移量、长度和含义 (Field Definitions)。
*   协议消息的结束标志 (End Delimiter)。
*   校验和/校验码的计算方式和位置 (Checksum)。

**实现方式：** 通过 `FIELD_DESC_FIXED` 和 `FIELD_DESC_VAR` 宏定义字段描述符数组，编译期自动生成元数据。

### 2. 状态机 (State Machine)
采用有限状态机 (FSM) 模型来管理解析过程。状态机根据接收到的数据和当前状态，结合解析规则，决定下一个状态和要执行的动作（如读取固定字节数、读取变长字段、计算校验和等）。这种模型非常适合处理流式数据。

**实现方式：** 通过遍历字段描述符数组，逐个解析字段，支持提前终止 (`PROTOCOL_ERR_PASSMSG`)。

### 3. 描述符/模板 (Descriptors/Templates)
将解析规则实例化为一种内部数据结构，称为**消息模板 (Message Template)** 或**描述符 (Descriptor)**。每个已知的协议消息类型都对应一个这样的模板。模板包含了所有必要的元数据，供解析引擎在运行时查询和使用。

**核心结构：**
```c
// 字段描述符
typedef struct {
    const char *name;                          // 字段名称
    const protocol_field_calls_t *calls;       // 回调函数配置指针
    uint16_t offset;                           // 在结构体中的偏移量
    int16_t itemSize;                          // 元素大小
    int16_t itemCount;                         // 元素个数 (>0:定长, <0:变长)
    uint8_t type;                              // 字段类型枚举值
    uint8_t flags;                             // 字段标志位
} protocol_field_descriptor_t;

// 消息描述符
typedef struct {
    const char *name;                          // 消息名称
    const protocol_field_descriptor_t *fields; // 字段数组
    field_callback_t on_message_start_callback;  // 消息开始回调
    field_callback_t on_message_end_callback;    // 消息结束回调
    uint16_t num_fields;                       // 字段数量
    int32_t total_size;                        // 消息总大小
} protocol_message_descriptor_t;
```

**嵌入式平台优化：**
- ✅ **移除位域**：使用完整 `uint8_t` 替代位域，避免 RISC-V32/ARM32 编译器兼容性问题
- ✅ **字段顺序优化**：指针 → 整数 → 小类型，减少 padding
- ✅ **跨平台 packed**：支持 GCC/Clang (`__attribute__((packed))`) 和 MSVC (`#pragma pack(1)`)
- ✅ **内存布局保证**：
  - 32-bit 平台：`protocol_field_descriptor_t` = 16 字节，`protocol_message_descriptor_t` = 22 字节
  - 64-bit 平台：`protocol_field_descriptor_t` = 24 字节，`protocol_message_descriptor_t` = 38 字节
- ✅ **编译时断言**：自动验证结构体大小，确保跨平台一致性

### 4. 抽象与分层 (Abstraction & Layering)
将解析过程分为几个层次：
*   **数据接收层:** 负责从底层（串口、网络等）接收原始字节流。
*   **协议识别层:** 根据规则匹配数据流的开头，确定消息所属的协议类型，并选择相应的解析模板。
*   **字段解析层:** 根据选定的模板，逐个解析消息体内的字段。
*   **数据交付层:** 将解析出的结构化数据交付给上层应用程序。

**内存安全机制：**
- 所有带回调的字段数据都会从原始缓冲区（可能是 DMA）拷贝到堆内存
- 使用双向链表管理临时数据节点
- 解析完成后自动释放所有节点，防止内存泄漏

**零拷贝优化：**
- 通过 `FIELD_FLAG_ZERO_COPY` 标志启用零拷贝模式
- 适用于高性能场景，直接传递原始缓冲区指针给回调
- 用户需确保在回调期间原始缓冲区保持有效且不被修改

**CRC 校验支持：**
- 内置 CRC-32 (IEEE 802.3) 默认实现
- 支持自定义 CRC 计算和验证函数
- 可在解析前自动验证消息完整性
- 支持 1/2/4 字节 CRC 字段

## C语言实现的关键考量

在C语言中实现此设计，需要特别注意以下几点：

*   **内存管理:** 解析规则和模板需要在内存中进行表示。应谨慎设计数据结构，避免不必要的内存开销，并提供清晰的初始化和销毁接口。
    - **自定义内存回调**：通过 `parsing_memCall_t` 注入 malloc/calloc/realloc/free，便于精确控制内存
    - **链表管理**：使用嵌入式双向链表 (`ll_t`) 管理临时数据节点
    - **自动清理**：解析完成后调用 `app_managed_list_destroy()` 释放所有节点
    - **实例化设计**：新版 API 使用 `app_parser_instance_t` 支持多实例并发，避免全局状态

*   **指针与缓冲区:** 状态机和解析过程将大量操作指向原始数据缓冲区的指针。必须严格遵守指针安全规范，防止越界访问。
    - **偏移量计算**：使用 `offsetof` 宏确保跨平台一致性
    - **字节序处理**：通过 `field_flag_t` 支持大端/小端强制转换
    - **柔性数组**：使用 C99 柔性数组成员 (`data[]`) 支持不定长结构

*   **宏与函数指针:** 可以利用宏来自动生成部分重复性的规则定义代码。函数指针可用于实现可插拔的校验和算法或字段解码逻辑，增强扩展性。
    - **字段描述符宏**：`FIELD_DESC_FIXED` / `FIELD_DESC_VAR` 自动生成元数据
    - **带标志位宏**：`FIELD_DESC_FIXED_WITH_FLAGS` / `FIELD_DESC_VAR_WITH_FLAGS` 支持零拷贝等高级特性
    - **类型大小宏**：`GET_TYPE_SIZE` 编译期计算类型大小
    - **回调机制**：每个字段可配置独立的 `on_parse_callback`
    - **CRC 回调**：支持自定义 CRC 计算和验证函数

*   **性能优化:** 对于热点代码路径（如状态转换、字段拷贝），应进行性能分析和优化，确保其效率。
    - **零拷贝设计**：无回调的定长字段可直接访问原始缓冲区；有回调但启用 `FIELD_FLAG_ZERO_COPY` 时直接传递源指针
    - **按需拷贝**：仅当字段有回调且未启用零拷贝时才拷贝到堆内存
    - **编译期计算**：偏移量、元素个数等元数据在编译期确定，减少运行时开销
    - **重入安全**：基于实例的设计避免全局锁，支持多线程并发解析

## 与其他语言的对比

传统的C语言解析器通常是硬编码的，一旦协议变更就需要修改源码并重新编译。而许多现代高级语言（如Python, Go）凭借其动态特性（如反射、动态数据结构）能更容易地实现类似的自适应解析。本设计试图在C语言严格的静态特性和性能要求下，通过巧妙的数据结构设计（规则表、模板）和状态机逻辑，模拟出高级语言的部分灵活性，从而弥合“C语言的性能”与“现代解析需求的灵活性”之间的鸿沟。

## 重要说明

### 嵌入式平台 Printf 格式兼容性

**本项目所有 printf 格式符已针对嵌入式环境优化！**

- ✅ 使用 `%u` 替代 `%zu`（size_t 格式符）
- ✅ 所有 `sizeof()` 和 `offsetof()` 结果都转换为 `(unsigned int)`
- ✅ 兼容简化 C 标准库的嵌入式环境（RISC-V32、ARM32 等）

示例：
```c
// 正确做法（嵌入式兼容）
printf("Size: %u\n", (unsigned int)sizeof(my_struct));
printf("Offset: %u\n", (unsigned int)offsetof(my_struct, field));

// 避免使用（某些嵌入式环境不支持）
printf("Size: %zu\n", sizeof(my_struct));  // ❌
```

---

### 字节序处理

**本协议解析器不进行字节序转换！**

- 解析器将原始数据从源缓冲区拷贝到安全的堆内存后，直接传递给用户回调
- 数据的字节序（大端/小端）在通讯双方必须预先协定
- 如需字节序转换，请在用户回调中自行处理

示例：
```c
static protocol_err_t field_callback(parsing_user_data_t *_user, 
                                      const parsing_raw_data_t *_raw) {
    // 假设协议约定为大端序，而主机为小端序
    uint16_t value = *(uint16_t*)_raw->rawStream;
    value = __builtin_bswap16(value);  // 字节序转换
    // 使用转换后的值
    return PROTOCOL_OK;
}
```

### 缓冲区安全

**调用者必须确保传入的原始数据缓冲区足够大，能够容纳完整的报文！**

- 解析器会根据 `msg_desc->total_size` 或字段定义访问缓冲区中的数据
- 如果缓冲区长度小于报文实际长度，会导致**缓冲区溢出**和**未定义行为**
- 对于变长报文，建议在调用解析前先验证缓冲区长度

示例：
```c
// 错误做法：缓冲区可能不完整
uint8_t partial_buffer[10];  // 只接收到部分数据
app_parse_message_ex(&parser, &user, &msg_desc, partial_buffer);  // 危险！

// 正确做法：确保缓冲区完整
if (received_length >= msg_desc->total_size) {
    app_parse_message_ex(&parser, &user, &msg_desc, complete_buffer);
} else {
    // 等待更多数据到达
}
```

### 内存管理

- **拷贝模式**（默认）：解析器会为每个字段分配堆内存并拷贝数据，确保数据不会被后续接收的新数据覆盖
- **零拷贝模式**：直接传递源缓冲区指针，用户需保证源数据在回调期间有效且不修改数据

### 重入安全性

- 旧版 API (`app_parse_message`) 使用全局变量，不支持并发
- 新版 API (`app_parse_message_ex`) 支持多实例并发，推荐使用

```c
app_parser_instance_t parser;
app_parser_init(&parser, &memCalls);
app_parse_message_ex(&parser, &user, &msg_desc, raw_data);
app_parser_deinit(&parser);
```

### CRC 校验

```c
// 配置 CRC-32
app_crc_config_t crc_cfg = {
    .calc_crc = NULL,      // 使用默认 CRC-32
    .crc_offset = 0,       // CRC 起始位置
    .crc_size = 4          // CRC 占 4 字节
};
app_parser_set_crc_config(&parser, &crc_cfg);
app_parser_enable_crc(&parser, true);

// 解析时自动校验 CRC
protocol_err_t ret = app_parse_message_ex(&parser, &user, &msg_desc, data);
if (ret == PROTOCOL_ERR_CRC) {
    // CRC 校验失败，处理错误
}
```

## 完整使用示例

### 示例 1：定长协议（旧版 API - 简单场景）

**适用场景：** 单线程应用、快速原型开发、不需要 CRC 校验的简单协议

```c
#include "AnyProtocolParser.h"
#include <stdlib.h>

// 1. 定义协议结构
typedef struct {
    uint16_t header;
    uint32_t timestamp;
    uint8_t  status;
    uint8_t  data[16];
} fixed_protocol_t;

// 2. 定义字段回调（可选）
static protocol_err_t field_callback(parsing_user_data_t *_user, 
                                     const parsing_raw_data_t *_raw) {
    // 处理字段数据（_raw->rawStream 指向堆内存中的安全副本）
    printf("Field size: %u\n", (unsigned int)_raw->streamSize);
    return PROTOCOL_OK;
}

static const protocol_field_calls_t field_calls = {
    .on_parse_callback = field_callback,
};

// 3. 配置字段描述符
const protocol_field_descriptor_t fields[] = {
    FIELD_DESC_FIXED(fixed_protocol_t, header, FIELD_TYPE_UINT16, &field_calls),
    FIELD_DESC_FIXED(fixed_protocol_t, timestamp, FIELD_TYPE_UINT32, &field_calls),
    FIELD_DESC_FIXED(fixed_protocol_t, status, FIELD_TYPE_UINT8, NULL),
    FIELD_DESC_FIXED(fixed_protocol_t, data, FIELD_TYPE_UINT8, &field_calls),
};

// 4. 创建消息模板
const protocol_message_descriptor_t msg_desc = {
    .name = "fixed_protocol",
    .fields = fields,
    .num_fields = FIELD_ARR_SIZE(fields),
    .total_size = sizeof(fixed_protocol_t),
};

// 5. 初始化内存管理
static void *parser_malloc(size_t bytes) { return malloc(bytes); }
static void *parser_calloc(size_t n, size_t s) { return calloc(n, s); }
static void *parser_realloc(void *p, size_t s) { return realloc(p, s); }
static void parser_free(void *p) { free(p); }

static const parsing_memCall_t mem_calls = {
    .malloc = parser_malloc,
    .calloc = parser_calloc,
    .realloc = parser_realloc,
    .free = parser_free,
};

// 6. 解析数据
int main(void) {
    app_memCall_init(&mem_calls);
    
    fixed_protocol_t raw_data = {
        .header = 0x1234,
        .timestamp = 1234567890,
        .status = 0x01,
        .data = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15},
    };
    
    parsing_user_data_t user_data = {NULL, 0};
    protocol_err_t ret = app_parse_message(
        &user_data, 
        &msg_desc, 
        (const uint8_t *)&raw_data
    );
    
    return ret == PROTOCOL_OK ? 0 : 1;
}
```

### 示例 1+：定长协议（新版 API - 需要 CRC 或多线程）

**适用场景：** 多线程环境、需要数据完整性校验、工业控制等可靠性要求高的场景

```c
#include "AnyProtocolParser.h"
#include <stdlib.h>

// ... [协议结构、回调、字段描述符、消息模板定义同上] ...

// 5. 初始化内存管理
static void *parser_malloc(size_t bytes) { return malloc(bytes); }
static void *parser_calloc(size_t n, size_t s) { return calloc(n, s); }
static void *parser_realloc(void *p, size_t s) { return realloc(p, s); }
static void parser_free(void *p) { free(p); }

static const parsing_memCall_t mem_calls = {
    .malloc = parser_malloc,
    .calloc = parser_calloc,
    .realloc = parser_realloc,
    .free = parser_free,
};

// 6. 使用重入安全 API 解析
int main(void) {
    // 初始化解析器实例
    app_parser_instance_t parser;
    if (app_parser_init(&parser, &mem_calls) != PROTOCOL_OK) {
        return -1;
    }
    
    // 配置 CRC-32 校验（假设消息末尾 4 字节为 CRC）
    app_crc_config_t crc_cfg = {
        .calc_crc = NULL,      // 使用内置 CRC-32
        .verify_crc = NULL,
        .crc_offset = 0,       // 从开头计算 CRC
        .crc_size = 4          // CRC 占 4 字节
    };
    app_parser_set_crc_config(&parser, &crc_cfg);
    app_parser_enable_crc(&parser, true);
    
    fixed_protocol_t raw_data = {
        .header = 0x1234,
        .timestamp = 1234567890,
        .status = 0x01,
        .data = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15},
    };
    
    parsing_user_data_t user_data = {NULL, 0};
    protocol_err_t ret = app_parse_message_ex(
        &parser,
        &user_data, 
        &msg_desc, 
        (const uint8_t *)&raw_data
    );
    
    if (ret == PROTOCOL_ERR_CRC) {
        printf("CRC verification failed!\n");
    }
    
    app_parser_deinit(&parser);
    return ret == PROTOCOL_OK ? 0 : 1;
}
```

### 示例 2：TLV 不定长协议

```c
// 1. 定义 TLV 结构
typedef struct {
    uint8_t type;
    uint8_t length;
    uint8_t data[];  // 柔性数组
} tlv_protocol_t;

// 2. 长度字段回调：保存长度值供后续字段使用
static protocol_err_t tlv_len_callback(parsing_user_data_t *_user, 
                                       const parsing_raw_data_t *_raw) {
    uint8_t len = *(uint8_t*)_raw->rawStream;
    _user->uDataSize = len;  // 保存到用户上下文
    return PROTOCOL_OK;
}

static const protocol_field_calls_t len_calls = {
    .on_parse_callback = tlv_len_callback,
};

// 3. 配置字段描述符
const protocol_field_descriptor_t tlv_fields[] = {
    FIELD_DESC_FIXED(tlv_protocol_t, type, FIELD_TYPE_UINT8, NULL),
    FIELD_DESC_FIXED(tlv_protocol_t, length, FIELD_TYPE_UINT8, &len_calls),
    // 变长字段：FIELD_LEN_SYMBOL 表示关联前一个字段的长度
    FIELD_DESC_VAR(tlv_protocol_t, data, FIELD_TYPE_UINT8, FIELD_LEN_SYMBOL, NULL),
};

// 4. 创建消息模板
const protocol_message_descriptor_t tlv_msg_desc = {
    .name = "tlv_protocol",
    .fields = tlv_fields,
    .num_fields = FIELD_ARR_SIZE(tlv_fields),
    .total_size = -1,  // 变长消息
};
```

### 示例 3：LTV 不定长协议（长度包含 Type）

```c
// LTV 结构：Length-Type-Value
typedef struct {
    uint8_t length;   // 总长度（包含 type + data）
    uint8_t type;
    uint8_t data[];
} ltv_protocol_t;

// 长度字段回调：计算 payload 长度 = 总长度 - type字段大小
static protocol_err_t ltv_len_callback(parsing_user_data_t *_user, 
                                       const parsing_raw_data_t *_raw) {
    uint8_t total_len = *(uint8_t*)_raw->rawStream;
    _user->uDataSize = total_len - 1;  // 减去 type 字段的 1 字节
    return PROTOCOL_OK;
}

const protocol_field_descriptor_t ltv_fields[] = {
    FIELD_DESC_FIXED(ltv_protocol_t, length, FIELD_TYPE_UINT8, &ltv_len_callback),
    FIELD_DESC_FIXED(ltv_protocol_t, type, FIELD_TYPE_UINT8, NULL),
    FIELD_DESC_VAR(ltv_protocol_t, data, FIELD_TYPE_UINT8, FIELD_LEN_SYMBOL, NULL),
};
```

### 示例 4：零拷贝模式

```c
// 1. 定义协议结构
typedef struct {
    uint16_t id;
    uint32_t value;
    uint8_t  data[32];
} zero_copy_protocol_t;

// 2. 零拷贝回调（直接访问原始缓冲区，不拷贝）
static protocol_err_t zero_copy_callback(parsing_user_data_t *_user, 
                                         const parsing_raw_data_t *_raw) {
    // 注意：_raw->rawStream 直接指向原始缓冲区
    // 不应修改数据，且需确保缓冲区在回调期间有效
    printf("[Zero-Copy] Field size: %u\n", (unsigned int)_raw->streamSize);
    return PROTOCOL_OK;
}

static const protocol_field_calls_t zc_calls = {
    .on_parse_callback = zero_copy_callback,
};

// 3. 使用带标志位的宏启用零拷贝
const protocol_field_descriptor_t zc_fields[] = {
    FIELD_DESC_FIXED_WITH_FLAGS(zero_copy_protocol_t, id, FIELD_TYPE_UINT16, 
                                FIELD_FLAG_ZERO_COPY, &zc_calls),
    FIELD_DESC_FIXED_WITH_FLAGS(zero_copy_protocol_t, value, FIELD_TYPE_UINT32, 
                                FIELD_FLAG_ZERO_COPY, &zc_calls),
    // 混合使用：这个字段仍然使用拷贝模式
    FIELD_DESC_FIXED_WITH_FLAGS(zero_copy_protocol_t, data, FIELD_TYPE_UINT8, 
                                FIELD_FLAG_NONE, &zc_calls),
};

const protocol_message_descriptor_t zc_msg_desc = {
    .name = "zero_copy_protocol",
    .fields = zc_fields,
    .num_fields = FIELD_ARR_SIZE(zc_fields),
    .total_size = sizeof(zero_copy_protocol_t),
};
```

## API 参考

### 核心函数

#### `app_memCall_init()`（旧版 API）
初始化内存管理回调（全局状态）。

```c
protocol_err_t app_memCall_init(const parsing_memCall_t *memCalls);
```

**适用场景：**
- 单线程应用（如 Arduino、简单的嵌入式设备）
- 不需要 CRC 校验的场景
- 快速原型开发或学习用途
- 资源受限且无多线程需求的系统

**限制：**
- ❌ **非线程安全**：内部使用全局变量，多线程并发会导致数据竞争
- ❌ **无 CRC 支持**：无法进行数据完整性校验

**参数：**
- `memCalls`: 包含 malloc/calloc/realloc/free 函数指针的结构体

**返回值：**
- `PROTOCOL_OK`: 成功
- `PROTOCOL_ERR_ARG`: 参数错误
- `PROTOCOL_ERR_FUNCS`: 内存函数测试失败

**注意：** 
- ⚠️ 此 API 内部使用全局变量（`g_memCalls`、`g_parseData`），**不是线程安全的**
- ⚠️ 多线程环境下同时调用会导致数据竞争和未定义行为
- ⚠️ 不支持 CRC 校验功能

---

#### `app_parser_init()`（新版 API）
初始化解析器实例（支持重入和多实例并发）。

```c
protocol_err_t app_parser_init(app_parser_instance_t *parser, 
                                const parsing_memCall_t *memCalls);
```

**适用场景：**
- 多线程/多任务环境
- 需要 CRC 校验的应用
- 需要同时解析多种协议的网关
- 工业控制、通信设备等可靠性要求高的场景

**参数：**
- `parser`: 解析器实例指针
- `memCalls`: 内存管理回调结构体

**返回值：**
- `PROTOCOL_OK`: 成功
- `PROTOCOL_ERR_ARG`: 参数错误
- `PROTOCOL_ERR_FUNCS`: 内存函数测试失败

---

#### `app_parser_deinit()`
反初始化解析器实例，释放内部资源。

```c
protocol_err_t app_parser_deinit(app_parser_instance_t *parser);
```

**参数：**
- `parser`: 解析器实例指针

**返回值：**
- `PROTOCOL_OK`: 成功
- `PROTOCOL_ERR_ARG`: 参数错误

---

#### `app_parser_set_crc_config()`
配置 CRC 校验参数。

```c
void app_parser_set_crc_config(app_parser_instance_t *parser, 
                                const app_crc_config_t *crc_config);
```

**参数：**
- `parser`: 解析器实例指针
- `crc_config`: CRC 配置结构体
  - `calc_crc`: CRC 计算函数（NULL 使用默认 CRC-32）
  - `verify_crc`: CRC 验证函数（可选）
  - `crc_offset`: CRC 字段在消息中的偏移
  - `crc_size`: CRC 字段大小（1/2/4 字节）

---

#### `app_parser_enable_crc()`
启用或禁用 CRC 校验。

```c
void app_parser_enable_crc(app_parser_instance_t *parser, bool enable);
```

**参数：**
- `parser`: 解析器实例指针
- `enable`: true 启用，false 禁用

---

#### `app_parse_message()`（旧版 API）
根据消息模板解析整条报文（全局状态）。

```c
protocol_err_t app_parse_message(
    parsing_user_data_t *user,
    const protocol_message_descriptor_t *msg_desc,
    const uint8_t *raw_data
);
```

**适用场景：**
- 单线程应用
- 简单的传感器数据采集
- 不需要数据完整性校验的场景
- 资源受限的嵌入式设备（减少代码复杂度）

**参数：**
- `user`: 用户自定义数据上下文（用于在字段间传递信息）
- `msg_desc`: 消息描述符（模板）
- `raw_data`: 原始报文字节流

**返回值：**
- `PROTOCOL_OK`: 解析成功
- `PROTOCOL_ERR_ARG`: 参数错误
- `PROTOCOL_ERR_MEM`: 内存分配失败
- `PROTOCOL_ERR_PASSMSG`: 用户回调要求跳过
- `PROTOCOL_ERR_CALLS_INIT`: 内存回调未初始化

**注意：** 不支持 CRC 校验，不适合多线程环境。

---

#### `app_parse_message_ex()`（新版 API）
根据消息模板解析整条报文（重入安全版本）。

```c
protocol_err_t app_parse_message_ex(
    app_parser_instance_t *parser,
    parsing_user_data_t *user,
    const protocol_message_descriptor_t *msg_desc,
    const uint8_t *raw_data
);
```

**适用场景：**
- 多线程/RTOS 环境
- 需要 CRC 校验的通信协议
- 多协议网关（可同时创建多个解析器实例）
- 工业自动化、汽车电子等高可靠性场景

**参数：**
- `parser`: 解析器实例指针
- `user`: 用户自定义数据上下文（用于在字段间传递信息）
- `msg_desc`: 消息描述符（模板）
- `raw_data`: 原始报文字节流

**返回值：**
- `PROTOCOL_OK`: 解析成功
- `PROTOCOL_ERR_ARG`: 参数错误
- `PROTOCOL_ERR_MEM`: 内存分配失败
- `PROTOCOL_ERR_PASSMSG`: 用户回调要求跳过
- `PROTOCOL_ERR_CRC`: CRC 校验失败
- `PROTOCOL_ERR_CALLS_INIT`: 内存回调未初始化

---

### 数据类型

#### 字段类型枚举 (`field_type_t`)
```c
typedef enum {
    FIELD_TYPE_UINT8,
    FIELD_TYPE_INT8,
    FIELD_TYPE_UINT16,
    FIELD_TYPE_INT16,
    FIELD_TYPE_UINT32,
    FIELD_TYPE_INT32,
    FIELD_TYPE_UINT64,
    FIELD_TYPE_INT64,
    FIELD_TYPE_FLOAT,
    FIELD_TYPE_DOUBLE,
    FIELD_TYPE_POINT,
    FIELD_TYPE_ARRARY,
} field_type_t;
```

#### 变长模式枚举 (`itemCountMode_t`)
```c
typedef enum {
    FIELD_ALL_REMAINING = -3,  // 剩余所有数据
    FIELD_LEN_SYMBOL = -2,     // 关联前一个字段长度
    FIELD_END_SYMBOL = -1,     // 遇到结束符（如 \0）
    FIELD_FIXED_LEN = 1        // 定长（语义参考）
} itemCountMode_t;
```

#### 错误码枚举 (`protocol_err_t`)
```c
typedef enum {
    PROTOCOL_OK = 0,
    PROTOCOL_ERR_FAIL,
    PROTOCOL_ERR_ARG,
    PROTOCOL_ERR_MEM,
    PROTOCOL_ERR_PARSE,
    PROTOCOL_ERR_PASSMSG,
    PROTOCOL_ERR_CALLS_INIT,
    PROTOCOL_ERR_FUNCS,
    PROTOCOL_ERR_CRC,      // CRC 校验失败
} protocol_err_t;
```

---

### 宏定义

#### `FIELD_DESC_FIXED`
定长字段描述符初始化宏（默认拷贝模式）。

```c
FIELD_DESC_FIXED(结构体类型, 成员名, 字段类型, 回调配置)
```

**示例：**
```c
FIELD_DESC_FIXED(my_struct_t, id, FIELD_TYPE_UINT16, NULL)
FIELD_DESC_FIXED(my_struct_t, data, FIELD_TYPE_UINT8, &my_callback)
```

---

#### `FIELD_DESC_FIXED_WITH_FLAGS`
定长字段描述符初始化宏（带标志位，支持零拷贝等高级特性）。

```c
FIELD_DESC_FIXED_WITH_FLAGS(结构体类型, 成员名, 字段类型, 标志位, 回调配置)
```

**示例：**
```c
// 启用零拷贝模式
FIELD_DESC_FIXED_WITH_FLAGS(my_struct_t, id, FIELD_TYPE_UINT16, 
                            FIELD_FLAG_ZERO_COPY, &my_callback)

// 强制大端序
FIELD_DESC_FIXED_WITH_FLAGS(my_struct_t, value, FIELD_TYPE_UINT32, 
                            FIELD_FLAG_BIG_ENDIAN, NULL)
```

---

#### `FIELD_DESC_VAR`
变长字段描述符初始化宏（默认拷贝模式）。

```c
FIELD_DESC_VAR(结构体类型, 成员名, 字段类型, 长度模式, 回调配置)
```

**示例：**
```c
FIELD_DESC_VAR(my_struct_t, buffer, FIELD_TYPE_UINT8, FIELD_LEN_SYMBOL, NULL)
FIELD_DESC_VAR(my_struct_t, string, FIELD_TYPE_UINT8, FIELD_END_SYMBOL, &str_callback)
```

---

#### `FIELD_DESC_VAR_WITH_FLAGS`
变长字段描述符初始化宏（带标志位）。

```c
FIELD_DESC_VAR_WITH_FLAGS(结构体类型, 成员名, 字段类型, 长度模式, 标志位, 回调配置)
```

**示例：**
```c
FIELD_DESC_VAR_WITH_FLAGS(my_struct_t, payload, FIELD_TYPE_UINT8, 
                          FIELD_LEN_SYMBOL, FIELD_FLAG_ZERO_COPY, &payload_callback)
```

---

### 回调函数类型

```c
typedef protocol_err_t (*field_callback_t)(
    parsing_user_data_t *_user,      // 用户上下文
    const parsing_raw_data_t *_raw   // 原始数据视图
);
```

**参数：**
- `_user`: 用户自定义数据，可在字段间传递信息
  - `_user->uData`: 通用指针
  - `_user->uDataSize`: 通用大小（常用于传递长度值）
- `_raw`: 原始数据视图
  - `_raw->rawStream`: 指向数据的指针（已拷贝到堆内存）
  - `_raw->streamSize`: 数据字节数

**返回值：**
- `PROTOCOL_OK`: 继续解析下一个字段
- `PROTOCOL_ERR_PASSMSG`: 跳过剩余字段，立即返回
- 其他错误码: 终止解析并返回错误

## 适用场景

*   需要处理多种不同通信协议的网关或路由器固件。
*   协议版本迭代较快，需要快速响应的嵌入式设备。
*   需要在运行时加载或切换协议定义的动态系统。
*   从 DMA 缓冲区接收数据，需要确保数据安全的场景。
*   工业控制系统中的自定义通信协议解析。
*   IoT 设备中的多协议适配层。

## 编译与构建

### 依赖要求

- **C 编译器**：支持 C99 标准的 GCC、Clang 或 MSVC
- **CMake**：3.10 或更高版本
- **可选**：MinGW（Windows 环境）

### 使用 CMake 构建

```bash
# 创建构建目录
mkdir build
cd build

# 配置项目
cmake ..

# 编译
cmake --build .
```

### 使用 GCC 直接编译

```bash
gcc -std=c99 -I include \
    sources/AnyProtocolParser.c \
    sources/main.c \
    -o parser_test
```

### Windows (MinGW)

```powershell
# 使用提供的批处理脚本
.\build.bat

# 或者手动编译
gcc -I include -I thirdparty/c-linked-list-main/src/linkedlist `
    sources/AnyProtocolParser.c `
    sources/main.c `
    -o build/test.exe
```

## 测试

项目包含完整的测试用例，覆盖以下场景：

### 1. 嵌入式平台内存对齐测试（新增）
验证结构体在 RISC-V32/ARM32 等嵌入式平台的兼容性：
- ✅ 结构体大小验证（32-bit: 16/22 字节，64-bit: 24/38 字节）
- ✅ 字段偏移量检查（无编译器自动填充 padding）
- ✅ 枚举值存储测试（`uint8_t` 类型兼容）
- ✅ 非对齐指针访问测试
- ✅ 数组布局连续性验证
- ✅ 跨平台一致性检查

### 2. 功能测试
1. **定长消息解析**：测试基本字段类型（UINT8/16/32/64、指针、数组）
2. **TLV 不定长消息**：Type-Length-Value 格式，长度表示 payload 大小
3. **LTV 不定长消息**：Length-Type-Value 格式，长度包含 type 字段
4. **零拷贝模式**：测试 `FIELD_FLAG_ZERO_COPY` 标志的使用
5. **重入安全 API**：测试多实例并发解析
6. **CRC 校验**：测试 CRC-32 验证功能（成功/失败场景）

运行测试：

```bash
./build/bin/outputFile.exe
```

预期输出示例：

```
========================================
  Embedded Platform Alignment Tests
  Target: RISC-V32 / ARM32
========================================
=== Structure Layout Information ===
Platform: 64-bit (pointer size = 8 bytes)

--- protocol_field_descriptor_t ---
Total size: 24 bytes (expected: 24 for 64-bit)
  name:    offset=0, size=8
  calls:   offset=8, size=8
  ...

=== Test 1: Structure Size Verification ===
[PASS] protocol_field_descriptor_t size is 24 bytes (64-bit platform)
[PASS] protocol_message_descriptor_t size is 38 bytes (64-bit platform)

=== Test 2: Field Offset Verification (No Padding) ===
[PASS] All field offsets are correct (no padding detected on 64-bit platform)

...

========================================
  Test Summary: 6/6 passed
========================================
[SUCCESS] All alignment tests passed!
Structure design is compatible with RISC-V32/ARM32.

=== Starting protocol parser functional tests ===
...
```

## 项目结构

```
AnyProtocolParser/
├── include/
│   ├── AnyProtocolParser.h    # 核心头文件（API、宏定义、数据结构）
│   └── DBG_macro.h            # 调试宏定义
├── sources/
│   ├── AnyProtocolParser.c    # 解析器实现
│   └── main.c                 # 测试用例
├── thirdparty/
│   └── c-linked-list-main/    # 第三方双向链表库（MIT 协议）
├── CMakeLists.txt             # CMake 构建配置
├── build.bat                  # Windows 构建脚本
└── readme.md                  # 本文档
```

## 性能特性

- **零拷贝优化**：通过 `FIELD_FLAG_ZERO_COPY` 启用零拷贝模式，直接传递原始缓冲区指针
- **按需分配**：仅当字段有回调且未启用零拷贝时才分配堆内存
- **编译期计算**：偏移量、元素个数等元数据在编译期确定
- **链表管理**：O(1) 时间复杂度的节点插入和删除
- **内存池友好**：可通过自定义 malloc/calloc 集成内存池
- **重入安全**：基于实例的设计避免全局锁，支持多线程并发解析
- **CRC 校验**：内置 CRC-32 默认实现，支持自定义算法

## 常见问题

### Q: 如何支持新的字段类型？
A: 在 `field_type_t` 枚举中添加新类型，并在 `GET_TYPE_SIZE` 宏中指定其字节大小。

### Q: 如何处理大端/小端转换？
A: 在字段描述符的 `flags` 成员中设置 `FIELD_FLAG_BIG_ENDIAN` 或 `FIELD_FLAG_LITTLE_ENDIAN`，然后在回调函数中进行字节序转换。或者使用 `FIELD_DESC_FIXED_WITH_FLAGS` 宏直接指定标志位。

### Q: 如何在字段间传递数据？
A: 使用 `parsing_user_data_t` 结构体的 `uData` 和 `uDataSize` 成员。例如，长度字段回调将长度存入 `uDataSize`，后续变长字段从中读取。

### Q: 如何提前终止解析？
A: 在回调函数中返回 `PROTOCOL_ERR_PASSMSG`，解析器会立即停止并返回。

### Q: 是否支持嵌套协议？
A: 当前版本不支持自动嵌套解析，但可以在回调函数中手动调用 `app_parse_message_ex()` 实现递归解析。

### Q: 什么是零拷贝模式？何时使用？
A: 零拷贝模式通过 `FIELD_FLAG_ZERO_COPY` 启用，解析器直接将原始缓冲区指针传递给回调函数，不进行内存拷贝。适用于：
- 高性能场景，减少内存分配和拷贝开销
- 只读访问字段数据
- 用户能确保原始缓冲区在回调期间保持有效

注意：零拷贝模式下不应修改 `_raw->rawStream` 指向的数据。

### Q: 如何使用 CRC 校验？
A: 
1. 初始化解析器实例：`app_parser_init(&parser, &mem_calls)`
2. 配置 CRC 参数：
   ```c
   app_crc_config_t crc_cfg = {
       .calc_crc = NULL,      // 使用默认 CRC-32
       .crc_offset = 0,
       .crc_size = 4
   };
   app_parser_set_crc_config(&parser, &crc_cfg);
   app_parser_enable_crc(&parser, true);
   ```
3. 解析消息时会自动进行 CRC 验证
4. 如果返回 `PROTOCOL_ERR_CRC`，表示校验失败

### Q: 如何选择旧版 API 还是新版 API？

A: 根据应用场景选择：

**使用旧版 API (`app_memCall_init` + `app_parse_message`) 当：**
- ✅ 确定的单线程应用（如简单的 Arduino 项目、裸机嵌入式系统）
- ✅ 不需要 CRC 校验
- ✅ 追求代码简洁，减少资源占用
- ✅ 快速原型开发或学习用途
- ✅ 资源极度受限且无线程概念的嵌入式设备

**⚠️ 重要警告：**
- ❌ **绝对不要在多线程环境中使用旧版 API**
- ❌ 内部使用全局变量 `g_memCalls` 和 `g_parseData`，多线程并发会导致数据竞争
- ❌ 如果在 RTOS 或多线程系统中使用，必须确保同一时刻只有一个线程调用解析函数

**使用新版 API (`app_parser_init` + `app_parse_message_ex`) 当：**
- ✅ 多线程/RTOS 环境（如 FreeRTOS、ThreadX）
- ✅ 需要 CRC 校验保证数据完整性
- ✅ 多协议网关（需要同时解析多种协议）
- ✅ 工业控制、汽车电子等高可靠性场景
- ✅ 需要在不同任务中使用独立的解析器实例

**性能对比：**
- 两者在字段解析性能上基本相同
- 新版 API 多了实例管理的微小开销（约几十字节内存/实例）
- 旧版 API 代码更简洁，但**有线程安全风险**
- **关键区别**：旧版 API 使用全局变量，新版 API 基于实例

### Q: 如何实现自定义 CRC 算法？
A: 提供自己的 CRC 计算函数，并在配置时传入：
```c
uint32_t my_crc_calc(const uint8_t *data, size_t len) {
    // 实现你的 CRC 算法
    return crc_value;
}

app_crc_config_t crc_cfg = {
    .calc_crc = my_crc_calc,
    .crc_offset = 0,
    .crc_size = 2  // 例如 CRC-16
};
```

## 许可证

本项目采用 MIT 许可证。详见 [LICENSE](LICENSE) 文件。

## 贡献

欢迎提交 Issue 和 Pull Request！

## 致谢

- [c-linked-list](https://github.com/embeddedartistry/c-linked-list) - 嵌入式友好的双向链表库

---

**版本**: 2.1  
**最后更新**: 2026-05-28

**v2.1 主要更新（嵌入式平台优化）：**
- ✅ 移除位域设计，改用完整 `uint8_t` 类型，避免 RISC-V32/ARM32 编译器兼容性问题
- ✅ 优化结构体字段顺序（指针 → 整数 → 小类型），减少 padding
- ✅ 添加跨平台 packed 宏定义（支持 GCC/Clang/MSVC）
- ✅ 添加编译时断言，自动验证结构体大小
- ✅ 新增嵌入式平台内存对齐测试套件（6 项测试）
- ✅ 完善文档说明嵌入式平台兼容性保证

**v2.0 主要更新：**
- ✅ 新增零拷贝模式（`FIELD_FLAG_ZERO_COPY`）
- ✅ 新增 CRC 校验支持（内置 CRC-32 + 自定义算法）
- ✅ 新增重入安全 API（`app_parser_instance_t` + `app_parse_message_ex`）
- ✅ 新增带标志位的字段描述符宏（`FIELD_DESC_FIXED_WITH_FLAGS` / `FIELD_DESC_VAR_WITH_FLAGS`）
- ✅ 改进内存管理和线程安全性
- ✅ 完善测试用例（7 个功能测试场景）