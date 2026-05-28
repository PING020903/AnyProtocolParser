#include "AnyProtocolParser.h"
#include "../thirdparty/c-linked-list-main/src/linkedlist/ll.h"
#include <stdbool.h>
#include <string.h> // memcpy

// 调试输出控制宏（可选）
#ifdef APP_PARSER_ENABLE_DEBUG_LOG
#include <stdio.h>
#define APP_LOG(fmt, ...) printf("[APP_PARSER] " fmt "\n", ##__VA_ARGS__)
#else
#define APP_LOG(fmt, ...) ((void)0)
#endif

#define APP_TEMPDATA_DEBUG 1

typedef struct {
  ll_t node;
  const protocol_field_descriptor_t *field;
  void *pData;
  int16_t itemSize;
  int16_t itemCount;
} parse_tempData_t;

// 保留全局变量用于向后兼容（旧版 API）
static const parsing_memCall_t *g_memCalls = NULL;
static const protocol_message_descriptor_t *g_msgDesc = NULL;
static parse_tempData_t *g_parseData = NULL;

static protocol_err_t app_managed_list_init(void) {
  g_parseData = g_memCalls->calloc(1, sizeof(parse_tempData_t));
  if (!g_parseData)
    return PROTOCOL_ERR_MEM;

  g_parseData->node.next = &g_parseData->node;
  g_parseData->node.prev = &g_parseData->node;

  return PROTOCOL_OK;
}

// 基于实例的链表初始化（支持重入）
static protocol_err_t app_managed_list_init_ex(app_parser_instance_t *parser) {
  if (!parser || !parser->memCalls)
    return PROTOCOL_ERR_ARG;

  parse_tempData_t *list_head =
      parser->memCalls->calloc(1, sizeof(parse_tempData_t));
  if (!list_head)
    return PROTOCOL_ERR_MEM;

  list_head->node.next = &list_head->node;
  list_head->node.prev = &list_head->node;

  parser->internal_data = list_head;
  return PROTOCOL_OK;
}

