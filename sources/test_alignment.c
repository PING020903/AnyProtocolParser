#include "DBG_macro.h"
#include "AnyProtocolParser.h"
#include <string.h>

/**
 * @brief 嵌入式平台内存对齐测试
 * 
 * 验证目标：
 * 1. RISC-V32/ARM32 (32位) 平台结构体大小正确
 * 2. 无编译器自动填充的 padding
 * 3. packed 属性生效
 * 4. 字段偏移量符合预期
 */

// 测试数据结构体（模拟实际协议）
typedef struct {
    uint8_t type;
    uint8_t length;
    uint16_t value;
} test_proto_t;

// 打印结构体布局信息
static void print_struct_layout(void) {
    DEBUG_PRINT("=== Structure Layout Information ===");
    DEBUG_PRINT("Platform: %s (pointer size = %u bytes)",
                sizeof(void*) == 4 ? "32-bit" : "64-bit",
                (unsigned int)sizeof(void*));
    
    // protocol_field_descriptor_t
    unsigned int ptr_size = (unsigned int)sizeof(void*);
    unsigned int expected_field_desc_size = (ptr_size == 4) ? 16 : 24;
    
    DEBUG_PRINT("\n--- protocol_field_descriptor_t ---");
    DEBUG_PRINT("Total size: %u bytes (expected: %u for %s)", 
                (unsigned int)sizeof(protocol_field_descriptor_t), 
                expected_field_desc_size,
                ptr_size == 4 ? "32-bit" : "64-bit");
    DEBUG_PRINT("  name:    offset=%u, size=%u", 
                (unsigned int)offsetof(protocol_field_descriptor_t, name), 
                (unsigned int)sizeof(((protocol_field_descriptor_t*)0)->name));
    DEBUG_PRINT("  calls:   offset=%u, size=%u", 
                (unsigned int)offsetof(protocol_field_descriptor_t, calls), 
                (unsigned int)sizeof(((protocol_field_descriptor_t*)0)->calls));
    DEBUG_PRINT("  offset:  offset=%u, size=%u", 
                (unsigned int)offsetof(protocol_field_descriptor_t, offset), 
                (unsigned int)sizeof(((protocol_field_descriptor_t*)0)->offset));
    DEBUG_PRINT("  itemSize: offset=%u, size=%u", 
                (unsigned int)offsetof(protocol_field_descriptor_t, itemSize), 
                (unsigned int)sizeof(((protocol_field_descriptor_t*)0)->itemSize));
    DEBUG_PRINT("  itemCount: offset=%u, size=%u", 
                (unsigned int)offsetof(protocol_field_descriptor_t, itemCount), 
                (unsigned int)sizeof(((protocol_field_descriptor_t*)0)->itemCount));
    DEBUG_PRINT("  type:    offset=%u, size=%u", 
                (unsigned int)offsetof(protocol_field_descriptor_t, type), 
                (unsigned int)sizeof(((protocol_field_descriptor_t*)0)->type));
    DEBUG_PRINT("  flags:   offset=%u, size=%u", 
                (unsigned int)offsetof(protocol_field_descriptor_t, flags), 
                (unsigned int)sizeof(((protocol_field_descriptor_t*)0)->flags));
    
    // protocol_message_descriptor_t
    unsigned int expected_msg_desc_size = (ptr_size == 4) ? 22 : 38;
    
    DEBUG_PRINT("\n--- protocol_message_descriptor_t ---");
    DEBUG_PRINT("Total size: %u bytes (expected: %u for %s)", 
                (unsigned int)sizeof(protocol_message_descriptor_t), 
                expected_msg_desc_size,
                ptr_size == 4 ? "32-bit" : "64-bit");
    DEBUG_PRINT("  name:                       offset=%u, size=%u", 
                (unsigned int)offsetof(protocol_message_descriptor_t, name), 
                (unsigned int)sizeof(((protocol_message_descriptor_t*)0)->name));
    DEBUG_PRINT("  fields:                     offset=%u, size=%u", 
                (unsigned int)offsetof(protocol_message_descriptor_t, fields), 
                (unsigned int)sizeof(((protocol_message_descriptor_t*)0)->fields));
    DEBUG_PRINT("  on_message_start_callback:  offset=%u, size=%u", 
                (unsigned int)offsetof(protocol_message_descriptor_t, on_message_start_callback), 
                (unsigned int)sizeof(((protocol_message_descriptor_t*)0)->on_message_start_callback));
    DEBUG_PRINT("  on_message_end_callback:    offset=%u, size=%u", 
                (unsigned int)offsetof(protocol_message_descriptor_t, on_message_end_callback), 
                (unsigned int)sizeof(((protocol_message_descriptor_t*)0)->on_message_end_callback));
    DEBUG_PRINT("  num_fields:                 offset=%u, size=%u", 
                (unsigned int)offsetof(protocol_message_descriptor_t, num_fields), 
                (unsigned int)sizeof(((protocol_message_descriptor_t*)0)->num_fields));
    DEBUG_PRINT("  total_size:                 offset=%u, size=%u", 
                (unsigned int)offsetof(protocol_message_descriptor_t, total_size), 
                (unsigned int)sizeof(((protocol_message_descriptor_t*)0)->total_size));
}

