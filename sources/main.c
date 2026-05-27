#include "DBG_macro.h"
#include "AnyProtocolParser.h"
#include <stdlib.h>

char __DBG_string[DBG_DEFAULT_BUFFER_LEN] = {0};

static uint8_t tlv_data[] = {0x0a, 0x08, 0x02, 0x04, 0x08, 0x01, 0x01, 0x02, 0x04, 0x08};
static uint8_t ltv_data[] = {0x0a, 0x01, 0x08, 0x02, 0x04, 0x08, 0x01, 0x01, 0x02, 0x04, 0x08};

typedef struct
{
    unsigned short uuu16;
    unsigned int uuu32;
    unsigned char uuu8;
    void *uuuptr;
    unsigned char uuu8_arr[16];
} test_t;

// TLV 结构（用于解析，不代表实际内存布局）
typedef struct
{
    unsigned char typeNum;   // 偏移 0
    unsigned char len;       // 偏移 1
    unsigned char data[];    // 偏移 2，柔性数组（不定长数据）
} test_tlv_t;

// LTV 结构（用于解析，不代表实际内存布局）
typedef struct
{
    unsigned char len;       // 偏移 0
    unsigned char typeNum;   // 偏移 1
    unsigned char data[];    // 偏移 2，柔性数组（不定长数据）
} test_ltv_t;

static void *parser_malloc(size_t bytes);
static void *parser_calloc(size_t itemCnt, size_t itemSize);
static void *parser_realloc(void *pBlock, size_t bytes);
static void parser_free(void *pBlock);
static protocol_err_t field_call(parsing_user_data_t *_user, const parsing_raw_data_t *_raw);
static protocol_err_t zero_copy_field_call(parsing_user_data_t *_user, const parsing_raw_data_t *_raw);
static protocol_err_t tlv_len_callback(parsing_user_data_t *_user, const parsing_raw_data_t *_raw);
static protocol_err_t ltv_len_callback(parsing_user_data_t *_user, const parsing_raw_data_t *_raw);

static const parsing_memCall_t memCalls = {
    .malloc = parser_malloc,
    .calloc = parser_calloc,
    .realloc = parser_realloc,
    .free = parser_free,
};

static const protocol_field_calls_t field_calls = {
    .on_parse_callback = field_call,
    .on_serialize_callback = NULL,
};

// 零拷贝回调配置
static const protocol_field_calls_t zero_copy_field_calls = {
    .on_parse_callback = zero_copy_field_call,
    .on_serialize_callback = NULL,
};

// TLV 长度字段的特殊回调：将长度值存入 user->uDataSize
static const protocol_field_calls_t tlv_len_calls = {
    .on_parse_callback = tlv_len_callback,
    .on_serialize_callback = NULL,
};

const protocol_field_descriptor_t feilds_fixed[] = {
    FIELD_DESC_FIXED(test_t, uuu16, FIELD_TYPE_UINT16, &field_calls),
    FIELD_DESC_FIXED(test_t, uuu32, FIELD_TYPE_UINT32, &field_calls),
    FIELD_DESC_FIXED(test_t, uuu8, FIELD_TYPE_UINT8, &field_calls),
    FIELD_DESC_FIXED(test_t, uuuptr, FIELD_TYPE_POINT, &field_calls),
    FIELD_DESC_FIXED(test_t, uuu8_arr, FIELD_TYPE_ARRARY, &field_calls),
};

// 使用零拷贝标志的字段描述符示例
const protocol_field_descriptor_t feilds_zero_copy[] = {
    FIELD_DESC_FIXED_WITH_FLAGS(test_t, uuu16, FIELD_TYPE_UINT16, FIELD_FLAG_ZERO_COPY, &zero_copy_field_calls),
    FIELD_DESC_FIXED_WITH_FLAGS(test_t, uuu32, FIELD_TYPE_UINT32, FIELD_FLAG_ZERO_COPY, &zero_copy_field_calls),
    FIELD_DESC_FIXED_WITH_FLAGS(test_t, uuu8, FIELD_TYPE_UINT8, FIELD_FLAG_NONE, &field_calls),  // 混合使用：这个字段仍然拷贝
    FIELD_DESC_FIXED_WITH_FLAGS(test_t, uuuptr, FIELD_TYPE_POINT, FIELD_FLAG_ZERO_COPY, &zero_copy_field_calls),
    FIELD_DESC_FIXED_WITH_FLAGS(test_t, uuu8_arr, FIELD_TYPE_ARRARY, FIELD_FLAG_ZERO_COPY, &zero_copy_field_calls),
};

