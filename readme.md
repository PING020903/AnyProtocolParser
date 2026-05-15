# 自适应协议解析器 (Adaptive Protocol Parser)

## 简介

本项目旨在设计并实现一个灵活、高效且易于扩展的自适应协议解析器。该解析器旨在解决传统硬编码解析方式在面对协议频繁变更或多种协议共存时的僵化问题，特别是在C语言等注重性能和资源控制的环境中。

**核心特性：**
- ✅ **规则驱动**：通过描述符宏定义协议结构，无需修改解析引擎
- ✅ **内存安全**：自动将 DMA/易失缓冲区数据拷贝到堆内存，防止数据覆盖
- ✅ **变长支持**：支持 TLV/LTV 等多种不定长协议格式
- ✅ **回调机制**：字段级回调函数，支持自定义处理逻辑
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
// 定长字段
FIELD_DESC_FIXED(my_protocol_t, type, FIELD_TYPE_UINT8, NULL),
FIELD_DESC_FIXED(my_protocol_t, length, FIELD_TYPE_UINT8, &length_callback),

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

```c
parsing_user_data_t user_data = {NULL, 0};
protocol_err_t ret = app_parse_message(&user_data, &msg_desc, raw_buffer);
```

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
    const char *name;                  // 字段名称
    field_type_t type;                 // 字段类型 (UINT8, UINT16, etc.)
    field_flag_t flags;                // 字段标志 (字节序等)
    uint16_t offset;                   // 在结构体中的偏移量
    int16_t itemSize;                  // 元素大小
    int16_t itemCount;                 // 元素个数 (>0:定长, <0:变长)
    const protocol_field_calls_t *calls; // 回调函数配置
} protocol_field_descriptor_t;

// 消息描述符
typedef struct {
    const char *name;                          // 消息名称
    const protocol_field_descriptor_t *fields; // 字段数组
    uint16_t num_fields;                       // 字段数量
    int32_t total_size;                        // 消息总大小
    field_callback_t on_message_start_callback;  // 消息开始回调
    field_callback_t on_message_end_callback;    // 消息结束回调
} protocol_message_descriptor_t;
```

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

## C语言实现的关键考量

在C语言中实现此设计，需要特别注意以下几点：

*   **内存管理:** 解析规则和模板需要在内存中进行表示。应谨慎设计数据结构，避免不必要的内存开销，并提供清晰的初始化和销毁接口。
    - **自定义内存回调**：通过 `parsing_memCall_t` 注入 malloc/calloc/realloc/free，便于精确控制内存
    - **链表管理**：使用嵌入式双向链表 (`ll_t`) 管理临时数据节点
    - **自动清理**：解析完成后调用 `app_managed_list_destroy()` 释放所有节点

*   **指针与缓冲区:** 状态机和解析过程将大量操作指向原始数据缓冲区的指针。必须严格遵守指针安全规范，防止越界访问。
    - **偏移量计算**：使用 `offsetof` 宏确保跨平台一致性
    - **字节序处理**：通过 `field_flag_t` 支持大端/小端强制转换
    - **柔性数组**：使用 C99 柔性数组成员 (`data[]`) 支持不定长结构

*   **宏与函数指针:** 可以利用宏来自动生成部分重复性的规则定义代码。函数指针可用于实现可插拔的校验和算法或字段解码逻辑，增强扩展性。
    - **字段描述符宏**：`FIELD_DESC_FIXED` / `FIELD_DESC_VAR` 自动生成元数据
    - **类型大小宏**：`GET_TYPE_SIZE` 编译期计算类型大小
    - **回调机制**：每个字段可配置独立的 `on_parse_callback`

*   **性能优化:** 对于热点代码路径（如状态转换、字段拷贝），应进行性能分析和优化，确保其效率。
    - **零拷贝设计**：定长字段可直接访问原始缓冲区（无回调时）
    - **按需拷贝**：仅当字段有回调时才拷贝到堆内存
    - **编译期计算**：偏移量、元素个数等在编译期确定，减少运行时开销

## 与其他语言的对比

传统的C语言解析器通常是硬编码的，一旦协议变更就需要修改源码并重新编译。而许多现代高级语言（如Python, Go）凭借其动态特性（如反射、动态数据结构）能更容易地实现类似的自适应解析。本设计试图在C语言严格的静态特性和性能要求下，通过巧妙的数据结构设计（规则表、模板）和状态机逻辑，模拟出高级语言的部分灵活性，从而弥合“C语言的性能”与“现代解析需求的灵活性”之间的鸿沟。

## 完整使用示例

### 示例 1：定长协议

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
    printf("Field size: %zu\n", _raw->streamSize);
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

## API 参考

### 核心函数

#### `app_memCall_init()`
初始化内存管理回调。

```c
protocol_err_t app_memCall_init(const parsing_memCall_t *memCalls);
```

**参数：**
- `memCalls`: 包含 malloc/calloc/realloc/free 函数指针的结构体

**返回值：**
- `PROTOCOL_OK`: 成功
- `PROTOCOL_ERR_ARG`: 参数错误
- `PROTOCOL_ERR_FUNCS`: 内存函数测试失败

---

#### `app_parse_message()`
根据消息模板解析整条报文。

```c
protocol_err_t app_parse_message(
    parsing_user_data_t *user,
    const protocol_message_descriptor_t *msg_desc,
    const uint8_t *raw_data
);
```

**参数：**
- `user`: 用户自定义数据上下文（用于在字段间传递信息）
- `msg_desc`: 消息描述符（模板）
- `raw_data`: 原始报文字节流

**返回值：**
- `PROTOCOL_OK`: 解析成功
- `PROTOCOL_ERR_ARG`: 参数错误
- `PROTOCOL_ERR_MEM`: 内存分配失败
- `PROTOCOL_ERR_PASSMSG`: 用户回调要求跳过

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
} protocol_err_t;
```