// 测试 1: 验证结构体大小
static int test_struct_size(void) {
    DEBUG_PRINT("\n=== Test 1: Structure Size Verification ===");
    
    int passed = 1;
    unsigned int ptr_size = (unsigned int)sizeof(void*);
    
    // 根据平台指针大小计算期望值
    unsigned int expected_field_desc = (ptr_size == 4) ? 16 : 24;  // 32-bit: 16, 64-bit: 24
    unsigned int expected_msg_desc = (ptr_size == 4) ? 22 : 38;    // 32-bit: 22, 64-bit: 38
    
    // 验证 protocol_field_descriptor_t 大小
    if ((unsigned int)sizeof(protocol_field_descriptor_t) != expected_field_desc) {
        DEBUG_PRINT("[FAIL] protocol_field_descriptor_t size is %u (expected %u for %s)", 
                    (unsigned int)sizeof(protocol_field_descriptor_t), expected_field_desc,
                    ptr_size == 4 ? "32-bit" : "64-bit");
        passed = 0;
    } else {
        DEBUG_PRINT("[PASS] protocol_field_descriptor_t size is %u bytes (%s platform)", 
                    (unsigned int)sizeof(protocol_field_descriptor_t),
                    ptr_size == 4 ? "32-bit" : "64-bit");
    }
    
    // 验证 protocol_message_descriptor_t 大小
    if ((unsigned int)sizeof(protocol_message_descriptor_t) != expected_msg_desc) {
        DEBUG_PRINT("[FAIL] protocol_message_descriptor_t size is %u (expected %u for %s)", 
                    (unsigned int)sizeof(protocol_message_descriptor_t), expected_msg_desc,
                    ptr_size == 4 ? "32-bit" : "64-bit");
        passed = 0;
    } else {
        DEBUG_PRINT("[PASS] protocol_message_descriptor_t size is %u bytes (%s platform)", 
                    (unsigned int)sizeof(protocol_message_descriptor_t),
                    ptr_size == 4 ? "32-bit" : "64-bit");
    }
    
    return passed;
}