const protocol_field_descriptor_t feilds_var[] = {
    FIELD_DESC_FIXED(test_tlv_t, typeNum, FIELD_TYPE_UINT8, &field_calls),
    FIELD_DESC_FIXED(test_tlv_t, len, FIELD_TYPE_UINT8, &tlv_len_calls),  // 使用特殊回调保存长度
    FIELD_DESC_VAR(test_tlv_t, data, FIELD_TYPE_UINT8, FIELD_LEN_SYMBOL, &field_calls),  // 改为 data 和 UINT8
};

// LTV 字段描述符（长度在前，类型在后）
static protocol_err_t ltv_len_callback(parsing_user_data_t *_user, const parsing_raw_data_t *_raw);

static const protocol_field_calls_t ltv_len_calls = {
    .on_parse_callback = ltv_len_callback,
    .on_serialize_callback = NULL,
};

const protocol_field_descriptor_t feilds_ltv[] = {
    FIELD_DESC_FIXED(test_ltv_t, len, FIELD_TYPE_UINT8, &ltv_len_calls),   // 长度字段在前
    FIELD_DESC_FIXED(test_ltv_t, typeNum, FIELD_TYPE_UINT8, &field_calls),
    FIELD_DESC_VAR(test_ltv_t, data, FIELD_TYPE_UINT8, FIELD_LEN_SYMBOL, &field_calls),
};

// 创建消息描述符
const protocol_message_descriptor_t msg_desc_fixed = {
    .name = "test_fixed_message",
    .fields = feilds_fixed,
    .num_fields = FIELD_ARR_SIZE(feilds_fixed),
    .total_size = sizeof(test_t),
    .on_message_start_callback = NULL,
    .on_message_end_callback = NULL,
};

const protocol_message_descriptor_t msg_desc_var = {
    .name = "test_tlv_message",
    .fields = feilds_var,
    .num_fields = FIELD_ARR_SIZE(feilds_var),
    .total_size = -1,  // 变长消息
    .on_message_start_callback = NULL,
    .on_message_end_callback = NULL,
};

const protocol_message_descriptor_t msg_desc_ltv = {
    .name = "test_ltv_message",
    .fields = feilds_ltv,
    .num_fields = FIELD_ARR_SIZE(feilds_ltv),
    .total_size = -1,  // 变长消息
    .on_message_start_callback = NULL,
    .on_message_end_callback = NULL,
};

// 零拷贝消息描述符
const protocol_message_descriptor_t msg_desc_zero_copy = {
    .name = "test_zero_copy_message",
    .fields = feilds_zero_copy,
    .num_fields = FIELD_ARR_SIZE(feilds_zero_copy),
    .total_size = sizeof(test_t),
    .on_message_start_callback = NULL,
    .on_message_end_callback = NULL,
};

