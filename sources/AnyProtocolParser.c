#include "AnyProtocolParser.h"
#include "../thirdparty/c-linked-list-main/src/linkedlist/ll.h"
#include <string.h> // memcpy
#include <stdio.h>  // printf

typedef struct
{
    ll_t node;
    const protocol_field_descriptor_t *field;
    void *pData;
    int16_t itemSize;
    int16_t itemCount;
} parse_tempData_t;

static const parsing_memCall_t *g_memCalls = NULL;
static const protocol_message_descriptor_t *g_msgDesc = NULL;
static parse_tempData_t *g_parseData = NULL;

static protocol_err_t app_managed_list_init(void)
{
    g_parseData = g_memCalls->calloc(1, sizeof(parse_tempData_t));
    if (!g_parseData)
        return PROTOCOL_ERR_MEM;

    g_parseData->node.next = &g_parseData->node;
    g_parseData->node.prev = &g_parseData->node;

    return PROTOCOL_OK;
}

static protocol_err_t app_managed_list_add(void *rawData, size_t itemSize, size_t itemCnt,
                                           const protocol_field_descriptor_t *pCurrentField)
{
    if (!rawData)
        return PROTOCOL_ERR_PASSMSG;

    void *p = g_memCalls->calloc(itemCnt, itemSize);
    if (!p)
        return PROTOCOL_ERR_MEM;

    memcpy(p, rawData, itemCnt * itemSize);

    parse_tempData_t *pFieldDataInfo = g_memCalls->malloc(sizeof(*pFieldDataInfo));
    if (!pFieldDataInfo)
    {
        g_memCalls->free(p);
        return PROTOCOL_ERR_MEM;
    }

    pFieldDataInfo->pData = p;
    pFieldDataInfo->itemCount = itemCnt;
    pFieldDataInfo->itemSize = itemSize;
    pFieldDataInfo->field = pCurrentField;
    list_add_tail(&pFieldDataInfo->node, &g_parseData->node);
    return PROTOCOL_OK;
}

/**
 * @brief 遍历链表并打印所有字段信息（示例：展示如何从节点还原结构体）
 */
static void app_managed_list_dump(void)
{
    if (!g_parseData)
        return;

    ll_t *current;
    ll_t *next;
    int index = 0;

    // 安全遍历链表
    list_for_each_safe(current, next, &g_parseData->node)
    {
        // ✅ 关键：从 ll_t* 节点指针还原到包含它的 parse_tempData_t 结构体
        parse_tempData_t *field_info = list_entry(current, parse_tempData_t, node);

        // 现在可以访问完整结构体的所有成员
        printf("[%d] Field: %s\n", index++, field_info->field->name);
        printf("    ItemSize: %d, Count: %d\n", field_info->itemSize, field_info->itemCount);
        printf("    Data Ptr: %p\n", field_info->pData);
    }
}

/**
 * @brief 释放整个链表及所有节点内存
 */
static void app_managed_list_destroy(void)
{
    if (!g_parseData)
        return;

    ll_t *current;
    ll_t *next;

    // 安全遍历并释放每个节点
    list_for_each_safe(current, next, &g_parseData->node)
    {
        // 从节点还原结构体指针
        parse_tempData_t *field_info = list_entry(current, parse_tempData_t, node);

        // 先释放内部动态分配的 pData
        if (field_info->pData)
        {
            g_memCalls->free(field_info->pData);
            field_info->pData = NULL;
        }

        // 再释放节点本身
        g_memCalls->free(field_info);
    }

    // 最后释放链表头
    g_memCalls->free(g_parseData);
    g_parseData = NULL;
}

/**
 * @brief 计算字段的实际字节大小（处理变长逻辑）
 * @note itemCount >= 0 为定长；< 0 为变长模式
 */
static size_t calculate_field_size(const protocol_field_descriptor_t *field,
                                   const parsing_user_data_t *user)
{
    if (field->itemCount >= 0)
    {
        // 定长：元素大小 * 个数
        return (size_t)field->itemSize * (size_t)field->itemCount;
    }
    else if (field->itemCount == FIELD_LEN_SYMBOL) // -2
    {
        // 关联前一个字段：从用户上下文中获取前一个字段的值作为个数
        return (size_t)field->itemSize * (size_t)user->uDataSize;
    }
    else if (field->itemCount == FIELD_END_SYMBOL) // -1
    {
        // 结束符号（如 \0）：需要用户在回调中动态检测
        // 这里返回 0，由回调函数自行处理
        return 0;
    }
    else if (field->itemCount == FIELD_ALL_REMAINING) // -3
    {
        // 剩余全部：由用户在回调中根据 total_size 自行处理
        return 0;
    }

    // 其他负值情况，默认返回 0
    return 0;
}

/**
 * @brief 解析单个字段
 */