// 测试 2: 验证字段偏移量（无 padding）
static int test_field_offsets(void) {
    DEBUG_PRINT("\n=== Test 2: Field Offset Verification (No Padding) ===");
    
    int passed = 1;
    unsigned int ptr_size = (unsigned int)sizeof(void*);
    
    // 根据平台指针大小计算期望偏移量
    if (ptr_size == 4) {
        // 32-bit 平台期望值
        // [0-3]   name (4 bytes)
        // [4-7]   calls (4 bytes)
        // [8-9]   offset (2 bytes)
        // [10-11] itemSize (2 bytes)
        // [12-13] itemCount (2 bytes)
        // [14]    type (1 byte)
        // [15]    flags (1 byte)
        
        if ((unsigned int)offsetof(protocol_field_descriptor_t, name) != 0) {
            DEBUG_PRINT("[FAIL] field_desc.name offset is %u (expected 0)", 
                        (unsigned int)offsetof(protocol_field_descriptor_t, name));
            passed = 0;
        }
        
        if ((unsigned int)offsetof(protocol_field_descriptor_t, calls) != 4) {
            DEBUG_PRINT("[FAIL] field_desc.calls offset is %u (expected 4)", 
                        (unsigned int)offsetof(protocol_field_descriptor_t, calls));
            passed = 0;
        }
        
        if ((unsigned int)offsetof(protocol_field_descriptor_t, offset) != 8) {
            DEBUG_PRINT("[FAIL] field_desc.offset offset is %u (expected 8)", 
                        (unsigned int)offsetof(protocol_field_descriptor_t, offset));
            passed = 0;
        }
        
        if ((unsigned int)offsetof(protocol_field_descriptor_t, itemSize) != 10) {
            DEBUG_PRINT("[FAIL] field_desc.itemSize offset is %u (expected 10)", 
                        (unsigned int)offsetof(protocol_field_descriptor_t, itemSize));
            passed = 0;
        }
        
        if ((unsigned int)offsetof(protocol_field_descriptor_t, itemCount) != 12) {
            DEBUG_PRINT("[FAIL] field_desc.itemCount offset is %u (expected 12)", 
                        (unsigned int)offsetof(protocol_field_descriptor_t, itemCount));
            passed = 0;
        }
        
        if ((unsigned int)offsetof(protocol_field_descriptor_t, type) != 14) {
            DEBUG_PRINT("[FAIL] field_desc.type offset is %u (expected 14)", 
                        (unsigned int)offsetof(protocol_field_descriptor_t, type));
            passed = 0;
        }
        
        if ((unsigned int)offsetof(protocol_field_descriptor_t, flags) != 15) {
            DEBUG_PRINT("[FAIL] field_desc.flags offset is %u (expected 15)", 
                        (unsigned int)offsetof(protocol_field_descriptor_t, flags));
            passed = 0;
        }
    } else {
        // 64-bit 平台期望值
        // [0-7]   name (8 bytes)
        // [8-15]  calls (8 bytes)
        // [16-17] offset (2 bytes)
        // [18-19] itemSize (2 bytes)
        // [20-21] itemCount (2 bytes)
        // [22]    type (1 byte)
        // [23]    flags (1 byte)
        
        if ((unsigned int)offsetof(protocol_field_descriptor_t, name) != 0) {
            DEBUG_PRINT("[FAIL] field_desc.name offset is %u (expected 0)", 
                        (unsigned int)offsetof(protocol_field_descriptor_t, name));
            passed = 0;
        }
        
        if ((unsigned int)offsetof(protocol_field_descriptor_t, calls) != 8) {
            DEBUG_PRINT("[FAIL] field_desc.calls offset is %u (expected 8)", 
                        (unsigned int)offsetof(protocol_field_descriptor_t, calls));
            passed = 0;
        }
        
        if ((unsigned int)offsetof(protocol_field_descriptor_t, offset) != 16) {
            DEBUG_PRINT("[FAIL] field_desc.offset offset is %u (expected 16)", 
                        (unsigned int)offsetof(protocol_field_descriptor_t, offset));
            passed = 0;
        }
        
        if ((unsigned int)offsetof(protocol_field_descriptor_t, itemSize) != 18) {
            DEBUG_PRINT("[FAIL] field_desc.itemSize offset is %u (expected 18)", 
                        (unsigned int)offsetof(protocol_field_descriptor_t, itemSize));
            passed = 0;
        }
        
        if ((unsigned int)offsetof(protocol_field_descriptor_t, itemCount) != 20) {
            DEBUG_PRINT("[FAIL] field_desc.itemCount offset is %u (expected 20)", 
                        (unsigned int)offsetof(protocol_field_descriptor_t, itemCount));
            passed = 0;
        }
        
        if ((unsigned int)offsetof(protocol_field_descriptor_t, type) != 22) {
            DEBUG_PRINT("[FAIL] field_desc.type offset is %u (expected 22)", 
                        (unsigned int)offsetof(protocol_field_descriptor_t, type));
            passed = 0;
        }
        
        if ((unsigned int)offsetof(protocol_field_descriptor_t, flags) != 23) {
            DEBUG_PRINT("[FAIL] field_desc.flags offset is %u (expected 23)", 
                        (unsigned int)offsetof(protocol_field_descriptor_t, flags));
            passed = 0;
        }
    }
    
    if (passed) {
        DEBUG_PRINT("[PASS] All field offsets are correct (no padding detected on %s platform)",
                    ptr_size == 4 ? "32-bit" : "64-bit");
    }
    
    return passed;
}