int main(void)
{
    DEBUG_PRINT("=== Starting protocol parser test ===");
    
    // 初始化内存管理回调
    app_memCall_init(&memCalls);
    
    // ========== 测试 1: 定长消息（旧版 API）==========
    DEBUG_PRINT("\n--- Test 1: Fixed-length message (legacy API) ---");
    test_t RAW_DATA = {
        .uuu16 = 0x4321,
        .uuu32 = 0x12348765,
        .uuu8 = 0x12,
        .uuuptr = NULL,
        .uuu8_arr = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15},
    };
    const uint8_t *pRawFixed = (const uint8_t *)&RAW_DATA;
    
    parsing_user_data_t user_data = {NULL, 0};
    protocol_err_t ret = app_parse_message(&user_data, &msg_desc_fixed, pRawFixed);
    DEBUG_PRINT("Fixed parse result: %d", ret);
    
    // ========== 测试 2: TLV 不定长消息（旧版 API）==========
    DEBUG_PRINT("\n--- Test 2: TLV variable-length message (legacy API) ---");
    DEBUG_PRINT("TLV raw data (len=%d):", (int)sizeof(tlv_data));
    VAR_PRINT_ARR_HEX(tlv_data, sizeof(tlv_data));
    
    parsing_user_data_t user_data_tlv = {NULL, 0};
    ret = app_parse_message(&user_data_tlv, &msg_desc_var, tlv_data);
    DEBUG_PRINT("TLV parse result: %d", ret);
    
    // ========== 测试 3: LTV 不定长消息（旧版 API）==========
    DEBUG_PRINT("\n--- Test 3: LTV variable-length message (legacy API) ---");
    DEBUG_PRINT("LTV raw data (len=%d):", (int)sizeof(ltv_data));
    VAR_PRINT_ARR_HEX(ltv_data, sizeof(ltv_data));
    
    parsing_user_data_t user_data_ltv = {NULL, 0};
    ret = app_parse_message(&user_data_ltv, &msg_desc_ltv, ltv_data);
    DEBUG_PRINT("LTV parse result: %d", ret);
    
    // ========== 测试 4: 零拷贝模式（旧版 API）==========
    DEBUG_PRINT("\n--- Test 4: Zero-copy mode (legacy API) ---");
    test_t RAW_DATA_ZC = {
        .uuu16 = 0xABCD,
        .uuu32 = 0xDEADBEEF,
        .uuu8 = 0xFF,
        .uuuptr = NULL,
        .uuu8_arr = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0x00},
    };
    const uint8_t *pRawZeroCopy = (const uint8_t *)&RAW_DATA_ZC;
    
    parsing_user_data_t user_data_zc = {NULL, 0};
    ret = app_parse_message(&user_data_zc, &msg_desc_zero_copy, pRawZeroCopy);
    DEBUG_PRINT("Zero-copy parse result: %d", ret);
    
    // ========== 测试 5: 重入安全 API（无 CRC）==========
    DEBUG_PRINT("\n--- Test 5: Reentrant API (without CRC) ---");
    app_parser_instance_t parser1;
    ret = app_parser_init(&parser1, &memCalls);
    if (ret != PROTOCOL_OK)
    {
        DEBUG_PRINT("Parser init failed: %d", ret);
        return -1;
    }
    
    parsing_user_data_t user_data_re1 = {NULL, 0};
    ret = app_parse_message_ex(&parser1, &user_data_re1, &msg_desc_fixed, pRawFixed);
    DEBUG_PRINT("Reentrant parse result: %d", ret);
    
    app_parser_deinit(&parser1);
    
    // ========== 测试 6: 重入安全 API + CRC 校验（失败场景）==========
    DEBUG_PRINT("\n--- Test 6: Reentrant API with CRC verification (failure case) ---");
    app_parser_instance_t parser2;
    app_parser_init(&parser2, &memCalls);
    
    // 配置 CRC：假设消息最后 4 字节是 CRC-32
    app_crc_config_t crc_cfg = {
        .calc_crc = NULL,  // 使用默认 CRC-32
        .verify_crc = NULL,
        .expected_crc = 0,
        .crc_offset = 0,   // 从开头计算 CRC
        .crc_size = 4      // CRC 占 4 字节
    };
    app_parser_set_crc_config(&parser2, &crc_cfg);
    app_parser_enable_crc(&parser2, true);
    
    // 构造带 CRC 的测试数据（简单示例：直接在原数据后追加 4 字节伪 CRC）
    uint8_t data_with_crc[64];  // 足够大的缓冲区
    memcpy(data_with_crc, &RAW_DATA, sizeof(test_t));
    // 填充伪 CRC 值（实际应用中应该计算真实 CRC）
    data_with_crc[26] = 0x01;
    data_with_crc[27] = 0x02;
    data_with_crc[28] = 0x03;
    data_with_crc[29] = 0x04;
    
    parsing_user_data_t user_data_crc = {NULL, 0};
    ret = app_parse_message_ex(&parser2, &user_data_crc, &msg_desc_fixed, data_with_crc);
    if (ret == PROTOCOL_ERR_CRC)
    {
        DEBUG_PRINT("CRC verification failed as expected (error code: %d = PROTOCOL_ERR_CRC)", ret);
        DEBUG_PRINT("User can handle CRC error here (e.g., request retransmission)");
    }
    else
    {
        DEBUG_PRINT("CRC verification result: %d", ret);
    }
    
    app_parser_deinit(&parser2);
    
    // ========== 测试 7: 多实例并发解析（演示重入安全性）==========
    DEBUG_PRINT("\n--- Test 7: Multiple concurrent parser instances ---");
    app_parser_instance_t parser_a, parser_b;
    app_parser_init(&parser_a, &memCalls);
    app_parser_init(&parser_b, &memCalls);
    
    // 两个解析器同时解析不同数据
    parsing_user_data_t user_a = {NULL, 0};
    parsing_user_data_t user_b = {NULL, 0};
    
    ret = app_parse_message_ex(&parser_a, &user_a, &msg_desc_fixed, pRawFixed);
    DEBUG_PRINT("Parser A result: %d", ret);
    
    ret = app_parse_message_ex(&parser_b, &user_b, &msg_desc_fixed, pRawZeroCopy);
    DEBUG_PRINT("Parser B result: %d", ret);
    
    app_parser_deinit(&parser_a);
    app_parser_deinit(&parser_b);
    
    DEBUG_PRINT("\n=== All tests completed ===");
    return 0;
}

