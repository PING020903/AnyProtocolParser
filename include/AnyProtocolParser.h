#pragma once
#ifndef _ANYPROTOCOLPARSER_H_
#define _ANYPROTOCOLPARSER_H_

#include <stdint.h>
#include <stddef.h>

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

    // --- 核心数据结构 ---

    // 前置声明
    typedef struct protocol_field_descriptor_s protocol_field_descriptor_t;
    typedef struct protocol_message_descriptor_s protocol_message_descriptor_t;

    typedef struct
    {
        field_callback_t on_parse_callback;
        field_callback_t on_serialize_callback;
    } protocol_field_calls_t;

    // 单个字段的描述符
    APP_PACKED_STRUCT(protocol_field_descriptor_s)
    {
        const char *name;                  // 字段名称（用于调试、回调标识）
        field_type_t type : APP_MAX_BITS;  // 字段类型
        field_flag_t flags : APP_MAX_BITS; // 字段标志
        uint16_t offset;                   // 在目标结构体中的偏移量 (使用固定宽度确保跨平台一致)
        int16_t itemSize;                  // 字段元素大小
        int16_t itemCount;                 // 字段大小: >0 为定长; -1 遇结束符; -2 关联前一字段长度; -3 剩余全部

        // 可选：回调函数配置指针
        const protocol_field_calls_t *calls;
    }
    APP_PACKED_END(protocol_field_descriptor_s);

    // 整个消息的描述符
    APP_PACKED_STRUCT(protocol_message_descriptor_s)
    {
        const char *name;                          // 消息名称
        const protocol_field_descriptor_t *fields; // 指向字段描述符数组的指针
        uint16_t num_fields;                       // 字段数量
        int32_t total_size;                        // 消息总大小 (如果所有字段大小已知且固定)

        // 可选：消息级别的回调
        // 例如，在整个消息解析开始前、结束后调用
        field_callback_t on_message_start_callback;
        field_callback_t on_message_end_callback;
    }
    APP_PACKED_END(protocol_message_descriptor_s);

    static const size_t appField_tSize = sizeof(protocol_field_descriptor_t);
    static const size_t appMsg_tSize = sizeof(protocol_message_descriptor_t);

    protocol_err_t app_memCall_init(const parsing_memCall_t *memCalls);

    protocol_err_t app_parse_message(parsing_user_data_t *user,
                                     const protocol_message_descriptor_t *msg_desc,
                                     const uint8_t *raw_data);

#ifdef __cplusplus
}
#endif

#endif /* ANY_PROTOCOL_PARSER_H */