static protocol_err_t parse_single_field(parsing_user_data_t *user,
                                         const protocol_field_descriptor_t *field,
                                         const uint8_t *base_ptr)
{
    if (!field || !base_ptr)
    {
        return PROTOCOL_ERR_ARG;
    }

    const uint8_t *field_ptr = base_ptr + field->offset;
    parsing_raw_data_t raw_view;
    raw_view.rawStream = (void *)field_ptr;

    // 计算字段大小
    size_t field_size = calculate_field_size(field, user);

    // 对于变长字段，如果 calculate_field_size 返回 0，需要特殊处理
    if (field_size == 0 && field->itemCount < 0)
    {
        // 变长字段：由回调函数自行决定如何处理
        // raw_view.streamSize = 0，回调函数需要根据实际情况处理
        raw_view.streamSize = 0;
    }
    else
    {
        raw_view.streamSize = field_size;
    }

    // 该字段没有回调配置
    if (!field->calls)
        return PROTOCOL_OK;

    // 计算实际的元素个数（处理变长字段）
    size_t actual_item_count = 0;
    size_t field_byte_size = calculate_field_size(field, user);

    if (field->itemCount >= 0)
    {
        // 定长字段：直接使用 itemCount
        actual_item_count = (size_t)field->itemCount;
    }
    else if (field_byte_size > 0 && field->itemSize > 0)
    {
        // 变长字段：根据字节大小计算元素个数
        actual_item_count = field_byte_size / (size_t)field->itemSize;
    }
    else
    {
        // 无法确定大小的变长字段（如 FIELD_END_SYMBOL），设为 1 让回调自行处理
        actual_item_count = 1;
        field_byte_size = (size_t)field->itemSize; // 至少拷贝一个元素
    }

    // 执行用户回调前，先将数据拷贝到安全的堆内存
    protocol_err_t ret = app_managed_list_add(raw_view.rawStream, (size_t)field->itemSize,
                                              actual_item_count, field);
    if (ret != PROTOCOL_OK)
    {
        printf("[%s]: list add error [%d]\n", __func__, ret);
        return ret;
    }

    // 从链表中获取最新添加的节点（即刚拷贝的数据）
    ll_t *last_node = g_parseData->node.prev; // 链表尾部的节点
    parse_tempData_t *safe_data = list_entry(last_node, parse_tempData_t, node);

    // 构造指向安全拷贝数据的 raw_view
    parsing_raw_data_t safe_raw_view;
    safe_raw_view.rawStream = safe_data->pData;
    safe_raw_view.streamSize = safe_data->itemCount * safe_data->itemSize;

    // 执行用户回调，传递的是安全拷贝后的数据
    if (field->calls->on_parse_callback)
    {
        ret = field->calls->on_parse_callback(user, &safe_raw_view);
        if (ret != PROTOCOL_OK)
        {
            return ret; // 支持用户返回 PROTOCOL_ERR_PASSMSG 提前终止
        }
    }

    return PROTOCOL_OK;
}

protocol_err_t app_memCall_init(const parsing_memCall_t *memCalls)
{
    const size_t testMem0_memSize = 64;
    const size_t testMem1_memSize = 256;
    uint8_t *testp[2] = {NULL};
    if (!memCalls)
        return PROTOCOL_ERR_ARG;

    if (!memCalls->malloc || !memCalls->calloc ||
        !memCalls->realloc || !memCalls->free)
        return PROTOCOL_ERR_ARG;

    g_memCalls = memCalls;

    testp[0] = g_memCalls->malloc(testMem0_memSize);
    if (!testp[0])
        goto _err;

    testp[1] = g_memCalls->calloc(sizeof(testMem1_memSize), testMem1_memSize);
    if (!testp[1])
        goto _err;

    testp[0] = g_memCalls->realloc(testp[0], testMem1_memSize);
    if (!testp[0])
        goto _err;

    for (int idx = 0; idx < FIELD_ARR_SIZE(testp); idx++)
    {
        if (testp[idx])
        {
            g_memCalls->free(testp[idx]);
        }
    }

    return PROTOCOL_OK;

_err:
    for (int idx = 0; idx < FIELD_ARR_SIZE(testp); idx++)
    {
        if (testp[idx])
        {
            g_memCalls->free(testp[idx]);
        }
    }

    return PROTOCOL_ERR_FUNCS;
}

/**
 * @brief 根据消息模板解析整条报文
 * @param user 用户自定义数据上下文
 * @param msg_desc 消息描述符（模板）
 * @param raw_data 原始报文字节流
 * @return PROTOCOL_OK 或错误码
 */
protocol_err_t app_parse_message(parsing_user_data_t *user,
                                 const protocol_message_descriptor_t *msg_desc,
                                 const uint8_t *raw_data)
{
    if (!user || !msg_desc || !raw_data)
    {
        return PROTOCOL_ERR_ARG;
    }

    if (!g_memCalls || !g_memCalls->malloc || !g_memCalls->calloc ||
        !g_memCalls->realloc || !g_memCalls->free)
    {
        return PROTOCOL_ERR_CALLS_INIT;
    }

    // 1. 消息开始回调
    if (msg_desc->on_message_start_callback)
    {
        parsing_raw_data_t full_msg = {(void *)raw_data, msg_desc->total_size};
        msg_desc->on_message_start_callback(user, &full_msg);
    }

    app_managed_list_init();
    // 2. 遍历所有字段进行提取
    for (uint16_t i = 0; i < msg_desc->num_fields; i++)
    {
        protocol_err_t err = parse_single_field(user, &msg_desc->fields[i], raw_data);
        if (err != PROTOCOL_OK)
        {
            // 遇到错误或用户要求跳过(PROTOCOL_ERR_PASSMSG)则立即跳出
            // 务必记得释放堆空间, 避免内存泄漏
            break;
        }
    }
    app_managed_list_dump();
    app_managed_list_destroy();

    // 3. 消息结束回调
    if (msg_desc->on_message_end_callback)
    {
        parsing_raw_data_t full_msg = {(void *)raw_data, msg_desc->total_size};
        msg_desc->on_message_end_callback(user, &full_msg);
    }

    return PROTOCOL_OK;
}