static void *parser_malloc(size_t bytes)
{
    return malloc(bytes);
}

static void *parser_calloc(size_t itemCnt, size_t itemSize)
{
    return calloc(itemCnt, itemSize);
}

static void *parser_realloc(void *pBlock, size_t bytes)
{
    return realloc(pBlock, bytes);
}

static void parser_free(void *pBlock)
{
    return free(pBlock);
}

static protocol_err_t field_call(parsing_user_data_t *_user, const parsing_raw_data_t *_raw){
    DEBUG_PRINT("Field callback: size=%zu", _raw->streamSize);
    VAR_PRINT_ARR_HEX((uint8_t*)_raw->rawStream, _raw->streamSize);
    return PROTOCOL_OK;
}

/**
 * @brief 零拷贝字段的回调示例
 * @note 在零拷贝模式下，_raw->rawStream 直接指向原始缓冲区，用户不应修改数据
 */
static protocol_err_t zero_copy_field_call(parsing_user_data_t *_user, const parsing_raw_data_t *_raw){
    DEBUG_PRINT("[Zero-Copy] Field callback: size=%zu (direct pointer to source buffer)", _raw->streamSize);
    VAR_PRINT_ARR_HEX((uint8_t*)_raw->rawStream, _raw->streamSize);
    
    // 注意：在零拷贝模式下，不应该修改 _raw->rawStream 指向的数据
    // 如果需要修改，应该先拷贝到自己的缓冲区
    return PROTOCOL_OK;
}

/**
 * @brief TLV 长度字段回调：将长度值保存到 user->uDataSize
 * @note 这样后续的 FIELD_LEN_SYMBOL 字段才能获取到正确的长度
 */
static protocol_err_t tlv_len_callback(parsing_user_data_t *_user, const parsing_raw_data_t *_raw){
    if (!_user || !_raw || _raw->streamSize == 0)
        return PROTOCOL_ERR_ARG;
    
    // 读取长度值（假设是 uint8_t）
    uint8_t len = *(uint8_t*)_raw->rawStream;
    
    // 将长度值存入 user->uDataSize，供后续字段使用
    _user->uDataSize = len;
    
    DEBUG_PRINT("TLV length field: value=%u (saved to user->uDataSize)", len);
    
    return PROTOCOL_OK;
}

/**
 * @brief LTV 长度字段回调：将长度值保存到 user->uDataSize
 * @note LTV 结构中长度字段在第一个位置
 * @note 假设 len 表示 type + payload 的总长度，需要减去 type 字段(1字节)
 */
static protocol_err_t ltv_len_callback(parsing_user_data_t *_user, const parsing_raw_data_t *_raw){
    if (!_user || !_raw || _raw->streamSize == 0)
        return PROTOCOL_ERR_ARG;
    
    // 读取长度值（假设是 uint8_t）
    uint8_t total_len = *(uint8_t*)_raw->rawStream;
    
    // 计算 payload 长度 = 总长度 - type字段大小(1字节)
    if (total_len > 1) {
        _user->uDataSize = total_len - 1;  // 减去 typeNum 的 1 字节
    } else {
        _user->uDataSize = 0;  // 长度为 0 或 1 时，没有 payload
    }
    
    DEBUG_PRINT("LTV length field: total_len=%u, payload_len=%zu (saved to user->uDataSize)", 
                total_len, _user->uDataSize);
    
    return PROTOCOL_OK;
}