static protocol_err_t
app_managed_list_add(void *rawData, size_t itemSize, size_t itemCnt,
                     const protocol_field_descriptor_t *pCurrentField) {
  if (!rawData)
    return PROTOCOL_ERR_PASSMSG;

  void *p = g_memCalls->calloc(itemCnt, itemSize);
  if (!p)
    return PROTOCOL_ERR_MEM;

  memcpy(p, rawData, itemCnt * itemSize);

  parse_tempData_t *pFieldDataInfo =
      g_memCalls->malloc(sizeof(*pFieldDataInfo));
  if (!pFieldDataInfo) {
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

// 基于实例的链表添加（支持重入）
static protocol_err_t
app_managed_list_add_ex(app_parser_instance_t *parser, void *rawData,
                        size_t itemSize, size_t itemCnt,
                        const protocol_field_descriptor_t *pCurrentField) {
  if (!parser || !parser->memCalls || !parser->internal_data)
    return PROTOCOL_ERR_ARG;

  if (!rawData)
    return PROTOCOL_ERR_PASSMSG;

  void *p = parser->memCalls->calloc(itemCnt, itemSize);
  if (!p)
    return PROTOCOL_ERR_MEM;

  memcpy(p, rawData, itemCnt * itemSize);

  parse_tempData_t *pFieldDataInfo =
      parser->memCalls->malloc(sizeof(*pFieldDataInfo));
  if (!pFieldDataInfo) {
    parser->memCalls->free(p);
    return PROTOCOL_ERR_MEM;
  }

  pFieldDataInfo->pData = p;
  pFieldDataInfo->itemCount = itemCnt;
  pFieldDataInfo->itemSize = itemSize;
  pFieldDataInfo->field = pCurrentField;

  parse_tempData_t *list_head = (parse_tempData_t *)parser->internal_data;
  list_add_tail(&pFieldDataInfo->node, &list_head->node);
  return PROTOCOL_OK;
}

/**
 * @brief 遍历链表并打印所有字段信息（示例：展示如何从节点还原结构体）
 */
static void app_managed_list_dump(void) {
  if (!g_parseData)
    return;

  ll_t *current;
  ll_t *next;
  int index = 0;

  // 安全遍历链表
  list_for_each_safe(current, next, &g_parseData->node) {
    // ✅ 关键：从 ll_t* 节点指针还原到包含它的 parse_tempData_t 结构体
    parse_tempData_t *field_info = list_entry(current, parse_tempData_t, node);

    // 现在可以访问完整结构体的所有成员
    APP_LOG("[%d] Field: %s", index++, field_info->field->name);
    APP_LOG("    ItemSize: %d, Count: %d", field_info->itemSize,
            field_info->itemCount);
    APP_LOG("    Data Ptr: %p", field_info->pData);
  }
}

// 基于实例的链表遍历（支持重入）
static void app_managed_list_dump_ex(app_parser_instance_t *parser) {
  if (!parser || !parser->internal_data)
    return;

  parse_tempData_t *list_head = (parse_tempData_t *)parser->internal_data;
  ll_t *current;
  ll_t *next;
  int index = 0;

  list_for_each_safe(current, next, &list_head->node) {
    parse_tempData_t *field_info = list_entry(current, parse_tempData_t, node);
    APP_LOG("[%d] Field: %s", index++, field_info->field->name);
    APP_LOG("    ItemSize: %d, Count: %d", field_info->itemSize,
            field_info->itemCount);
    APP_LOG("    Data Ptr: %p", field_info->pData);
  }
}

/**
 * @brief 释放整个链表及所有节点内存
 */
static void app_managed_list_destroy(void) {
  if (!g_parseData)
    return;

  ll_t *current;
  ll_t *next;

  // 安全遍历并释放每个节点
  list_for_each_safe(current, next, &g_parseData->node) {
    // 从节点还原结构体指针
    parse_tempData_t *field_info = list_entry(current, parse_tempData_t, node);

    // 先释放内部动态分配的 pData
    if (field_info->pData) {
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

// 基于实例的链表销毁（支持重入）
static void app_managed_list_destroy_ex(app_parser_instance_t *parser) {
  if (!parser || !parser->internal_data)
    return;

  parse_tempData_t *list_head = (parse_tempData_t *)parser->internal_data;
  ll_t *current;
  ll_t *next;

  list_for_each_safe(current, next, &list_head->node) {
    parse_tempData_t *field_info = list_entry(current, parse_tempData_t, node);

    if (field_info->pData) {
      parser->memCalls->free(field_info->pData);
      field_info->pData = NULL;
    }

    parser->memCalls->free(field_info);
  }

  parser->memCalls->free(list_head);
  parser->internal_data = NULL;
}

/**
 * @brief 计算字段的实际字节大小（处理变长逻辑）
 * @note itemCount >= 0 为定长；< 0 为变长模式
 */
static size_t calculate_field_size(const protocol_field_descriptor_t *field,
                                   const parsing_user_data_t *user) {

  // 定长：元素大小 * 个数
  if (field->itemCount >= 0) {
    return (size_t)field->itemSize * (size_t)field->itemCount;
  }

  switch (field->itemCount) {
  case FIELD_LEN_SYMBOL: // 关联前一个字段：从用户上下文中获取前一个字段的值作为个数
    return (size_t)field->itemSize * (size_t)user->uDataSize;
  case FIELD_END_SYMBOL:    // 结束符号（如 \0）：需要用户在回调中动态检测
  case FIELD_ALL_REMAINING: // 剩余全部：由用户在回调中根据 total_size 自行处理
  default:                  // 其他负值情况，默认返回 0
    return 0;
  }

  return 0;
}

/** IEEE 802.3 CRC-32 查找表（编译期生成，无运行时初始化竞态） */
static const uint32_t crc32_table[256] = {
    0x00000000, 0x77073096, 0xEE0E612C, 0x990951BA, 0x076DC419, 0x706AF48F,
    0xE963A535, 0x9E6495A3, 0x0EDB8832, 0x79DCB8A4, 0xE0D5E91E, 0x97D2D988,
    0x09B64C2B, 0x7EB17CBD, 0xE7B82D07, 0x90BF1D91, 0x1DB71064, 0x6AB020F2,
    0xF3B97148, 0x84BE41DE, 0x1ADAD47D, 0x6DDDE4EB, 0xF4D4B551, 0x83D385C7,
    0x136C9856, 0x646BA8C0, 0xFD62F97A, 0x8A65C9EC, 0x14015C4F, 0x63066CD9,
    0xFA0F3D63, 0x8D080DF5, 0x3B6E20C8, 0x4C69105E, 0xD56041E4, 0xA2677172,
    0x3C03E4D1, 0x4B04D447, 0xD20D85FD, 0xA50AB56B, 0x35B5A8FA, 0x42B2986C,
    0xDBBBC9D6, 0xACBCF940, 0x32D86CE3, 0x45DF5C75, 0xDCD60DCF, 0xABD13D59,
    0x26D930AC, 0x51DE003A, 0xC8D75180, 0xBFD06116, 0x21B4F4B5, 0x56B3C423,
    0xCFBA9599, 0xB8BDA50F, 0x2802B89E, 0x5F058808, 0xC60CD9B2, 0xB10BE924,
    0x2F6F7C87, 0x58684C11, 0xC1611DAB, 0xB6662D3D, 0x76DC4190, 0x01DB7106,
    0x98D220BC, 0xEFD5102A, 0x71B18589, 0x06B6B51F, 0x9FBFE4A5, 0xE8B8D433,
    0x7807C9A2, 0x0F00F934, 0x9609A88E, 0xE10E9818, 0x7F6A0DBB, 0x086D3D2D,
    0x91646C97, 0xE6635C01, 0x6B6B51F4, 0x1C6C6162, 0x856530D8, 0xF262004E,
    0x6C0695ED, 0x1B01A57B, 0x8208F4C1, 0xF50FC457, 0x65B0D9C6, 0x12B7E950,
    0x8BBEB8EA, 0xFCB9887C, 0x62DD1DDF, 0x15DA2D49, 0x8CD37CF3, 0xFBD44C65,
    0x4DB26158, 0x3AB551CE, 0xA3BC0074, 0xD4BB30E2, 0x4ADFA541, 0x3DD895D7,
    0xA4D1C46D, 0xD3D6F4FB, 0x4369E96A, 0x346ED9FC, 0xAD678846, 0xDA60B8D0,
    0x44042D73, 0x33031DE5, 0xAA0A4C5F, 0xDD0D7CC9, 0x5005713C, 0x270241AA,
    0xBE0B1010, 0xC90C2086, 0x5768B525, 0x206F85B3, 0xB966D409, 0xCE61E49F,
    0x5EDEF90E, 0x29D9C998, 0xB0D09822, 0xC7D7A8B4, 0x59B33D17, 0x2EB40D81,
    0xB7BD5C3B, 0xC0BA6CAD, 0xEDB88320, 0x9ABFB3B6, 0x03B6E20C, 0x74B1D29A,
    0xEAD54739, 0x9DD277AF, 0x04DB2615, 0x73DC1683, 0xE3630B12, 0x94643B84,
    0x0D6D6A3E, 0x7A6A5AA8, 0xE40ECF0B, 0x9309FF9D, 0x0A00AE27, 0x7D079EB1,
    0xF00F9344, 0x8708A3D2, 0x1E01F268, 0x6906C2FE, 0xF762575D, 0x806567CB,
    0x196C3671, 0x6E6B06E7, 0xFED41B76, 0x89D32BE0, 0x10DA7A5A, 0x67DD4ACC,
    0xF9B9DF6F, 0x8EBEEFF9, 0x17B7BE43, 0x60B08ED5, 0xD6D6A3E8, 0xA1D1937E,
    0x38D8C2C4, 0x4FDFF252, 0xD1BB67F1, 0xA6BC5767, 0x3FB506DD, 0x48B2364B,
    0xD80D2BDA, 0xAF0A1B4C, 0x36034AF6, 0x41047A60, 0xDF60EFC3, 0xA867DF55,
    0x316E8EEF, 0x4669BE79, 0xCB61B38C, 0xBC66831A, 0x256FD2A0, 0x5268E236,
    0xCC0C7795, 0xBB0B4703, 0x220216B9, 0x5505262F, 0xC5BA3BBE, 0xB2BD0B28,
    0x2BB45A92, 0x5CB36A04, 0xC2D7FFA7, 0xB5D0CF31, 0x2CD99E8B, 0x5BDEAE1D,
    0x9B64C2B0, 0xEC63F226, 0x756AA39C, 0x026D930A, 0x9C0906A9, 0xEB0E363F,
    0x72076785, 0x05005713, 0x95BF4A82, 0xE2B87A14, 0x7BB12BAE, 0x0CB61B38,
    0x92D28E9B, 0xE5D5BE0D, 0x7CDCEFB7, 0x0BDBDF21, 0x86D3D2D4, 0xF1D4E242,
    0x68DDB3F8, 0x1FDA836E, 0x81BE16CD, 0xF6B9265B, 0x6FB077E1, 0x18B74777,
    0x88085AE6, 0xFF0F6A70, 0x66063BCA, 0x11010B5C, 0x8F659EFF, 0xF862AE69,
    0x616BFFD3, 0x166CCF45, 0xA00AE278, 0xD70DD2EE, 0x4E048354, 0x3903B3C2,
    0xA7672661, 0xD06016F7, 0x4969474D, 0x3E6E77DB, 0xAED16A4A, 0xD9D65ADC,
    0x40DF0B66, 0x37D83BF0, 0xA9BCAE53, 0xDEBB9EC5, 0x47B2CF7F, 0x30B5FFE9,
    0xBDBDF21C, 0xCABAC28A, 0x53B39330, 0x24B4A3A6, 0xBAD03605, 0xCDD70693,
    0x54DE5729, 0x23D967BF, 0xB3667A2E, 0xC4614AB8, 0x5D681B02, 0x2A6F2B94,
    0xB40BBE37, 0xC30C8EA1, 0x5A05DF1B, 0x2D02EF8D,
};

/**
 * @brief 默认 CRC-32 实现（IEEE 802.3 标准）
 * @note 如果用户未提供 CRC 回调，可使用此默认实现
 */
static uint32_t app_crc32_default(const uint8_t *data, size_t len) {
  uint32_t crc = 0xFFFFFFFF;
  for (size_t i = 0; i < len; i++) {
    crc = (crc >> 8) ^ crc32_table[(crc ^ data[i]) & 0xFF];
  }
  return crc ^ 0xFFFFFFFF;
}

/**
 * @brief 验证消息的 CRC
 */
static protocol_err_t app_verify_crc(app_parser_instance_t *parser,
                                     const uint8_t *raw_data, size_t msg_len) {
  if (!parser || !parser->crc_enabled)
    return PROTOCOL_OK; // 未启用 CRC，直接通过

  if (!parser->crc_config.calc_crc && !parser->crc_config.verify_crc)
    return PROTOCOL_OK; // 未配置 CRC 回调

  const app_crc_config_t *crc_cfg = &parser->crc_config;

  // 检查 CRC 偏移是否合法
  if (crc_cfg->crc_offset + crc_cfg->crc_size > msg_len) {
    parser->last_error_code = PROTOCOL_ERR_CRC;
    return PROTOCOL_ERR_CRC;
  }

  // 提取期望的 CRC 值
  uint32_t expected_crc = 0;
  const uint8_t *crc_ptr = raw_data + crc_cfg->crc_offset;

  switch (crc_cfg->crc_size) {
  case 1:
    expected_crc = *crc_ptr;
    break;
  case 2:
    expected_crc = (uint16_t)(crc_ptr[0] | (crc_ptr[1] << 8));
    break;
  case 4:
    expected_crc = (uint32_t)(crc_ptr[0] | (crc_ptr[1] << 8) |
                              (crc_ptr[2] << 16) | (crc_ptr[3] << 24));
    break;
  default:
    parser->last_error_code = PROTOCOL_ERR_CRC;
    return PROTOCOL_ERR_CRC;
  }

  // 使用用户提供的验证函数
  if (crc_cfg->verify_crc) {
    if (!crc_cfg->verify_crc(raw_data, msg_len, expected_crc)) {
      parser->last_error_code = PROTOCOL_ERR_CRC;
      return PROTOCOL_ERR_CRC;
    }
    return PROTOCOL_OK;
  }

  // 使用默认的 CRC 计算函数
  if (crc_cfg->calc_crc) {
    // 计算除 CRC 字段外的数据的 CRC
    // 假设 CRC 在消息末尾
    size_t data_len = crc_cfg->crc_offset;
    uint32_t calculated_crc = crc_cfg->calc_crc(raw_data, data_len);

    if (calculated_crc != expected_crc) {
      parser->last_error_code = PROTOCOL_ERR_CRC;
      return PROTOCOL_ERR_CRC;
    }
    return PROTOCOL_OK;
  }

  return PROTOCOL_OK;
}

/**
 * @brief 解析单个字段
 */
static protocol_err_t
parse_single_field(parsing_user_data_t *user,
                   const protocol_field_descriptor_t *field,
                   const uint8_t *base_ptr) {
  if (!field || !base_ptr) {
    return PROTOCOL_ERR_ARG;
  }

  const uint8_t *field_ptr = base_ptr + field->offset;
  parsing_raw_data_t raw_view;
  raw_view.rawStream = (void *)field_ptr;

  // 计算字段大小
  size_t field_size = calculate_field_size(field, user);

  // 对于变长字段，如果 calculate_field_size 返回 0，需要特殊处理
  if (field_size == 0 && field->itemCount < 0) {
    // 变长字段：由回调函数自行决定如何处理
    // raw_view.streamSize = 0，回调函数需要根据实际情况处理
    raw_view.streamSize = 0;
  } else {
    raw_view.streamSize = field_size;
  }

  // 该字段没有回调配置
  if (!field->calls)
    return PROTOCOL_OK;

  // 检查是否启用零拷贝模式
  int is_zero_copy = (field->flags & FIELD_FLAG_ZERO_COPY) != 0;

  if (is_zero_copy) {
    // 零拷贝模式：直接传递源缓冲区指针
    // 注意：用户需要确保在回调中不修改数据，且原始缓冲区在回调期间保持有效
    if (field->calls->on_parse_callback) {
      protocol_err_t ret = field->calls->on_parse_callback(user, &raw_view);
      if (ret != PROTOCOL_OK) {
        return ret; // 支持用户返回 PROTOCOL_ERR_PASSMSG 提前终止
      }
    }
    return PROTOCOL_OK;
  }

  // 拷贝模式：计算实际的元素个数（处理变长字段）
  size_t actual_item_count = 0;
  size_t field_byte_size = calculate_field_size(field, user);

  if (field->itemCount >= 0) {
    // 定长字段：直接使用 itemCount
    actual_item_count = (size_t)field->itemCount;
  } else if (field_byte_size > 0 && field->itemSize > 0) {
    // 变长字段：根据字节大小计算元素个数
    actual_item_count = field_byte_size / (size_t)field->itemSize;
  } else {
    // 无法确定大小的变长字段（如 FIELD_END_SYMBOL），设为 1 让回调自行处理
    actual_item_count = 1;
    field_byte_size = (size_t)field->itemSize; // 至少拷贝一个元素
  }

  // 执行用户回调前，先将数据拷贝到安全的堆内存
  protocol_err_t ret = app_managed_list_add(
      raw_view.rawStream, (size_t)field->itemSize, actual_item_count, field);
  if (ret != PROTOCOL_OK) {
    APP_LOG("[%s]: list add error [%d]", __func__, ret);
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
  if (field->calls->on_parse_callback) {
    ret = field->calls->on_parse_callback(user, &safe_raw_view);
    if (ret != PROTOCOL_OK) {
      return ret; // 支持用户返回 PROTOCOL_ERR_PASSMSG 提前终止
    }
  }

  return PROTOCOL_OK;
}

/**
 * @brief 解析单个字段（重入安全版本）
 */
static protocol_err_t
parse_single_field_ex(app_parser_instance_t *parser, parsing_user_data_t *user,
                      const protocol_field_descriptor_t *field,
                      const uint8_t *base_ptr) {
  if (!parser || !field || !base_ptr) {
    return PROTOCOL_ERR_ARG;
  }

  const uint8_t *field_ptr = base_ptr + field->offset;
  parsing_raw_data_t raw_view;
  raw_view.rawStream = (void *)field_ptr;

  // 计算字段大小
  size_t field_size = calculate_field_size(field, user);

  // 对于变长字段，如果 calculate_field_size 返回 0，需要特殊处理
  if (field_size == 0 && field->itemCount < 0) {
    raw_view.streamSize = 0;
  } else {
    raw_view.streamSize = field_size;
  }

  // 该字段没有回调配置
  if (!field->calls)
    return PROTOCOL_OK;

  // 检查是否启用零拷贝模式
  int is_zero_copy = (field->flags & FIELD_FLAG_ZERO_COPY) != 0;

  if (is_zero_copy) {
    // 零拷贝模式：直接传递源缓冲区指针
    if (field->calls->on_parse_callback) {
      protocol_err_t ret = field->calls->on_parse_callback(user, &raw_view);
      if (ret != PROTOCOL_OK) {
        return ret;
      }
    }
    return PROTOCOL_OK;
  }

  // 拷贝模式：计算实际的元素个数
  size_t actual_item_count = 0;
  size_t field_byte_size = calculate_field_size(field, user);

  if (field->itemCount >= 0) {
    actual_item_count = (size_t)field->itemCount;
  } else if (field_byte_size > 0 && field->itemSize > 0) {
    actual_item_count = field_byte_size / (size_t)field->itemSize;
  } else {
    actual_item_count = 1;
    field_byte_size = (size_t)field->itemSize;
  }

  // 使用实例版本的链表添加
  protocol_err_t ret = app_managed_list_add_ex(parser, raw_view.rawStream,
                                               (size_t)field->itemSize,
                                               actual_item_count, field);
  if (ret != PROTOCOL_OK) {
    APP_LOG("[%s]: list add error [%d]", __func__, ret);
    return ret;
  }

  // 从链表中获取最新添加的节点
  parse_tempData_t *list_head = (parse_tempData_t *)parser->internal_data;
  ll_t *last_node = list_head->node.prev;
  parse_tempData_t *safe_data = list_entry(last_node, parse_tempData_t, node);

  // 构造指向安全拷贝数据的 raw_view
  parsing_raw_data_t safe_raw_view;
  safe_raw_view.rawStream = safe_data->pData;
  safe_raw_view.streamSize = safe_data->itemCount * safe_data->itemSize;

  // 执行用户回调
  if (field->calls->on_parse_callback) {
    ret = field->calls->on_parse_callback(user, &safe_raw_view);
    if (ret != PROTOCOL_OK) {
      return ret;
    }
  }

  return PROTOCOL_OK;
}

protocol_err_t app_memCall_init(const parsing_memCall_t *memCalls) {
  const size_t testMem0_memSize = 64;
  const size_t testMem1_memSize = 256;
  uint8_t *testp[2] = {NULL};
  if (!memCalls)
    return PROTOCOL_ERR_ARG;

  if (!memCalls->malloc || !memCalls->calloc || !memCalls->realloc ||
      !memCalls->free)
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

  for (int idx = 0; idx < FIELD_ARR_SIZE(testp); idx++) {
    if (testp[idx]) {
      g_memCalls->free(testp[idx]);
    }
  }

  return PROTOCOL_OK;

_err:
  for (int idx = 0; idx < FIELD_ARR_SIZE(testp); idx++) {
    if (testp[idx]) {
      g_memCalls->free(testp[idx]);
    }
  }

  return PROTOCOL_ERR_FUNCS;
}

/**
 * @brief 初始化解析器实例（支持重入）
 */
protocol_err_t app_parser_init(app_parser_instance_t *parser,
                               const parsing_memCall_t *memCalls) {
  if (!parser || !memCalls)
    return PROTOCOL_ERR_ARG;

  if (!memCalls->malloc || !memCalls->calloc || !memCalls->realloc ||
      !memCalls->free)
    return PROTOCOL_ERR_ARG;

  memset(parser, 0, sizeof(app_parser_instance_t));
  parser->memCalls = memCalls;
  parser->internal_data = NULL;
  parser->crc_enabled = false;
  parser->last_error_code = 0;
  parser->user_context = NULL;

  // 测试内存回调函数
  const size_t test_size = 64;
  void *test_ptr = memCalls->malloc(test_size);
  if (!test_ptr)
    return PROTOCOL_ERR_FUNCS;

  memCalls->free(test_ptr);
  return PROTOCOL_OK;
}

/**
 * @brief 反初始化解析器实例
 */
protocol_err_t app_parser_deinit(app_parser_instance_t *parser) {
  if (!parser)
    return PROTOCOL_ERR_ARG;

  // 清理可能残留的内部数据
  if (parser->internal_data) {
    app_managed_list_destroy_ex(parser);
  }

  memset(parser, 0, sizeof(app_parser_instance_t));
  return PROTOCOL_OK;
}

/**
 * @brief 配置 CRC 校验
 */
void app_parser_set_crc_config(app_parser_instance_t *parser,
                               const app_crc_config_t *crc_config) {
  if (!parser || !crc_config)
    return;

  memcpy(&parser->crc_config, crc_config, sizeof(app_crc_config_t));
}

/**
 * @brief 启用/禁用 CRC 校验
 */
void app_parser_enable_crc(app_parser_instance_t *parser, bool enable) {
  if (!parser)
    return;

  parser->crc_enabled = enable;

  // 如果启用但未配置 CRC 计算函数，使用默认实现
  if (enable && !parser->crc_config.calc_crc &&
      !parser->crc_config.verify_crc) {
    parser->crc_config.calc_crc = app_crc32_default;
    parser->crc_config.crc_size = 4; // 默认 CRC-32
  }
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
                                 const uint8_t *raw_data) {
    protocol_err_t parse_err = PROTOCOL_OK;
  
  if (!user || !msg_desc || !raw_data) {
    return PROTOCOL_ERR_ARG;
  }

  if (!g_memCalls || !g_memCalls->malloc || !g_memCalls->calloc ||
      !g_memCalls->realloc || !g_memCalls->free) {
    return PROTOCOL_ERR_CALLS_INIT;
  }

  // 1. 消息开始回调
  if (msg_desc->on_message_start_callback) {
    parsing_raw_data_t full_msg = {(void *)raw_data, msg_desc->total_size};
    msg_desc->on_message_start_callback(user, &full_msg);
  }

  app_managed_list_init();
  // 2. 遍历所有字段进行提取
  for (uint16_t i = 0; i < msg_desc->num_fields; i++) {
    protocol_err_t err =
        parse_single_field(user, &msg_desc->fields[i], raw_data);
    if (err != PROTOCOL_OK) {
      // 遇到错误或用户要求跳过(PROTOCOL_ERR_PASSMSG)则立即跳出
      // 务必记得释放堆空间, 避免内存泄漏
      parse_err = err;
      break;
    }
  }
#if APP_TEMPDATA_DEBUG
  app_managed_list_dump();
#endif
  app_managed_list_destroy();


  // 3. 消息结束回调
  if (msg_desc->on_message_end_callback) {
    parsing_raw_data_t full_msg = {(void *)raw_data, msg_desc->total_size};
    msg_desc->on_message_end_callback(user, &full_msg);
  }

  return parse_err;
}

/**
 * @brief 根据消息模板解析整条报文（重入安全版本）
 * @param parser 解析器实例
 * @param user 用户自定义数据上下文
 * @param msg_desc 消息描述符（模板）
 * @param raw_data 原始报文字节流
 * @return PROTOCOL_OK 或错误码
 */
protocol_err_t
app_parse_message_ex(app_parser_instance_t *parser, parsing_user_data_t *user,
                     const protocol_message_descriptor_t *msg_desc,
                     const uint8_t *raw_data) {
    protocol_err_t parse_err = PROTOCOL_OK;
  if (!parser || !user || !msg_desc || !raw_data) {
    return PROTOCOL_ERR_ARG;
  }

  if (!parser->memCalls || !parser->memCalls->malloc ||
      !parser->memCalls->calloc || !parser->memCalls->realloc ||
      !parser->memCalls->free) {
    return PROTOCOL_ERR_CALLS_INIT;
  }

  // 0. CRC 校验（如果启用）
  if (parser->crc_enabled) {
    // 计算消息总长度（如果是变长消息，需要特殊处理）
    size_t msg_len =
        (msg_desc->total_size > 0) ? (size_t)msg_desc->total_size : 0;

    // 对于变长消息，尝试从用户数据中获取长度
    if (msg_len == 0 && user->uDataSize > 0) {
      msg_len = user->uDataSize;
    }

    if (msg_len > 0) {
      protocol_err_t crc_ret = app_verify_crc(parser, raw_data, msg_len);
      if (crc_ret != PROTOCOL_OK) {
        return crc_ret; // 直接返回 PROTOCOL_ERR_CRC
      }
    }
  }

  // 1. 消息开始回调
  if (msg_desc->on_message_start_callback) {
    parsing_raw_data_t full_msg = {(void *)raw_data, msg_desc->total_size};
    msg_desc->on_message_start_callback(user, &full_msg);
  }

  // 初始化实例内部的链表
  protocol_err_t ret = app_managed_list_init_ex(parser);
  if (ret != PROTOCOL_OK) {
    return ret;
  }

  // 2. 遍历所有字段进行提取
  for (uint16_t i = 0; i < msg_desc->num_fields; i++) {
    protocol_err_t err =
        parse_single_field_ex(parser, user, &msg_desc->fields[i], raw_data);
    if (err != PROTOCOL_OK) {
      // 遇到错误或用户要求跳过(PROTOCOL_ERR_PASSMSG)则立即跳出
      break;
    }
  }
#if APP_TEMPDATA_DEBUG
  // 调试输出（可选）
  app_managed_list_dump_ex(parser);
#endif

  // 清理内部链表
  app_managed_list_destroy_ex(parser);

  // 3. 消息结束回调
  if (msg_desc->on_message_end_callback) {
    parsing_raw_data_t full_msg = {(void *)raw_data, msg_desc->total_size};
    msg_desc->on_message_end_callback(user, &full_msg);
  }

  return PROTOCOL_OK;
}