// 测试 3: 验证枚举值存储
static int test_enum_storage(void) {
    DEBUG_PRINT("\n=== Test 3: Enum Value Storage ===");
    
    int passed = 1;
    
    protocol_field_descriptor_t desc;
    memset(&desc, 0, sizeof(desc));
    
    // 测试所有 field_type_t 枚举值
    desc.type = FIELD_TYPE_UINT8;
    if (desc.type != FIELD_TYPE_UINT8) {
        DEBUG_PRINT("[FAIL] FIELD_TYPE_UINT8 storage failed");
        passed = 0;
    }
    
    desc.type = FIELD_TYPE_ARRARY;  // 最大值 (11)
    if (desc.type != FIELD_TYPE_ARRARY) {
        DEBUG_PRINT("[FAIL] FIELD_TYPE_ARRARY storage failed");
        passed = 0;
    }
    
    // 测试 field_flag_t 组合
    desc.flags = FIELD_FLAG_NONE;
    if (desc.flags != FIELD_FLAG_NONE) {
        DEBUG_PRINT("[FAIL] FIELD_FLAG_NONE storage failed");
        passed = 0;
    }
    
    desc.flags = FIELD_FLAG_BIG_ENDIAN | FIELD_FLAG_ZERO_COPY;
    if (desc.flags != (FIELD_FLAG_BIG_ENDIAN | FIELD_FLAG_ZERO_COPY)) {
        DEBUG_PRINT("[FAIL] Flag combination storage failed");
        passed = 0;
    }
    
    if (passed) {
        DEBUG_PRINT("[PASS] Enum values stored correctly in uint8_t fields");
    }
    
    return passed;
}

// 测试 4: 验证指针访问对齐
static int test_pointer_alignment(void) {
    DEBUG_PRINT("\n=== Test 4: Pointer Access Alignment ===");
    
    int passed = 1;
    
    // 分配一个未对齐的缓冲区（模拟嵌入式环境）
    uint8_t buffer[64];
    memset(buffer, 0, sizeof(buffer));
    
    // 故意从奇数地址开始（如果可能）
    protocol_field_descriptor_t *desc = (protocol_field_descriptor_t *)(buffer + 1);
    
    // 初始化字段
    desc->name = "test";
    desc->calls = NULL;
    desc->offset = 0x1234;
    desc->itemSize = 4;
    desc->itemCount = 8;
    desc->type = FIELD_TYPE_UINT32;
    desc->flags = FIELD_FLAG_BIG_ENDIAN;
    
    // 验证读取正确性
    if (desc->offset != 0x1234) {
        DEBUG_PRINT("[FAIL] Unaligned access to 'offset' failed");
        passed = 0;
    }
    
    if (desc->itemSize != 4) {
        DEBUG_PRINT("[FAIL] Unaligned access to 'itemSize' failed");
        passed = 0;
    }
    
    if (desc->type != FIELD_TYPE_UINT32) {
        DEBUG_PRINT("[FAIL] Unaligned access to 'type' failed");
        passed = 0;
    }
    
    if (passed) {
        DEBUG_PRINT("[PASS] Unaligned pointer access works correctly");
    }
    
    return passed;
}