---

### 宏定义

#### `FIELD_DESC_FIXED`
定长字段描述符初始化宏。

```c
FIELD_DESC_FIXED(结构体类型, 成员名, 字段类型, 回调配置)
```

**示例：**
```c
FIELD_DESC_FIXED(my_struct_t, id, FIELD_TYPE_UINT16, NULL)
FIELD_DESC_FIXED(my_struct_t, data, FIELD_TYPE_UINT8, &my_callback)
```

---

#### `FIELD_DESC_VAR`
变长字段描述符初始化宏。

```c
FIELD_DESC_VAR(结构体类型, 成员名, 字段类型, 长度模式, 回调配置)
```

**示例：**
```c
FIELD_DESC_VAR(my_struct_t, buffer, FIELD_TYPE_UINT8, FIELD_LEN_SYMBOL, NULL)
FIELD_DESC_VAR(my_struct_t, string, FIELD_TYPE_UINT8, FIELD_END_SYMBOL, &str_callback)
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

1. **定长消息解析**：测试基本字段类型（UINT8/16/32/64、指针、数组）
2. **TLV 不定长消息**：Type-Length-Value 格式，长度表示 payload 大小
3. **LTV 不定长消息**：Length-Type-Value 格式，长度包含 type 字段

运行测试：

```bash
./build/test.exe
```

预期输出示例：

```
=== Starting protocol parser test ===

--- Test 1: Fixed-length message ---
[Field callback outputs...]
Fixed parse result: 0

--- Test 2: TLV variable-length message ---
TLV raw data (len=10):
        [a] [8] [2] [4] [8] [1] [1] [2] [4] [8]
TLV length field: value=8 (saved to user->uDataSize)
[Field callback outputs...]
TLV parse result: 0

--- Test 3: LTV variable-length message ---
LTV raw data (len=11):
        [a] [1] [8] [2] [4] [8] [1] [1] [2] [4] [8]
LTV length field: total_len=10, payload_len=9 (saved to user->uDataSize)
[Field callback outputs...]
LTV parse result: 0
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

- **零拷贝优化**：无回调的定长字段直接访问原始缓冲区
- **按需分配**：仅当字段有回调时才分配堆内存
- **编译期计算**：偏移量、元素个数等元数据在编译期确定
- **链表管理**：O(1) 时间复杂度的节点插入和删除
- **内存池友好**：可通过自定义 malloc/calloc 集成内存池

## 常见问题

### Q: 如何支持新的字段类型？
A: 在 `field_type_t` 枚举中添加新类型，并在 `GET_TYPE_SIZE` 宏中指定其字节大小。

### Q: 如何处理大端/小端转换？
A: 在字段描述符的 `flags` 成员中设置 `FIELD_FLAG_BIG_ENDIAN` 或 `FIELD_FLAG_LITTLE_ENDIAN`，然后在回调函数中进行字节序转换。

### Q: 如何在字段间传递数据？
A: 使用 `parsing_user_data_t` 结构体的 `uData` 和 `uDataSize` 成员。例如，长度字段回调将长度存入 `uDataSize`，后续变长字段从中读取。

### Q: 如何提前终止解析？
A: 在回调函数中返回 `PROTOCOL_ERR_PASSMSG`，解析器会立即停止并返回。

### Q: 是否支持嵌套协议？
A: 当前版本不支持自动嵌套解析，但可以在回调函数中手动调用 `app_parse_message()` 实现递归解析。

## 许可证

本项目采用 MIT 许可证。详见 [LICENSE](LICENSE) 文件。

## 贡献

欢迎提交 Issue 和 Pull Request！

## 致谢

- [c-linked-list](https://github.com/embeddedartistry/c-linked-list) - 嵌入式友好的双向链表库

---

**版本**: 1.0  
**最后更新**: 2026-05-15