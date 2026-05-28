#pragma once
#ifndef _ANYPROTOCOLPARSER_H_
#define _ANYPROTOCOLPARSER_H_

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C"
{
#endif

/* 最大字段数限制 */
#define APP_MAX_FIELDS 32
/* 最大协议模板数限制 */
#define APP_MAX_TEMPLATES 16
/* 最大比特位数 */
#define APP_MAX_BITS 8

#ifndef offsetof
#define offsetof(s, m) ((size_t)&(((s *)0)->m))
#endif

#ifndef FIELD_SIZE
#define FIELD_SIZE(type, member) (sizeof(((type *)0)->member))
#endif

#ifndef FIELD_NAME
#define FIELD_NAME(x) #x
#endif

#ifndef FIELD_ARR_SIZE
#define FIELD_ARR_SIZE(_arr) sizeof(_arr) / sizeof((_arr)[0])
#endif

/* 跨平台对齐处理 */
#if defined(_MSC_VER)
#define APP_PACKED_STRUCT(name) __pragma(pack(push, 1)) typedef struct name
#define APP_PACKED_END(name) \
    name;                    \
    __pragma(pack(pop))
#elif defined(__GNUC__) || defined(__clang__)
#define APP_PACKED_STRUCT(name) typedef struct __attribute__((packed)) name
#define APP_PACKED_END(name) name;
#else
#define APP_PACKED_STRUCT(name) typedef struct name
#define APP_PACKED_END(name) name;
#endif

#ifndef GET_TYPE_SIZE
/* 根据字段类型枚举获取对应的字节大小 */
#define GET_TYPE_SIZE(f_type)                                                                                                                                     \
    ((f_type == FIELD_TYPE_UINT8 || f_type == FIELD_TYPE_INT8) ? 1 : (f_type == FIELD_TYPE_UINT16 || f_type == FIELD_TYPE_INT16)                              ? 2 \
                                                                 : (f_type == FIELD_TYPE_UINT32 || f_type == FIELD_TYPE_INT32 || f_type == FIELD_TYPE_FLOAT)  ? 4 \
                                                                 : (f_type == FIELD_TYPE_UINT64 || f_type == FIELD_TYPE_INT64 || f_type == FIELD_TYPE_DOUBLE) ? 8 \
                                                                                                                                                              : 1) /* 指针或未知类型默认按 1 字节处理 */
#endif

#ifndef FIELD_ITEM_COUNT_SAFE
/* 安全计算元素个数：总字节数 / 单个元素字节数 */
#define FIELD_ITEM_COUNT_SAFE(_type, _member, f_type) \
    (sizeof(((_type *)0)->_member) / GET_TYPE_SIZE(f_type))
#endif

#ifndef FIELD_DESC_FIXED
/*
 * 定长字段描述符初始化宏 (自动计算元素个数)
 * @param _type: 结构体类型名
 * @param _member: 结构体成员名
 * @param f_type: 字段数据类型枚举 (如 FIELD_TYPE_UINT16)
 * @param _calls: 回调函数配置指针 (protocol_field_calls_t*)，可为 NULL
 */
#define FIELD_DESC_FIXED(_type, _member, f_type, _calls)                      \
    {                                                                         \
        .name = #_member,                                                     \
        .type = f_type,                                                       \
        .flags = FIELD_FLAG_NONE,                                             \
        .itemSize = GET_TYPE_SIZE(f_type),                                    \
        .itemCount = (sizeof(((_type *)0)->_member) / GET_TYPE_SIZE(f_type)), \
        .offset = offsetof(_type, _member),                                   \
        .calls = (_calls)}
#endif

#ifndef FIELD_DESC_FIXED_WITH_FLAGS
/*
 * 定长字段描述符初始化宏 (带标志位)
 * @param _type: 结构体类型名
 * @param _member: 结构体成员名
 * @param f_type: 字段数据类型枚举 (如 FIELD_TYPE_UINT16)
 * @param _flags: 字段标志位 (如 FIELD_FLAG_ZERO_COPY)
 * @param _calls: 回调函数配置指针 (protocol_field_calls_t*)，可为 NULL
 */
#define FIELD_DESC_FIXED_WITH_FLAGS(_type, _member, f_type, _flags, _calls)   \
    {                                                                         \
        .name = #_member,                                                     \
        .type = f_type,                                                       \
        .flags = (_flags),                                                    \
        .itemSize = GET_TYPE_SIZE(f_type),                                    \
        .itemCount = (sizeof(((_type *)0)->_member) / GET_TYPE_SIZE(f_type)), \
        .offset = offsetof(_type, _member),                                   \
        .calls = (_calls)}
#endif

#ifndef FIELD_DESC_VAR
/*
 * 变长字段描述符初始化宏
 * @param _type: 结构体类型名
 * @param _member: 结构体成员名
 * @param f_type: 字段数据类型枚举 (如 FIELD_TYPE_UINT8)
 * @param mode: 长度模式枚举 (FIELD_LEN_SYMBOL, FIELD_END_SYMBOL, FIELD_ALL_REMAINING)
 * @param _calls: 回调函数配置指针 (protocol_field_calls_t*)，可为 NULL
 */
#define FIELD_DESC_VAR(_type, _member, f_type, mode, _calls) \
    {                                                        \
        .name = #_member,                                    \
        .type = f_type,                                      \
        .flags = FIELD_FLAG_NONE,                            \
        .itemSize = GET_TYPE_SIZE(f_type),                   \
        .itemCount = (mode),                                 \
        .offset = offsetof(_type, _member),                  \
        .calls = (_calls)}
#endif

#ifndef FIELD_DESC_VAR_WITH_FLAGS
/*
 * 变长字段描述符初始化宏 (带标志位)
 * @param _type: 结构体类型名
 * @param _member: 结构体成员名
 * @param f_type: 字段数据类型枚举 (如 FIELD_TYPE_UINT8)
 * @param mode: 长度模式枚举 (FIELD_LEN_SYMBOL, FIELD_END_SYMBOL, FIELD_ALL_REMAINING)
 * @param _flags: 字段标志位 (如 FIELD_FLAG_ZERO_COPY)
 * @param _calls: 回调函数配置指针 (protocol_field_calls_t*)，可为 NULL
 */
#define FIELD_DESC_VAR_WITH_FLAGS(_type, _member, f_type, mode, _flags, _calls) \
    {                                                                           \
        .name = #_member,                                                       \
        .type = f_type,                                                         \
        .flags = (_flags),                                                      \
        .itemSize = GET_TYPE_SIZE(f_type),                                      \
        .itemCount = (mode),                                                    \
        .offset = offsetof(_type, _member),                                     \
        .calls = (_calls)}
#endif

#ifndef APP_MIN
#define APP_MIN(x, y) (((x) < (y)) ? (x) : (y))
#endif

#ifndef APP_MAX
#define APP_MAX(x, y) (((x) < (y)) ? (y) : (x))
#endif

    typedef enum
    {
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

    // 字段类型枚举，用于区分不同类型的字段
    typedef enum
    {
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

    // 字段标志位（可选，用于扩展功能）
    typedef enum
    {
        FIELD_FLAG_NONE = 0x00,
        FIELD_FLAG_BIG_ENDIAN = 0x01,       // 强制大端序
        FIELD_FLAG_LITTLE_ENDIAN = 0x02,    // 强制小端序
        FIELD_FLAG_ZERO_COPY = 0x04,        // 零拷贝模式：直接传递源缓冲区指针，不进行 memcpy
        // ... 更多标志
    } field_flag_t;

    typedef enum
    {
        FIELD_ALL_REMAINING = -3, // 剩余所有
        FIELD_LEN_SYMBOL = -2,    // 长度符号（关联前一个字段）
        FIELD_END_SYMBOL = -1,    // 结束符号（如 \0）
        FIELD_FIXED_LEN = 1       // 定长（实际值由用户传入的数字决定，此枚举仅作为语义参考）
    } itemCountMode_t;

    // 外部用户数据传递
    typedef struct
    {
        void *uData;
        size_t uDataSize;
    } parsing_user_data_t;

    // 报文传递(如有需要递归解析)
    typedef struct
    {
        void *rawStream;
        size_t streamSize;
    } parsing_raw_data_t;

    /*
        任意协议解析需用到堆空间申请
        鉴于需在不同的机器上运行, 故堆申请的实现需使用者自我实现, 以便精确控制内存消耗
    */
    typedef struct
    {
        void *(*malloc)(size_t bytes);
        void *(*calloc)(size_t itemCount, size_t itemSize);
        void *(*realloc)(void *pBlock, size_t bytes);
        void (*free)(void *pBlock);
    } parsing_memCall_t;

    // 字段回调函数指针类型定义
    typedef protocol_err_t (*field_callback_t)(parsing_user_data_t *_user, const parsing_raw_data_t *_raw);

    // CRC 校验函数指针类型
    typedef uint32_t (*crc_calc_callback_t)(const uint8_t *data, size_t len);
    typedef bool (*crc_verify_callback_t)(const uint8_t *data, size_t len, uint32_t expected_crc);

    // CRC 校验配置
    typedef struct
    {
        crc_calc_callback_t calc_crc;         // CRC 计算函数
        crc_verify_callback_t verify_crc;     // CRC 验证函数
        uint32_t expected_crc;                // 期望的 CRC 值（用于验证）
        size_t crc_offset;                    // CRC 在消息中的偏移位置
        size_t crc_size;                      // CRC 字段大小（1/2/4 字节）
    } app_crc_config_t;

    // --- 核心数据结构 ---

    // 前置声明
    typedef struct protocol_field_descriptor_s protocol_field_descriptor_t;
    typedef struct protocol_message_descriptor_s protocol_message_descriptor_t;

    typedef struct
    {
        field_callback_t on_parse_callback;
        field_callback_t on_serialize_callback;
    } protocol_field_calls_t;

    // 解析器实例结构体（支持重入和线程安全）
    typedef struct app_parser_instance_s
    {
        const parsing_memCall_t *memCalls;           // 内存管理回调
        void *internal_data;                         // 内部数据（链表头等）
        app_crc_config_t crc_config;                 // CRC 校验配置
        bool crc_enabled;                            // 是否启用 CRC 校验
        uint32_t last_error_code;                    // 最后一次错误码
        void *user_context;                          // 用户自定义上下文（可用于 OS 互斥锁等）
    } app_parser_instance_t;

    // 单个字段的描述符
    // 优化说明：
    // 1. 移除位域（bitfield），改用完整 uint8_t，避免嵌入式编译器兼容性问题
    // 2. 调整字段顺序：指针 -> 整数 -> 小类型，减少 padding
    // 3. RISC-V32/ARM32 内存布局（4字节对齐）：
    //    [0-3]   name (4 bytes pointer)
    //    [4-7]   calls (4 bytes pointer)
    //    [8-9]   offset (2 bytes)
    //    [10-11] itemSize (2 bytes)
    //    [12-13] itemCount (2 bytes)
    //    [14]    type (1 byte)
    //    [15]    flags (1 byte)
    //    总计：16 字节，无填充
    APP_PACKED_STRUCT(protocol_field_descriptor_s)
    {
        const char *name;                          // 字段名称（用于调试、回调标识）- 4字节指针
        const protocol_field_calls_t *calls;       // 回调函数配置指针 - 4字节指针
        uint16_t offset;                           // 在目标结构体中的偏移量 - 2字节
        int16_t itemSize;                          // 字段元素大小 - 2字节
        int16_t itemCount;                         // 字段大小: >0 为定长; -1 遇结束符; -2 关联前一字段长度; -3 剩余全部 - 2字节
        uint8_t type;                              // 字段类型枚举值 - 1字节（替代位域）
        uint8_t flags;                             // 字段标志 - 1字节（替代位域）
    }
    APP_PACKED_END(protocol_field_descriptor_s);

    // 整个消息的描述符
    // 优化说明：
    // 1. 调整字段顺序：指针 -> 整数，减少 padding
    // 2. RISC-V32/ARM32 内存布局（4字节对齐）：
    //    [0-3]   name (4 bytes pointer)
    //    [4-7]   fields (4 bytes pointer)
    //    [8-11]  on_message_start_callback (4 bytes function pointer)
    //    [12-15] on_message_end_callback (4 bytes function pointer)
    //    [16-17] num_fields (2 bytes)
    //    [18-21] total_size (4 bytes)
    //    总计：22 字节（packed 无填充）
    APP_PACKED_STRUCT(protocol_message_descriptor_s)
    {
        const char *name;                          // 消息名称 - 4字节指针
        const protocol_field_descriptor_t *fields; // 指向字段描述符数组的指针 - 4字节
        field_callback_t on_message_start_callback; // 消息开始回调 - 4字节函数指针
        field_callback_t on_message_end_callback;   // 消息结束回调 - 4字节函数指针
        uint16_t num_fields;                       // 字段数量 - 2字节
        int32_t total_size;                        // 消息总大小 (如果所有字段大小已知且固定) - 4字节
    }
    APP_PACKED_END(protocol_message_descriptor_s);

    // 编译时断言：验证结构体大小和字段偏移（嵌入式平台关键）
    // protocol_field_descriptor_t: 
    //   - 32-bit: 16 bytes (4+4+2+2+2+1+1)
    //   - 64-bit: 24 bytes (8+8+2+2+2+1+1)
    // protocol_message_descriptor_t:
    //   - 32-bit: 22 bytes (4+4+4+4+2+4)
    //   - 64-bit: 38 bytes (8+8+8+8+2+4)
    #if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
        #if defined(_WIN64) || defined(__x86_64__) || defined(__aarch64__)
            // 64-bit 平台
            _Static_assert(sizeof(protocol_field_descriptor_t) == 24, 
                           "protocol_field_descriptor_t size must be 24 bytes on 64-bit platforms");
            _Static_assert(sizeof(protocol_message_descriptor_t) == 38, 
                           "protocol_message_descriptor_t size must be 38 bytes on 64-bit platforms");
        #else
            // 32-bit 平台 (RISC-V32, ARM32, x86)
            _Static_assert(sizeof(protocol_field_descriptor_t) == 16, 
                           "protocol_field_descriptor_t size must be 16 bytes on 32-bit platforms");
            _Static_assert(sizeof(protocol_message_descriptor_t) == 22, 
                           "protocol_message_descriptor_t size must be 22 bytes on 32-bit platforms");
        #endif
    #endif

    static const size_t appField_tSize = sizeof(protocol_field_descriptor_t);
    static const size_t appMsg_tSize = sizeof(protocol_message_descriptor_t);

    // 解析器实例管理 API
    protocol_err_t app_parser_init(app_parser_instance_t *parser, 
                                    const parsing_memCall_t *memCalls);
    
    protocol_err_t app_parser_deinit(app_parser_instance_t *parser);
    
    // 配置 CRC 校验
    void app_parser_set_crc_config(app_parser_instance_t *parser, 
                                    const app_crc_config_t *crc_config);
    
    void app_parser_enable_crc(app_parser_instance_t *parser, bool enable);

    // 旧版 API（保持向后兼容，内部使用默认实例）
    protocol_err_t app_memCall_init(const parsing_memCall_t *memCalls);

    protocol_err_t app_parse_message(parsing_user_data_t *user,
                                     const protocol_message_descriptor_t *msg_desc,
                                     const uint8_t *raw_data);

    // 新版重入安全 API
    protocol_err_t app_parse_message_ex(app_parser_instance_t *parser,
                                        parsing_user_data_t *user,
                                        const protocol_message_descriptor_t *msg_desc,
                                        const uint8_t *raw_data);

#ifdef __cplusplus
}
#endif

#endif /* ANY_PROTOCOL_PARSER_H */