// 测试 5: 验证数组布局连续性
static int test_array_layout(void) {
    DEBUG_PRINT("\n=== Test 5: Array Layout Continuity ===");
    
    int passed = 1;
    
    // 创建描述符数组
    protocol_field_descriptor_t descs[3];
    memset(descs, 0, sizeof(descs));
    
    // 验证数组元素连续存储（无额外填充）
    unsigned int expected_gap = (unsigned int)sizeof(protocol_field_descriptor_t);
    unsigned int actual_gap = (unsigned int)((uint8_t*)&descs[1] - (uint8_t*)&descs[0]);
    
    if (actual_gap != expected_gap) {
        DEBUG_PRINT("[FAIL] Array element gap is %u (expected %u)", 
                    actual_gap, expected_gap);
        passed = 0;
    } else {
        DEBUG_PRINT("[PASS] Array elements are contiguous (gap=%u bytes)", actual_gap);
    }
    
    // 验证总大小
    unsigned int expected_total = (unsigned int)sizeof(protocol_field_descriptor_t) * 3;
    if ((unsigned int)sizeof(descs) != expected_total) {
        DEBUG_PRINT("[FAIL] Array total size is %u (expected %u)", 
                    (unsigned int)sizeof(descs), expected_total);
        passed = 0;
    } else {
        DEBUG_PRINT("[PASS] Array total size is correct (%u bytes)", (unsigned int)sizeof(descs));
    }
    
    return passed;
}

// 测试 6: 跨平台一致性验证
static int test_cross_platform_consistency(void) {
    DEBUG_PRINT("\n=== Test 6: Cross-Platform Consistency ===");
    
    int passed = 1;
    
    // 验证固定宽度类型大小
    if (sizeof(uint8_t) != 1 || sizeof(uint16_t) != 2 || sizeof(uint32_t) != 4) {
        DEBUG_PRINT("[FAIL] Fixed-width types have unexpected sizes");
        passed = 0;
    } else {
        DEBUG_PRINT("[PASS] Fixed-width types are consistent (u8=%u, u16=%u, u32=%u)", 
                    (unsigned int)sizeof(uint8_t), (unsigned int)sizeof(uint16_t), (unsigned int)sizeof(uint32_t));
    }
    
    // 验证指针大小（32位平台应为4字节）
    if (sizeof(void*) != 4 && sizeof(void*) != 8) {
        DEBUG_PRINT("[WARN] Pointer size is %u (unexpected)", (unsigned int)sizeof(void*));
        // 不视为失败，因为可能是 64 位平台
    } else {
        DEBUG_PRINT("[INFO] Pointer size is %u bytes (%s platform)", 
                    (unsigned int)sizeof(void*), 
                    sizeof(void*) == 4 ? "32-bit" : "64-bit");
    }
    
    return passed;
}

// 主测试函数
int test_embedded_alignment(void) {
    DEBUG_PRINT("\n========================================");
    DEBUG_PRINT("  Embedded Platform Alignment Tests");
    DEBUG_PRINT("  Target: RISC-V32 / ARM32");
    DEBUG_PRINT("========================================");
    
    // 打印详细布局信息
    print_struct_layout();
    
    // 执行所有测试
    int results[6];
    int total_tests = 0;
    int passed_tests = 0;
    
    results[0] = test_struct_size();
    results[1] = test_field_offsets();
    results[2] = test_enum_storage();
    results[3] = test_pointer_alignment();
    results[4] = test_array_layout();
    results[5] = test_cross_platform_consistency();
    
    // 统计结果
    for (int i = 0; i < 6; i++) {
        total_tests++;
        if (results[i]) {
            passed_tests++;
        }
    }
    
    // 输出总结
    DEBUG_PRINT("\n========================================");
    DEBUG_PRINT("  Test Summary: %d/%d passed", passed_tests, total_tests);
    DEBUG_PRINT("========================================");
    
    if (passed_tests == total_tests) {
        DEBUG_PRINT("[SUCCESS] All alignment tests passed!");
        DEBUG_PRINT("Structure design is compatible with RISC-V32/ARM32.");
        return 0;
    } else {
        DEBUG_PRINT("[FAILURE] %d test(s) failed!", total_tests - passed_tests);
        DEBUG_PRINT("Please review the structure design for embedded platforms.");
        return 1;
    }
}
