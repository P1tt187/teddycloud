
#define TRACE_LEVEL TRACE_LEVEL_INFO

#include "esp32.h"

#include <errno.h>          // for error_t
#include <inttypes.h>       // for PRIX32, PRIu32, PRIX8, PRIX16, PRIX64
#include <stdint.h>         // for uint8_t, uint32_t, uint16_t, int16_t, int...
#include <stdbool.h>        // for bool, true, false
#include <string.h>         // for size_t, strcmp, memcmp, memset, NULL
#include <time.h>           // for time_t

#include "cert.h"           // for cert_generate_mac
#include "date_time.h"      // for DateTime, convertUnixTimeToDate, getCurre...
#include "debug.h"          // for TRACE_INFO, TRACE_ERROR, TRACE_LEVEL_INFO
#include "diskio.h"         // for RES_OK, DRESULT, DSTATUS, RES_ERROR, disk...
#include "error.h"          // for NO_ERROR, ERROR_FAILURE, ERROR_NOT_FOUND
#include "ff.h"             // for FILINFO, FR_OK, BYTE, f_mount, f_close
#include "fs_ext.h"         // for fsOpenFileEx
#include "fs_port.h"        // for FS_SEEK_SET, FsDirEntry, FS_FILE_MODE_WRITE
#include "hash/sha256.h"    // for sha256Update, sha256Final, sha256Init
#include "os_port.h"        // for osFreeMem, osAllocMem, osStrcpy, osStrlen
#include "path.h"           // for pathAddSlash, pathCanonicalize, pathCombine
#include "pem_import.h"     // for pemImportCertificate
#include "settings.h"       // for settings_get_string
#include "server_helpers.h" // for custom_asprintf

#define ESP_PARTITION_TYPE_APP 0
#define ESP_PARTITION_TYPE_DATA 1

#pragma pack(push, 1)

/*
 ********** ESP32 partition structures **********
 */
struct ESP32_part_entry
{
    uint16_t magic;
    uint8_t partType;
    uint8_t partSubType;
    uint32_t fileOffset;
    uint32_t length;
    uint8_t label[16];
    uint32_t reserved;
};

/*
 ********** ESP32 firmware structures **********
 */
struct ESP32_segment
{
    uint32_t loadAddress;
    uint32_t length;
    // uint8_t data[length];
};

struct ESP32_EFH
{
    uint8_t wp_pin;
    uint8_t spi_drive[3];
    uint16_t chip_id;
    uint8_t min_chip_rev_old;
    uint16_t min_chip_revision;
    uint16_t max_chip_revision;
    uint8_t reserved[4];
    uint8_t has_hash;
};

struct ESP32_header
{
    uint8_t magic;
    uint8_t segments;
    uint8_t flashMode;
    uint8_t flashInfo;
    uint32_t entry;
    struct ESP32_EFH extended;
    // ESP32_segment segment_data[segments];
};

struct ESP32_footer
{
    uint8_t chk;
    uint8_t sha[32];
};

/*
 ********** ESP32 asset wear levelling structures **********
 */
#define WL_CFG_SECTORS_COUNT 1
#define WL_DUMMY_SECTORS_COUNT 1
#define WL_CONFIG_HEADER_SIZE 48
#define WL_STATE_RECORD_SIZE 16
#define WL_STATE_HEADER_SIZE 64
#define WL_STATE_COPY_COUNT 2
#define WL_SECTOR_SIZE 0x1000

struct WL_STATE_T_DATA
{
    uint32_t pos;
    uint32_t max_pos;
    uint32_t move_count;
    uint32_t access_count;
    uint32_t max_count;
    uint32_t block_size;
    uint32_t version;
    uint32_t device_id;
    uint8_t reserved[28];
};

struct WL_CONFIG_T_DATA
{
    uint32_t start_addr;
    uint32_t full_mem_size;
    uint32_t page_size;
    uint32_t sector_size;
    uint32_t updaterate;
    uint32_t wr_size;
    uint32_t version;
    uint32_t temp_buff_size;
};

struct wl_state
{
    struct WL_STATE_T_DATA wl_state;
    size_t fs_offset;
    size_t wl_sectors_size;
    size_t partition_size;
    size_t fat_sectors;
    size_t total_sectors;
    size_t total_records;
    size_t wl_state_size;
    size_t wl_state_sectors_cnt;
    FsFile *file;
};

#pragma pack(pop)

struct wl_state esp32_wl_state;

size_t esp32_wl_translate(const struct wl_state *state, size_t sector)
{
    sector = (sector + state->wl_state.move_count) % state->fat_sectors;

    if (sector >= state->total_records)
    {
        sector += 1;
    }
    return sector;
}

DWORD get_fattime()
{
    // Retrieve current time
    time_t time = getCurrentUnixTime();
    DateTime date;
    convertUnixTimeToDate(time, &date);

    return (
        (uint32_t)(date.year - 1980) << 25 |
        (uint32_t)(date.month) << 21 |
        (uint32_t)(date.day) << 16 |
        (uint32_t)(date.hours) << 11 |
        (uint32_t)(date.minutes) << 5 |
        (uint32_t)(date.seconds) >> 1);
}

DRESULT disk_ioctl(BYTE pdrv, BYTE cmd, void *buff)
{
    return RES_OK;
}

DSTATUS disk_status(BYTE pdrv)
{
    return RES_OK;
}

DSTATUS disk_initialize(BYTE pdrv)
{
    return RES_OK;
}

DRESULT disk_write(BYTE pdrv, const BYTE *buff, LBA_t sector, UINT count)
{
    struct wl_state *state = (struct wl_state *)&esp32_wl_state;

    for (int sec = 0; sec < count; sec++)
    {
        size_t trans_sec = esp32_wl_translate(state, sector + sec);

        fsSeekFile(state->file, state->fs_offset + trans_sec * WL_SECTOR_SIZE, FS_SEEK_SET);

        error_t error = fsWriteFile(state->file, (void *)&buff[sec * WL_SECTOR_SIZE], WL_SECTOR_SIZE);
        if (error != NO_ERROR)
        {
            TRACE_ERROR("Failed to write sector\r\n");
            return RES_ERROR;
        }
    }
    return RES_OK;
}

DRESULT disk_read(BYTE pdrv, BYTE *buff, LBA_t sector, UINT count)
{
    struct wl_state *state = (struct wl_state *)&esp32_wl_state;

    for (int sec = 0; sec < count; sec++)
    {
        size_t read;
        size_t trans_sec = esp32_wl_translate(state, sector + sec);

        fsSeekFile(state->file, state->fs_offset + trans_sec * WL_SECTOR_SIZE, FS_SEEK_SET);

        error_t error = fsReadFile(state->file, (void *)&buff[sec * WL_SECTOR_SIZE], WL_SECTOR_SIZE, &read);
        if (error != NO_ERROR || read != WL_SECTOR_SIZE)
        {
            TRACE_ERROR("Failed to read sector\r\n");
            return RES_ERROR;
        }
    }
    return RES_OK;
}

error_t esp32_wl_init(struct wl_state *state, FsFile *file, size_t offset, size_t length)
{
    state->fs_offset = offset;
    state->file = file;
    state->partition_size = length;
    state->total_sectors = state->partition_size / WL_SECTOR_SIZE;
    state->wl_state_size = WL_STATE_HEADER_SIZE + WL_STATE_RECORD_SIZE * state->total_sectors;
    state->wl_state_sectors_cnt = (state->wl_state_size + WL_SECTOR_SIZE - 1) / WL_SECTOR_SIZE;
    state->wl_sectors_size = (state->wl_state_sectors_cnt * WL_SECTOR_SIZE * WL_STATE_COPY_COUNT) + WL_SECTOR_SIZE;
    state->fat_sectors = state->total_sectors - 1 - (WL_STATE_COPY_COUNT * state->wl_state_sectors_cnt);

    fsSeekFile(file, offset + state->partition_size - state->wl_sectors_size, FS_SEEK_SET);

    size_t read;
    error_t error = fsReadFile(file, &state->wl_state, sizeof(state->wl_state), &read);

    if (error != NO_ERROR || read != sizeof(state->wl_state))
    {
        TRACE_ERROR("Failed to read wl_state\r\n");
        return ERROR_FAILURE;
    }

    uint8_t state_record_empty[WL_STATE_RECORD_SIZE];
    memset(state_record_empty, 0xFF, WL_STATE_RECORD_SIZE);
    for (int pos = 0; pos < state->wl_state_size; pos++)
    {
        uint8_t state_record[WL_STATE_RECORD_SIZE];
        error = fsReadFile(file, state_record, sizeof(state_record), &read);

        if (error != NO_ERROR || read != sizeof(state_record))
        {
            TRACE_ERROR("Failed to read state_record\r\n");
            return ERROR_FAILURE;
        }
        if (!memcmp(state_record_empty, state_record, WL_STATE_RECORD_SIZE))
        {
            break;
        }
        state->total_records++;
    }

    return NO_ERROR;
}

/* according to https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/storage/nvs_flash.html */
#define NVS_SECTOR_SIZE 4096

#define NVS_PAGE_BIT_INIT 1
#define NVS_PAGE_BIT_FULL 2
#define NVS_PAGE_BIT_FREEING 4
#define NVS_PAGE_BIT_CORRUPT 8

#define NVS_PAGE_STATE_UNINIT 0xFFFFFFFF
#define NVS_PAGE_STATE_ACTIVE (NVS_PAGE_STATE_UNINIT & ~NVS_PAGE_BIT_INIT)
#define NVS_PAGE_STATE_FULL (NVS_PAGE_STATE_ACTIVE & ~NVS_PAGE_BIT_FULL)
#define NVS_PAGE_STATE_FREEING (NVS_PAGE_STATE_FULL & ~NVS_PAGE_BIT_FREEING)
#define NVS_PAGE_STATE_CORRUPT (NVS_PAGE_STATE_FREEING & ~NVS_PAGE_BIT_CORRUPT)

#define MAX_NAMESPACE_COUNT 8
#define MAX_KEY_SIZE 16
#define MAX_ENTRY_COUNT 126
#define MAX_STRING_LENGTH 64
#define BUFFER_HEX_SIZE(length) ((length) * 3 + 3)

enum ESP32_nvs_type
{
    NVS_TYPE_U8 = 0x01,
    NVS_TYPE_U16 = 0x02,
    NVS_TYPE_U32 = 0x04,
    NVS_TYPE_U64 = 0x08,
    NVS_TYPE_I8 = 0x11,
    NVS_TYPE_I16 = 0x12,
    NVS_TYPE_I32 = 0x14,
    NVS_TYPE_I64 = 0x18,
    NVS_TYPE_STR = 0x21,
    NVS_TYPE_BLOB = 0x42,
    NVS_TYPE_BLOB_IDX = 0x48,
    NVS_TYPE_ANY = 0xff
};

enum ESP32_nvs_item_state
{
    NVS_STATE_ERASED = 0x00,
    NVS_STATE_WRITTEN = 0x02,
    NVS_STATE_EMPTY = 0x03
};

struct ESP32_nvs_page_header
{
    uint32_t state;
    uint32_t seq;
    uint8_t version;
    uint8_t unused[19];
    uint32_t crc32;
    uint8_t state_bitmap[32];
};

struct ESP32_nvs_item
{
    uint8_t nsIndex;
    uint8_t datatype;
    uint8_t span;
    uint8_t chunkIndex;
    uint32_t crc32;
    char key[16];
    union
    {
        struct
        {
            uint16_t size;
            uint16_t reserved;
            uint32_t data_crc32;
        } var_length;
        struct
        {
            uint32_t size;
            uint8_t chunk_count;
            uint8_t chunk_start;
            uint16_t reserved;
        } blob_index;
        uint8_t data[8];
        uint8_t uint8;
        uint16_t uint16;
        uint32_t uint32;
        uint64_t uint64;
        int8_t int8;
        int16_t int16;
        int32_t int32;
        int64_t int64;
    };
};

uint8_t esp32_nvs_state_get(struct ESP32_nvs_page_header *page_header, uint8_t index)
{
    int bmp_idx = index / 4;
    int bmp_bit = (index % 4) * 2;
    uint8_t bmp = (page_header->state_bitmap[bmp_idx] >> (bmp_bit)) & 3;

    return bmp;
}

void esp32_nvs_state_set(struct ESP32_nvs_page_header *page_header, uint8_t index, uint8_t state)
{
    int bmp_idx = index / 4;
    int bmp_bit = (index % 4) * 2;

    page_header->state_bitmap[bmp_idx] &= ~(3 << bmp_bit);
    page_header->state_bitmap[bmp_idx] |= (state << bmp_bit);
}

static uint32_t crc32_byte(uint32_t crc, uint8_t d)
{
    uint8_t b;

    for (b = 1; b; b <<= 1)
    {
        crc ^= (d & b) ? 1 : 0;
        crc = (crc & 1) ? crc >> 1 ^ 0xEDB88320 : crc >> 1;
    }
    return crc;
}

static uint32_t crc32(void *ptr, int length)
{
    uint32_t crc = 0;
    uint8_t *data = (uint8_t *)ptr;

    while (length--)
    {
        crc = crc32_byte(crc, *data);
        data++;
    }

    return ~crc;
}

static uint32_t crc32_header(void *header)
{
    uint8_t *ptr = (uint8_t *)header;
    uint8_t buf[0x20 - 4];

    osMemcpy(&buf[0], &ptr[0], 0x04);
    osMemcpy(&buf[4], &ptr[8], 0x18);

    return crc32(buf, 0x1C);
}

static error_t file_read_block(FsFile *file, size_t offset, void *buffer, size_t length)
{
    size_t read;

    error_t error = fsSeekFile(file, offset, FS_SEEK_SET);
    if (error != NO_ERROR)
    {
        TRACE_ERROR("Failed to seek input file\r\n");
        return ERROR_FAILURE;
    }

    error = fsReadFile(file, buffer, length, &read);
    if (error != NO_ERROR)
    {
        TRACE_ERROR("Failed to read input file: %i\r\n", error);
        return ERROR_FAILURE;
    }

    if (length != read)
    {
        TRACE_ERROR("Failed to read enough data\r\n");
        return ERROR_FAILURE;
    }

    return NO_ERROR;
}

static error_t file_write_block(FsFile *file, size_t offset, void *buffer, size_t length)
{
    error_t error = fsSeekFile(file, offset, FS_SEEK_SET);
    if (error != NO_ERROR)
    {
        TRACE_ERROR("Failed to seek input file\r\n");
        return ERROR_FAILURE;
    }

    error = fsWriteFile(file, buffer, length);
    if (error != NO_ERROR)
    {
        TRACE_ERROR("Failed to write file\r\n");
        return ERROR_FAILURE;
    }

    return NO_ERROR;
}

/**
 * @brief Processes a single NVS item.
 *
 * This function processes a single NVS item, updates its CRC if necessary, and
 * logs relevant information about the item. If the CRC check fails, the function
 * updates the CRC and returns an error code indicating the CRC mismatch.
 *
 * @param file Pointer to the file to read/write.
 * @param offset Offset within the file.
 * @param part_offset Offset of the specific NVS item within the file.
 * @param item Pointer to the NVS item structure.
 * @param namespaces Array of namespace strings, passed by reference.
 * @return
 *         - NO_ERROR if successful.
 *         - ERROR_BAD_CRC if the CRC check fails and the CRC is updated.
 *         - ERROR_FAILURE for other errors.
 */
static error_t process_nvs_item(FsFile *file, size_t offset, size_t part_offset, struct ESP32_nvs_item *item, char (*namespaces)[MAX_NAMESPACE_COUNT][MAX_KEY_SIZE])
{
    error_t error = NO_ERROR;

    if (item->nsIndex == 0)
    {
        TRACE_INFO("      Namespace   %s\r\n", item->key);
        if (item->uint8 > MAX_NAMESPACE_COUNT)
        {
            TRACE_ERROR("namespace index is %" PRIu32 ", which seems invalid", item->uint8);
            return ERROR_FAILURE;
        }
        osStrcpy((*namespaces)[item->uint8], item->key);

        uint32_t crc_header_calc = crc32_header(item);
        TRACE_INFO("      Header CRC  %08" PRIX32 " (calc %08" PRIX32 ")\r\n", item->crc32, crc_header_calc);
        if (item->crc32 != crc_header_calc)
        {
            TRACE_INFO("      * Updating CRC\r\n");
            item->crc32 = crc_header_calc;
            error = ERROR_BAD_CRC;
        }
    }
    else
    {
        if (item->nsIndex > MAX_NAMESPACE_COUNT)
        {
            TRACE_ERROR("      Namespace   index is %" PRIu32 ", which seems invalid", item->nsIndex);
        }
        else
        {
            TRACE_INFO("      Namespace   %s\r\n", (*namespaces)[item->nsIndex]);
        }

        TRACE_INFO("      Key         %s\r\n", item->key);

        switch (item->datatype & 0xF0)
        {
        case 0x00:
        case 0x10:
        {
            char type_string[32] = {0};
            osSprintf(type_string, "%sint%i_t", (item->datatype & 0xF0) ? "" : "u", (item->datatype & 0x0F) * 8);
            TRACE_INFO("      Type        %s (0x%08" PRIX32 ")\r\n", type_string, item->datatype);
            TRACE_INFO("      Chunk Index 0x%02" PRIX8 "\r\n", item->chunkIndex);
            switch (item->datatype)
            {
            case NVS_TYPE_U8:
                TRACE_INFO("      Value       %" PRIu8 " (0x%02" PRIX8 ")\r\n", item->uint8, item->uint8);
                break;
            case NVS_TYPE_U16:
                TRACE_INFO("      Value       %" PRIu16 " (0x%04" PRIX16 ")\r\n", item->uint16, item->uint16);
                break;
            case NVS_TYPE_U32:
                TRACE_INFO("      Value       %" PRIu32 " (0x%08" PRIX32 ")\r\n", item->uint32, item->uint32);
                break;
            case NVS_TYPE_U64:
                TRACE_INFO("      Value       %" PRIu64 " (0x%016" PRIX64 ")\r\n", item->uint64, item->uint64);
                break;
            case NVS_TYPE_I8:
                TRACE_INFO("      Value       %" PRId8 " (0x%02" PRIX8 ")\r\n", item->int8, item->int8);
                break;
            case NVS_TYPE_I16:
                TRACE_INFO("      Value       %" PRId16 " (0x%04" PRIX16 ")\r\n", item->int16, item->int16);
                break;
            case NVS_TYPE_I32:
                TRACE_INFO("      Value       %" PRId32 " (0x%08" PRIX32 ")\r\n", item->int32, item->int32);
                break;
            case NVS_TYPE_I64:
                TRACE_INFO("      Value       %" PRId64 " (0x%016" PRIX64 ")\r\n", item->int64, item->int64);
                break;
            }
            break;
        }
        }

        switch (item->datatype)
        {
        case NVS_TYPE_STR:
        {
            TRACE_INFO("      Type        string (0x%02" PRIX8 ")\r\n", item->datatype);
            TRACE_INFO("      Chunk Index 0x%02" PRIX8 "\r\n", item->chunkIndex);
            uint16_t length = item->uint16;
            if (length > MAX_STRING_LENGTH)
            {
                TRACE_ERROR("length is %" PRIu32 ", which seems invalid", length);
                return ERROR_FAILURE;
            }
            char *buffer = osAllocMem(length);
            if (!buffer)
            {
                TRACE_ERROR("Failed to allocate memory\r\n");
                return ERROR_FAILURE;
            }

            error = file_read_block(file, offset + part_offset + sizeof(struct ESP32_nvs_item), buffer, length);
            if (error != NO_ERROR)
            {
                osFreeMem(buffer);
                return ERROR_FAILURE;
            }

            uint32_t crc_data_calc = crc32(buffer, length);
            TRACE_INFO("      Size        %" PRIu32 "\r\n", item->var_length.size);
            TRACE_INFO("      Data        %s\r\n", buffer);
            TRACE_INFO("      Data CRC    %08" PRIX32 " (calc %08" PRIX32 ")\r\n", item->var_length.data_crc32, crc_data_calc);
            if (item->var_length.data_crc32 != crc_data_calc)
            {
                TRACE_INFO("      * Updating CRC\r\n");
                item->var_length.data_crc32 = crc_data_calc;
                error = ERROR_BAD_CRC;
            }
            osFreeMem(buffer);
            break;
        }

        case NVS_TYPE_BLOB_IDX:
            TRACE_INFO("      Type        blob index (0x%02" PRIX8 ")\r\n", item->datatype);
            TRACE_INFO("      Chunk Index 0x%02" PRIX8 "\r\n", item->chunkIndex);
            TRACE_INFO("      Total size  %" PRIu32 "\r\n", item->blob_index.size);
            TRACE_INFO("      Chunk count %" PRIu32 "\r\n", item->blob_index.chunk_count);
            TRACE_INFO("      Chunk start %" PRIu32 "\r\n", item->blob_index.chunk_start);
            break;

        case NVS_TYPE_BLOB:
        {
            TRACE_INFO("      Type        blob (0x%" PRIX8 ")\r\n", item->datatype);
            TRACE_INFO("      Chunk Index 0x%02" PRIX8 "\r\n", item->chunkIndex);
            uint16_t length = item->uint16;
            if (length > MAX_STRING_LENGTH)
            {
                TRACE_ERROR("length is %" PRIu32 ", which seems invalid", length);
                return ERROR_FAILURE;
            }
            uint8_t *buffer = osAllocMem(length);
            char *buffer_hex = osAllocMem(BUFFER_HEX_SIZE(length));
            if (!buffer || !buffer_hex)
            {
                TRACE_ERROR("Failed to allocate memory\r\n");
                osFreeMem(buffer);
                osFreeMem(buffer_hex);
                return ERROR_FAILURE;
            }

            error = file_read_block(file, offset + part_offset + sizeof(struct ESP32_nvs_item), buffer, length);
            if (error != NO_ERROR)
            {
                osFreeMem(buffer);
                osFreeMem(buffer_hex);
                return ERROR_FAILURE;
            }

            for (int hex_pos = 0; hex_pos < length; hex_pos++)
            {
                osSprintf(&buffer_hex[3 * hex_pos], "%02" PRIX8 " ", buffer[hex_pos]);
            }
            uint32_t crc_data_calc = crc32(buffer, length);
            TRACE_INFO("      Size        %" PRIu32 "\r\n", item->var_length.size);
            TRACE_INFO("      Data        %s\r\n", buffer_hex);
            TRACE_INFO("      Data CRC    %08" PRIX32 " (calc %08" PRIX32 ")\r\n", item->var_length.data_crc32, crc_data_calc);
            if (item->var_length.data_crc32 != crc_data_calc)
            {
                TRACE_INFO("      * Updating CRC\r\n");
                item->var_length.data_crc32 = crc_data_calc;
                error = ERROR_BAD_CRC;
            }
            osFreeMem(buffer);
            osFreeMem(buffer_hex);
            break;
        }
        }

        uint32_t crc_header_calc = crc32_header(item);
        TRACE_INFO("      Header CRC  %08" PRIX32 " (calc %08" PRIX32 ")\r\n", item->crc32, crc_header_calc);
        if (item->crc32 != crc_header_calc)
        {
            TRACE_INFO("      * Updating CRC\r\n");
            item->crc32 = crc_header_calc;
            error = ERROR_BAD_CRC;
        }
    }

    return error;
}

/**
 * @brief Fixes up the NVS data.
 *
 * This function scans through the NVS data in the specified file, updates CRCs,
 * and logs relevant information about the NVS pages and items.
 *
 * @param file Pointer to the file to read/write.
 * @param offset Offset within the file.
 * @param length Length of the data to process.
 * @param modify Flag indicating whether to modify the file.
 * @return Error code indicating success or failure.
 */
error_t esp32_fixup_nvs(FsFile *file, size_t offset, size_t length, bool modify)
{
    char namespaces[MAX_NAMESPACE_COUNT][MAX_KEY_SIZE] = {0};

    for (size_t sector_offset = 0; sector_offset < length; sector_offset += NVS_SECTOR_SIZE)
    {
        error_t error;
        struct ESP32_nvs_page_header page_header;
        size_t block_offset = offset + sector_offset;

        error = file_read_block(file, block_offset, &page_header, sizeof(page_header));
        if (error != NO_ERROR)
        {
            TRACE_ERROR("Failed to read input file\r\n");
            return ERROR_FAILURE;
        }

        switch (page_header.state)
        {
        case NVS_PAGE_STATE_UNINIT:
            TRACE_INFO("NVS Block --, offset 0x%08" PRIX32 "\r\n", (uint32_t)block_offset);
            TRACE_INFO("  State: %s\r\n", "Uninitialized");
            continue;
        case NVS_PAGE_STATE_CORRUPT:
            TRACE_INFO("NVS Block --, offset 0x%08" PRIX32 "\r\n", (uint32_t)block_offset);
            TRACE_INFO("  State: %s\r\n", "Corrupt");
            continue;

        case NVS_PAGE_STATE_ACTIVE:
            TRACE_INFO("NVS Block #%" PRIu32 ", offset 0x%08" PRIX32 "\r\n", page_header.seq, (uint32_t)block_offset);
            TRACE_INFO("  State: %s\r\n", "Active");
            break;
        case NVS_PAGE_STATE_FULL:
            TRACE_INFO("NVS Block #%" PRIu32 ", offset 0x%08" PRIX32 "\r\n", page_header.seq, (uint32_t)block_offset);
            TRACE_INFO("  State: %s\r\n", "Full");
            break;
        case NVS_PAGE_STATE_FREEING:
            TRACE_INFO("NVS Block #%" PRIu32 ", offset 0x%08" PRIX32 "\r\n", page_header.seq, (uint32_t)block_offset);
            TRACE_INFO("  State: %s\r\n", "Freeing");
            break;
        }

        TRACE_INFO("  Header CRC  %08" PRIX32 "\r\n", page_header.crc32);

        for (int entry = 0; entry < MAX_ENTRY_COUNT; entry++)
        {
            uint8_t bmp = esp32_nvs_state_get(&page_header, entry);
            size_t entry_offset = sizeof(struct ESP32_nvs_page_header) + entry * sizeof(struct ESP32_nvs_item);

            if (bmp == NVS_STATE_WRITTEN)
            {
                TRACE_INFO("    Entry #%" PRIu32 ", offset 0x%08" PRIX32 "\r\n", entry, (uint32_t)entry_offset);

                struct ESP32_nvs_item nvs_item;
                error = file_read_block(file, block_offset + entry_offset, &nvs_item, sizeof(nvs_item));
                if (error != NO_ERROR)
                {
                    TRACE_ERROR("Failed to read input file\r\n");
                    return ERROR_FAILURE;
                }

                error = process_nvs_item(file, offset, sector_offset + entry_offset, &nvs_item, &namespaces);
                if (error == ERROR_BAD_CRC)
                {
                    TRACE_INFO("    CRC updated -> Writing entry\r\n");
                    file_write_block(file, block_offset + entry_offset, &nvs_item, sizeof(nvs_item));
                }
                else if (error != NO_ERROR)
                {
                    return error;
                }

                entry += nvs_item.span - 1;
            }
        }
    }

    return NO_ERROR;
}

error_t esp32_nvs_del(FsFile *file, size_t offset, size_t length, const char *namespace, const char *key)
{
    int ns_index = -1;

    for (size_t sector_offset = 0; sector_offset < length; sector_offset += NVS_SECTOR_SIZE)
    {
        error_t error;
        struct ESP32_nvs_page_header page_header;
        size_t block_offset = offset + sector_offset;

        error = file_read_block(file, block_offset, &page_header, sizeof(page_header));
        if (error != NO_ERROR)
        {
            TRACE_ERROR("Failed to read input file\r\n");
            return ERROR_FAILURE;
        }

        switch (page_header.state)
        {
        case NVS_PAGE_STATE_UNINIT:
            continue;
        case NVS_PAGE_STATE_CORRUPT:
            continue;

        case NVS_PAGE_STATE_ACTIVE:
            break;
        case NVS_PAGE_STATE_FULL:
            break;
        case NVS_PAGE_STATE_FREEING:
            break;
        }

        for (int entry = 0; entry < MAX_ENTRY_COUNT; entry++)
        {
            uint8_t bmp = esp32_nvs_state_get(&page_header, entry);

            /* nothing there, so nothing to delete */
            if (bmp != NVS_STATE_WRITTEN)
            {
                continue;
            }
            size_t entry_offset = sizeof(struct ESP32_nvs_page_header) + entry * sizeof(struct ESP32_nvs_item);

            struct ESP32_nvs_item nvs_item;
            error = file_read_block(file, block_offset + entry_offset, &nvs_item, sizeof(nvs_item));
            if (error != NO_ERROR)
            {
                TRACE_ERROR("Failed to read input file\r\n");
                return ERROR_FAILURE;
            }

            if (nvs_item.nsIndex == 0)
            {
                if (nvs_item.uint8 > MAX_NAMESPACE_COUNT)
                {
                    TRACE_ERROR("namespace index is %" PRIu32 ", which seems invalid", nvs_item.uint8);
                    return ERROR_FAILURE;
                }
                if (!strcmp(namespace, nvs_item.key))
                {
                    ns_index = nvs_item.uint8;
                }
            }
            else
            {
                if (ns_index == nvs_item.nsIndex && !strcmp(nvs_item.key, key))
                {
                    TRACE_INFO("NVS Block #%" PRIu32 ", offset 0x%08" PRIX32 "\r\n", page_header.seq, (uint32_t)block_offset);
                    TRACE_INFO("    Entry #%" PRIu32 ", offset 0x%08" PRIX32 "\r\n", entry, (uint32_t)entry_offset);
                    TRACE_INFO("      Found NVS entry %s.%s, deleting\r\n", namespace, key);

                    /* now clear all item entries */
                    struct ESP32_nvs_item nvs_item_erased;
                    memset(&nvs_item_erased, 0xFF, sizeof(nvs_item_erased));

                    for (int slice = 0; slice < nvs_item.span; slice++)
                    {
                        esp32_nvs_state_set(&page_header, entry + slice, NVS_STATE_EMPTY);
                        file_write_block(file, block_offset + entry_offset + slice * sizeof(struct ESP32_nvs_item), &nvs_item_erased, sizeof(nvs_item_erased));
                    }
                }
            }

            entry += nvs_item.span - 1;
        }

        error = file_write_block(file, block_offset, &page_header, sizeof(page_header));
        if (error != NO_ERROR)
        {
            TRACE_ERROR("Failed to write back file\r\n");
            return ERROR_FAILURE;
        }
    }

    return NO_ERROR;
}

struct ESP32_nvs_item *esp32_nvs_create_uint8(const char *name, uint8_t value)
{
    struct ESP32_nvs_item *item = osAllocMem(sizeof(struct ESP32_nvs_item));
    osMemset(item, 0xFF, sizeof(struct ESP32_nvs_item));

    item[0].datatype = NVS_TYPE_U8;
    item[0].nsIndex = 0;
    item[0].span = 1;
    item[0].chunkIndex = 0xFF;
    item[0].uint8 = value;
    osMemset(item[0].key, 0x00, sizeof(item[0].key));
    osStrcpy(item[0].key, name);

    return item;
}

struct ESP32_nvs_item *esp32_nvs_create_string(const char *name, const char *value)
{
    int span = 2 + osStrlen(value) / sizeof(struct ESP32_nvs_item);
    struct ESP32_nvs_item *item = osAllocMem(sizeof(struct ESP32_nvs_item) * span);
    osMemset(item, 0xFF, sizeof(struct ESP32_nvs_item) * span);

    item[0].datatype = NVS_TYPE_STR;
    item[0].nsIndex = 0;
    item[0].span = span;
    item[0].chunkIndex = 0xFF;
    item[0].uint16 = osStrlen(value);
    osMemset(item[0].key, 0x00, sizeof(item[0].key));
    osStrcpy(item[0].key, name);
    osStrcpy((char *)&item[1], value);

    return item;
}

struct ESP32_nvs_item *esp32_nvs_create_blob(const char *name, const char *value, size_t length)
{
    int span = 2 + length / sizeof(struct ESP32_nvs_item);
    struct ESP32_nvs_item *item = osAllocMem(sizeof(struct ESP32_nvs_item) * (span + 1));
    osMemset(item, 0xFF, sizeof(struct ESP32_nvs_item) * (span + 1));

    item[0].datatype = NVS_TYPE_BLOB;
    item[0].nsIndex = 0;
    item[0].span = span;
    item[0].chunkIndex = 0;
    item[0].var_length.size = length;
    osMemset(item[0].key, 0x00, sizeof(item[0].key));
    osStrcpy(item[0].key, name);
    osMemcpy((char *)&item[1], value, length);

    int idx = 2 + length / sizeof(struct ESP32_nvs_item);
    item[idx].datatype = NVS_TYPE_BLOB_IDX;
    item[idx].nsIndex = 0;
    item[idx].span = 1;
    osMemset(item[idx].key, 0x00, sizeof(item[idx].key));
    osStrcpy(item[idx].key, name);
    item[idx].blob_index.size = length;
    item[idx].blob_index.chunk_count = 1;
    item[idx].blob_index.chunk_start = 0;

    return item;
}

error_t esp32_nvs_add(FsFile *file, size_t offset, size_t length, const char *namespace, struct ESP32_nvs_item *item, int entries)
{
    for (size_t sector_offset = 0; sector_offset < length; sector_offset += NVS_SECTOR_SIZE)
    {
        error_t error;
        struct ESP32_nvs_page_header page_header;
        size_t block_offset = offset + sector_offset;

        error = file_read_block(file, block_offset, &page_header, sizeof(page_header));
        if (error != NO_ERROR)
        {
            TRACE_ERROR("Failed to read input file\r\n");
            return ERROR_FAILURE;
        }

        switch (page_header.state)
        {
        case NVS_PAGE_STATE_UNINIT:
            continue;
        case NVS_PAGE_STATE_CORRUPT:
            continue;

        case NVS_PAGE_STATE_ACTIVE:
            break;
        case NVS_PAGE_STATE_FULL:
            break;
        case NVS_PAGE_STATE_FREEING:
            break;
        }

        for (int entry = 0; entry < MAX_ENTRY_COUNT && (entry + item->span - 1 < MAX_ENTRY_COUNT); entry++)
        {
            size_t entry_offset = sizeof(struct ESP32_nvs_page_header) + entry * sizeof(struct ESP32_nvs_item);
            uint8_t bmp = esp32_nvs_state_get(&page_header, entry);

            /* parse namespace entries */
            if (bmp == NVS_STATE_WRITTEN)
            {
                struct ESP32_nvs_item nvs_item;
                error = file_read_block(file, block_offset + entry_offset, &nvs_item, sizeof(nvs_item));
                if (error != NO_ERROR)
                {
                    TRACE_ERROR("Failed to read input file\r\n");
                    return ERROR_FAILURE;
                }

                if (nvs_item.nsIndex == 0 && !strcmp(namespace, nvs_item.key))
                {
                    TRACE_INFO("Namespace %s found: %u\r\n", namespace, nvs_item.uint8);

                    /* set namespace for all headers in the passed block */
                    for (int slice = 0; slice < entries; slice += item[slice].span)
                    {
                        item[slice].nsIndex = nvs_item.uint8;
                    }
                }
                continue;
            }

            bool candidate = true;
            for (int slice = 0; slice < entries; slice++)
            {
                if (esp32_nvs_state_get(&page_header, entry + slice) == NVS_STATE_WRITTEN)
                {
                    candidate = false;
                }
            }

            if (!candidate || item->nsIndex == 0)
            {
                continue;
            }
            TRACE_INFO("NVS Block #%" PRIu32 ", offset 0x%08" PRIX32 "\r\n", page_header.seq, (uint32_t)block_offset);
            TRACE_INFO("    Entry #%" PRIu32 ", offset 0x%08" PRIX32 "\r\n", entry, (uint32_t)entry_offset);
            TRACE_INFO("      %i empty slots found, writing there\r\n", entries);

            error = file_write_block(file, block_offset + entry_offset, item, sizeof(struct ESP32_nvs_item) * entries);
            if (error != NO_ERROR)
            {
                TRACE_ERROR("Failed to write file\r\n");
                return ERROR_FAILURE;
            }

            for (int slice = 0; slice < entries; slice++)
            {
                esp32_nvs_state_set(&page_header, entry + slice, NVS_STATE_WRITTEN);
            }

            error = file_write_block(file, block_offset, &page_header, sizeof(page_header));
            if (error != NO_ERROR)
            {
                TRACE_ERROR("Failed to write back file\r\n");
                return ERROR_FAILURE;
            }
            return NO_ERROR;
        }
    }

    return NO_ERROR;
}

error_t esp32_fixup_fatfs(FsFile *file, size_t offset, size_t length, bool modify)
{
    if (esp32_wl_init(&esp32_wl_state, file, offset, length) != NO_ERROR)
    {
        TRACE_ERROR("Failed to init wear leveling\r\n");
        return ERROR_FAILURE;
    }

    FATFS fs;
    FRESULT ret = f_mount(&fs, "0:", 1);

    if (ret == FR_OK)
    {
        DIR dirInfo;
        FRESULT res = f_opendir(&dirInfo, "\\CERT\\");

        if (res == FR_OK)
        {
            TRACE_INFO("Index of CERT\r\n");
            do
            {
                FILINFO fileInfo;
                if (f_readdir(&dirInfo, &fileInfo) != FR_OK)
                {
                    break;
                }
                if (!fileInfo.fname[0])
                {
                    break;
                }
                TRACE_INFO("  %-12s %-10" PRIu32 " %04d-%02d-%02d %02d:%02d:%02d\r\n", fileInfo.fname, fileInfo.fsize,
                           ((fileInfo.fdate >> 9) & 0x7F) + 1980,
                           (fileInfo.fdate >> 5) & 0x0F,
                           (fileInfo.fdate) & 0x1F,
                           (fileInfo.ftime >> 11) & 0x1F,
                           (fileInfo.ftime >> 5) & 0x3F,
                           (fileInfo.ftime & 0x1F) * 2);
            } while (1);

            f_closedir(&dirInfo);
        }

        f_unmount("0:");
    }

    return NO_ERROR;
}

error_t esp32_fat_extract_folder(FsFile *file, size_t offset, size_t length, const char *path, const char *out_path)
{
    if (esp32_wl_init(&esp32_wl_state, file, offset, length) != NO_ERROR)
    {
        TRACE_ERROR("Failed to init wear leveling\r\n");
        return ERROR_FAILURE;
    }

    FATFS fs;
    FRESULT ret = f_mount(&fs, "0:", 1);

    if (ret != FR_OK)
    {
        TRACE_ERROR("Failed to mount image\r\n");
        return ERROR_FAILURE;
    }
    DIR dirInfo;
    FRESULT res = f_opendir(&dirInfo, path);

    if (res == FR_OK)
    {
        do
        {
            FILINFO fileInfo;
            if (f_readdir(&dirInfo, &fileInfo) != FR_OK)
            {
                break;
            }
            if (!fileInfo.fname[0])
            {
                break;
            }
            char fatFileName[FS_MAX_PATH_LEN];
            osStrcpy(fatFileName, path);
            osStrcat(fatFileName, "\\");
            osStrcat(fatFileName, fileInfo.fname);

            char outFileName[FS_MAX_PATH_LEN];
            osStrcpy(outFileName, out_path);

            pathAddSlash(outFileName, FS_MAX_PATH_LEN);
            pathCombine(outFileName, fileInfo.fname, FS_MAX_PATH_LEN);
            pathCanonicalize(outFileName);

            TRACE_INFO("Write '%s to '%s' (%" PRIu32 " bytes)\r\n", fatFileName, outFileName, fileInfo.fsize);

            FsFile *outFile = fsOpenFile(outFileName, FS_FILE_MODE_WRITE);
            if (!outFile)
            {
                TRACE_ERROR("Failed to open output file\r\n");
                return ERROR_FAILURE;
            }

            FIL fp;
            if (f_open(&fp, fatFileName, FA_READ) != FR_OK)
            {
                TRACE_ERROR("Failed to open FAT file\r\n");
                return ERROR_FAILURE;
            }
            uint8_t buffer[512];
            for (int pos = 0; pos < fileInfo.fsize; pos += sizeof(buffer))
            {
                uint32_t read;
                if (f_read(&fp, buffer, sizeof(buffer), &read) != FR_OK)
                {
                    TRACE_ERROR("Failed to read from FAT file\r\n");
                    return ERROR_FAILURE;
                }

                if (fsWriteFile(outFile, buffer, read) != NO_ERROR)
                {
                    TRACE_ERROR("Failed to write output file\r\n");
                    return ERROR_FAILURE;
                }
            }
            f_close(&fp);
            fsCloseFile(outFile);
        } while (1);

        f_closedir(&dirInfo);
    }

    f_unmount("0:");

    return NO_ERROR;
}

error_t esp32_fat_inject_folder(FsFile *file, size_t offset, size_t length, const char *path, const char *in_path)
{
    if (esp32_wl_init(&esp32_wl_state, file, offset, length) != NO_ERROR)
    {
        TRACE_ERROR("Failed to init wear leveling\r\n");
        return ERROR_FAILURE;
    }

    FATFS fs;
    FRESULT ret = f_mount(&fs, "0:", 1);

    if (ret != FR_OK)
    {
        TRACE_ERROR("Failed to mount image\r\n");
        return ERROR_FAILURE;
    }

    FsDir *dir = fsOpenDir(in_path);
    if (!dir)
    {
        TRACE_ERROR("Failed to open source directory\r\n");
        return ERROR_FAILURE;
    }

    do
    {
        FsDirEntry dirEntry;

        if (fsReadDir(dir, &dirEntry) != NO_ERROR)
        {
            break;
        }

        if (dirEntry.attributes & FS_FILE_ATTR_DIRECTORY)
        {
            continue;
        }

        char fatFileName[FS_MAX_PATH_LEN];
        osStrcpy(fatFileName, path);
        osStrcat(fatFileName, "\\");
        osStrcat(fatFileName, dirEntry.name);

        char inFileName[FS_MAX_PATH_LEN];
        osStrcpy(inFileName, in_path);

        pathAddSlash(inFileName, FS_MAX_PATH_LEN);
        pathCombine(inFileName, dirEntry.name, FS_MAX_PATH_LEN);
        pathCanonicalize(inFileName);

        TRACE_INFO("Write '%s to '%s'\r\n", inFileName, fatFileName);

        FsFile *inFile = fsOpenFile(inFileName, FS_FILE_MODE_READ);
        if (!inFile)
        {
            TRACE_ERROR("Failed to open output file\r\n");
            return ERROR_FAILURE;
        }

        // f_unlink(fatFileName);

        FIL fp;
        if (f_open(&fp, fatFileName, FA_WRITE | FA_CREATE_ALWAYS) != FR_OK)
        {
            TRACE_ERROR("Failed to open FAT file\r\n");
            return ERROR_FAILURE;
        }

        uint8_t buffer[512];
        uint32_t written_total = 0;
        for (int pos = 0; pos < dirEntry.size; pos += sizeof(buffer))
        {
            size_t read;
            if (fsReadFile(inFile, buffer, sizeof(buffer), &read) != NO_ERROR)
            {
                TRACE_ERROR("Failed to read from input file\r\n");
                return ERROR_FAILURE;
            }
            TRACE_INFO("  Read %zu byte\r\n", read);

            uint32_t written;
            if (f_write(&fp, buffer, read, &written) != FR_OK || read != written)
            {
                TRACE_ERROR("Failed to write to FAT file\r\n");
                return ERROR_FAILURE;
            }

            written_total += written;
        }
        TRACE_INFO("  Wrote %u byte\r\n", written_total);
        f_sync(&fp);
        f_close(&fp);
        fsCloseFile(inFile);

    } while (1);

    fsCloseDir(dir);

    f_unmount("0:");

    return NO_ERROR;
}

error_t esp32_fixup_partitions(FsFile *file, size_t offset, bool modify)
{
    size_t offset_current = offset;
    struct ESP32_part_entry entry = {0};
    int num = 0;
    error_t error = 0;

    while (true)
    {
        fsSeekFile(file, offset_current, FS_SEEK_SET);

        size_t read;
        error = fsReadFile(file, &entry, sizeof(entry), &read);

        if (read != sizeof(entry))
        {
            TRACE_ERROR("Failed to read entry\r\n");
            return ERROR_FAILURE;
        }

        if (entry.magic == 0x50AA)
        {
            TRACE_INFO("#%d  Type: %d, SubType: %02X, Offset: 0x%06X, Length: 0x%06X, Label: '%s'\r\n", num, entry.partType, entry.partSubType, entry.fileOffset, entry.length, entry.label);

            if (entry.partType == ESP_PARTITION_TYPE_APP)
            {
                esp32_fixup_image(file, entry.fileOffset, entry.length, modify);
            }
            if (entry.partType == ESP_PARTITION_TYPE_DATA && entry.partSubType == 0x81)
            {
                esp32_fixup_fatfs(file, entry.fileOffset, entry.length, modify);
            }
            if (entry.partType == ESP_PARTITION_TYPE_DATA && entry.partSubType == 0x02)
            {
                esp32_fixup_nvs(file, entry.fileOffset, entry.length, modify);
            }
        }
        else
        {
            break;
        }

        offset_current += sizeof(entry);
        num++;
    }

    return error;
}

error_t esp32_update_wifi_partitions(FsFile *file, size_t offset, const char *ssid, const char *password)
{
    size_t offset_current = offset;
    struct ESP32_part_entry entry = {0};
    int num = 0;
    error_t error = NO_ERROR;

    while (true)
    {
        size_t read = 0;

        fsSeekFile(file, offset_current, FS_SEEK_SET);
        error = fsReadFile(file, &entry, sizeof(entry), &read);

        if (read != sizeof(entry))
        {
            TRACE_ERROR("Failed to read entry\r\n");
            return ERROR_FAILURE;
        }

        if (entry.magic == 0x50AA)
        {
            TRACE_INFO("#%d  Type: %d, SubType: %02X, Offset: 0x%06X, Length: 0x%06X, Label: '%s'\r\n", num, entry.partType, entry.partSubType, entry.fileOffset, entry.length, entry.label);

            if (entry.partType == ESP_PARTITION_TYPE_DATA && entry.partSubType == 0x02)
            {
                esp32_nvs_del(file, entry.fileOffset, entry.length, "TB_WIFI", "SSID0");
                esp32_nvs_del(file, entry.fileOffset, entry.length, "TB_WIFI", "PW0");
                esp32_nvs_del(file, entry.fileOffset, entry.length, "TB_WIFI", "INDEX");

                struct ESP32_nvs_item *ssid_item = esp32_nvs_create_blob("SSID0", ssid, osStrlen(ssid));
                struct ESP32_nvs_item *pass_item = esp32_nvs_create_blob("PW0", password, osStrlen(password));
                struct ESP32_nvs_item *index_item = esp32_nvs_create_uint8("INDEX", 1);

                esp32_nvs_add(file, entry.fileOffset, entry.length, "TB_WIFI", ssid_item, ssid_item->span + 1);
                esp32_nvs_add(file, entry.fileOffset, entry.length, "TB_WIFI", pass_item, pass_item->span + 1);
                esp32_nvs_add(file, entry.fileOffset, entry.length, "TB_WIFI", index_item, index_item->span);

                osFreeMem(ssid_item);
                osFreeMem(pass_item);
                osFreeMem(index_item);
            }
        }
        else
        {
            break;
        }

        offset_current += sizeof(entry);
        num++;
    }

    return error;
}

error_t esp32_patch_wifi(const char *path, const char *ssid, const char *pass)
{
    uint32_t length = 0;

    TRACE_INFO("Patching wifi settings in '%s'\r\n", path);

    error_t error = fsGetFileSize(path, &length);

    if (error || length < 0x9000)
    {
        TRACE_ERROR("File does not exist or is too small '%s'\r\n", path);
        return ERROR_NOT_FOUND;
    }

    FsFile *file = fsOpenFileEx(path, "rb+");

    esp32_update_wifi_partitions(file, 0x9000, ssid, pass);

    fsCloseFile(file);

    return NO_ERROR;
}

error_t esp32_get_partition(FsFile *file, size_t offset, const char *label, size_t *part_start, size_t *part_size)
{
    size_t offset_current = offset;
    struct ESP32_part_entry entry = {0};
    int num = 0;
    TRACE_INFO("Search for partition '%s'\r\n", label);

    while (true)
    {
        size_t read = 0;

        fsSeekFile(file, offset_current, FS_SEEK_SET);
        error_t error = fsReadFile(file, &entry, sizeof(entry), &read);

        if (error != NO_ERROR || read != sizeof(entry))
        {
            TRACE_ERROR("Failed to read entry\r\n");
            return ERROR_FAILURE;
        }

        if (entry.magic == 0x50AA)
        {
            if (!osStrcmp((const char *)entry.label, label))
            {
                TRACE_INFO("Found partition '%s' at 0x%06" PRIX32 "\r\n", label, entry.fileOffset);
                *part_start = entry.fileOffset;
                *part_size = entry.length;
                return NO_ERROR;
            }
        }
        else
        {
            break;
        }

        offset_current += sizeof(entry);
        num++;
    }

    TRACE_ERROR("Partition '%s' not found\r\n", label);
    return ERROR_FAILURE;
}

void esp32_chk_update(uint8_t *chk, void *buffer, size_t length)
{
    for (size_t pos = 0; pos < length; pos++)
    {
        *chk ^= ((uint8_t *)buffer)[pos];
    }
}

error_t esp32_fixup_image(FsFile *file, size_t offset, size_t length, bool modify)
{
    size_t offset_current = offset;
    struct ESP32_header header = {0};

    fsSeekFile(file, offset_current, FS_SEEK_SET);

    uint8_t chk_calc = 0xEF;
    Sha256Context ctx;
    sha256Init(&ctx);

    size_t read = 0;
    error_t error = fsReadFile(file, &header, sizeof(header), &read);

    if (error != NO_ERROR || read != sizeof(header))
    {
        TRACE_ERROR("Failed to read header\r\n");
        return ERROR_FAILURE;
    }

    sha256Update(&ctx, &header, sizeof(header));
    offset_current += sizeof(header);

    if (header.magic != 0xE9)
    {
        TRACE_INFO("No image found\r\n");
        return ERROR_FAILURE;
    }

    for (int seg = 0; seg < header.segments; seg++)
    {
        struct ESP32_segment segment = {0};

        fsSeekFile(file, offset_current, FS_SEEK_SET);
        error = fsReadFile(file, &segment, sizeof(segment), &read);

        if (error != NO_ERROR || read != sizeof(segment))
        {
            TRACE_ERROR("Failed to read segment\r\n");
            return ERROR_FAILURE;
        }
        TRACE_INFO("#%d Address: 0x%08X, Len: 0x%06X, Offset: 0x%06X\r\n", seg, segment.loadAddress, segment.length, (uint32_t)offset_current);

        sha256Update(&ctx, &segment, sizeof(segment));
        offset_current += sizeof(segment);

        for (size_t pos = 0; pos < segment.length;)
        {
            uint8_t buffer[512];
            size_t maxLen = sizeof(buffer);

            if (maxLen > segment.length - pos)
            {
                maxLen = segment.length - pos;
            }

            fsSeekFile(file, offset_current, FS_SEEK_SET);
            error = fsReadFile(file, buffer, maxLen, &read);
            if (error != NO_ERROR || read != maxLen)
            {
                TRACE_ERROR("Failed to read data\r\n");
                return ERROR_FAILURE;
            }
            sha256Update(&ctx, buffer, maxLen);
            esp32_chk_update(&chk_calc, buffer, maxLen);
            offset_current += maxLen;
            pos += maxLen;
        }
    }
    TRACE_INFO(" Offset: 0x%06X\r\n", (uint32_t)offset_current);

    while ((offset_current & 0x0F) != 0x0F)
    {
        uint8_t null = 0;
        sha256Update(&ctx, &null, sizeof(null));
        offset_current++;
    }
    fsSeekFile(file, offset_current, FS_SEEK_SET);

    uint8_t chk;
    error = fsReadFile(file, &chk, sizeof(chk), &read);

    if (error != NO_ERROR || read != sizeof(chk))
    {
        TRACE_ERROR("Failed to read chk\r\n");
        return ERROR_FAILURE;
    }
    TRACE_INFO("CHK: 0x%02X\r\n", chk);
    TRACE_INFO("CHK: 0x%02X (calculated)\r\n", chk_calc);

    if (modify && chk != chk_calc)
    {
        TRACE_INFO("Fix checksum\r\n");
        fsSeekFile(file, offset_current, FS_SEEK_SET);
        error = fsWriteFile(file, &chk_calc, sizeof(chk_calc));
        if (error != NO_ERROR)
        {
            TRACE_ERROR("Failed to write chk\r\n");
            return ERROR_FAILURE;
        }
    }

    offset_current += 1;

    sha256Update(&ctx, &chk_calc, sizeof(chk_calc));

    uint8_t sha256_calc[32];
    sha256Final(&ctx, sha256_calc);
    if (header.extended.has_hash)
    {
        uint8_t sha256[32];
        error = fsReadFile(file, &sha256, sizeof(sha256), &read);

        if (error != NO_ERROR || read != sizeof(sha256))
        {
            TRACE_ERROR("Failed to read sha256\r\n");
            return ERROR_FAILURE;
        }
        char buf[sizeof(sha256) * 2 + 1];
        for (int pos = 0; pos < sizeof(sha256); pos++)
        {
            osSprintf(&buf[pos * 2], "%02X", sha256[pos]);
        }
        TRACE_INFO("SHA1: %s\r\n", buf);

        for (int pos = 0; pos < sizeof(sha256_calc); pos++)
        {
            osSprintf(&buf[pos * 2], "%02X", sha256_calc[pos]);
        }
        TRACE_INFO("SHA1: %s (calculated)\r\n", buf);

        if (modify && osMemcmp(sha256_calc, sha256, sizeof(sha256_calc)))
        {
            TRACE_INFO("Fix SHA1\r\n");
            fsSeekFile(file, offset_current, FS_SEEK_SET);
            error = fsWriteFile(file, &sha256_calc, sizeof(sha256_calc));
            if (error != NO_ERROR)
            {
                TRACE_ERROR("Failed to write SHA1\r\n");
                return ERROR_FAILURE;
            }
        }
    }
    return error;
}

error_t esp32_fixup(const char *path, bool modify)
{
    uint32_t length = 0;
    error_t error = fsGetFileSize(path, &length);

    if (error || length < 0x9000)
    {
        TRACE_ERROR("File does not exist or is too small '%s'\r\n", path);
        return ERROR_NOT_FOUND;
    }

    FsFile *file = fsOpenFileEx(path, "rb+");

    esp32_fixup_image(file, 0, length, modify);
    esp32_fixup_partitions(file, 0x9000, modify);

    return NO_ERROR;
}

error_t esp32_fat_extract(const char *firmware, const char *fat_path, const char *out_path)
{
    uint32_t length = 0;
    error_t error = fsGetFileSize(firmware, &length);

    if (error || length < 0x9000)
    {
        TRACE_ERROR("File does not exist or is too small '%s'\r\n", firmware);
        return ERROR_NOT_FOUND;
    }

    FsFile *file = fsOpenFileEx(firmware, "rb+");

    size_t part_offset = 0;
    size_t part_size = 0;
    error = esp32_get_partition(file, 0x9000, "assets", &part_offset, &part_size);
    if (error != NO_ERROR)
    {
        TRACE_ERROR("Asset partition not found\r\n");
        return ERROR_NOT_FOUND;
    }

    esp32_fat_extract_folder(file, part_offset, part_size, fat_path, out_path);
    fsCloseFile(file);

    return NO_ERROR;
}

error_t esp32_fat_inject(const char *firmware, const char *fat_path, const char *in_path)
{
    uint32_t length = 0;
    error_t error = fsGetFileSize(firmware, &length);

    if (error || length < 0x9000)
    {
        TRACE_ERROR("File does not exist or is too small '%s'\r\n", firmware);
        return ERROR_NOT_FOUND;
    }

    FsFile *file = fsOpenFileEx(firmware, "rb+");

    size_t part_offset = 0;
    size_t part_size = 0;
    error = esp32_get_partition(file, 0x9000, "assets", &part_offset, &part_size);
    if (error != NO_ERROR)
    {
        TRACE_ERROR("Asset partition not found\r\n");
        return ERROR_NOT_FOUND;
    }

    esp32_fat_inject_folder(file, part_offset, part_size, fat_path, in_path);

    fsCloseFile(file);

    return NO_ERROR;
}

/**
 * @brief Replaces all occurrences of a pattern string with a replacement string in a given buffer.
 *
 * This function searches the buffer for the C-string 'pattern' and replaces all its occurrences with the
 * C-string 'replace'. It doesn't check if the sizes of the two strings differ and overwrites the original
 * string beyond its end if needed. It makes sure not to read or write beyond the buffer end.
 *
 * @param buffer The buffer in which to replace occurrences of 'pattern'.
 * @param buffer_len The length of the buffer.
 * @param pattern The pattern string to search for.
 * @param replace The replacement string.
 * @return The number of replacements made.
 */
uint32_t mem_replace(uint8_t *buffer, size_t buffer_len, const char *pattern, const char *replace)
{
    int replaced = 0;
    size_t pattern_len = osStrlen(pattern) + 1;
    size_t replace_len = osStrlen(replace) + 1;

    if (pattern_len == 0 || buffer_len == 0)
    {
        return 0;
    }

    for (size_t i = 0; i <= buffer_len - pattern_len; ++i)
    {
        if (memcmp(&buffer[i], pattern, pattern_len) == 0)
        {
            // Make sure we don't write beyond buffer end
            size_t end_index = i + replace_len;
            if (end_index > buffer_len)
            {
                break;
            }

            osMemcpy(&buffer[i], replace, replace_len);
            i += (pattern_len - 1); // Skip ahead
            replaced++;
        }
    }

    return replaced;
}

error_t esp32_inject_cert(const char *rootPath, const char *patchedPath, const char *mac)
{
    error_t ret = NO_ERROR;
    char *cert_path = custom_asprintf("%s/cert_%s/", rootPath, mac);
    TRACE_INFO("Generating certs for MAC %s into '%s'\r\n", mac, cert_path);

    do
    {
        if (!fsDirExists(cert_path))
        {
            if (fsCreateDir(cert_path) != NO_ERROR)
            {
                TRACE_ERROR("Failed to create '%s'\r\n", cert_path);
                ret = ERROR_NOT_FOUND;
                break;
            }
        }

        if (cert_generate_mac(mac, cert_path) != NO_ERROR)
        {
            TRACE_ERROR("Failed to generate certificate\r\n");
            ret = ERROR_NOT_FOUND;
            break;
        }

        TRACE_INFO("Injecting certs into '%s'\r\n", patchedPath);
        if (esp32_fat_inject(patchedPath, "CERT", cert_path) != NO_ERROR)
        {
            TRACE_ERROR("Failed to patch image\r\n");
            ret = ERROR_NOT_FOUND;
            break;
        }
    } while (0);

    osFreeMem(cert_path);

    return ret;
}

error_t esp32_inject_ca(const char *rootPath, const char *patchedPath, const char *mac)
{
    error_t ret = NO_ERROR;
    char *cert_path = custom_asprintf("%s/cert_%s/", rootPath, mac);
    char *ca_file = custom_asprintf("%s/ca.der", cert_path);

    TRACE_INFO("Writing CA into '%s'\r\n", ca_file);

    do
    {
        if (!fsDirExists(cert_path))
        {
            if (fsCreateDir(cert_path) != NO_ERROR)
            {
                TRACE_ERROR("Failed to create '%s'\r\n", cert_path);
                ret = ERROR_NOT_FOUND;
                break;
            }
        }

        const char *server_ca = settings_get_string("internal.server.ca");
        size_t server_ca_der_size = 0;

        TRACE_INFO("Convert CA certificate...\r\n");
        if (pemImportCertificate(server_ca, osStrlen(server_ca), NULL, &server_ca_der_size, NULL) != NO_ERROR)
        {
            TRACE_ERROR("pemImportCertificate failed\r\n");
            return ERROR_FAILURE;
        }
        uint8_t *server_ca_der = osAllocMem(server_ca_der_size);
        if (pemImportCertificate(server_ca, osStrlen(server_ca), server_ca_der, &server_ca_der_size, NULL) != NO_ERROR)
        {
            TRACE_ERROR("pemImportCertificate failed\r\n");
            return ERROR_FAILURE;
        }

        if (fsFileExists(ca_file))
        {
            if (fsDeleteFile(ca_file) != NO_ERROR)
            {
                TRACE_ERROR("Failed to delete pre-existing '%s'\r\n", cert_path);
                ret = ERROR_NOT_FOUND;
                break;
            }
        }

        FsFile *ca = fsOpenFile(ca_file, FS_FILE_MODE_WRITE);
        if (!ca)
        {
            TRACE_ERROR("Failed to create CA file\r\n");
            ret = ERROR_NOT_FOUND;
            break;
        }
        if (fsWriteFile(ca, server_ca_der, server_ca_der_size) != NO_ERROR)
        {
            TRACE_ERROR("Failed to write CA file\r\n");
            ret = ERROR_NOT_FOUND;
            break;
        }
        fsCloseFile(ca);

        TRACE_INFO("Injecting CA into '%s'\r\n", patchedPath);
        if (esp32_fat_inject(patchedPath, "CERT", cert_path) != NO_ERROR)
        {
            TRACE_ERROR("Failed to patch image\r\n");
            ret = ERROR_NOT_FOUND;
            break;
        }
        if (fsDeleteFile(ca_file) != NO_ERROR)
        {
            TRACE_ERROR("Failed to delete during clean-up '%s'\r\n", cert_path);
            ret = ERROR_NOT_FOUND;
            break;
        }
        if (fsRemoveDir(cert_path) != NO_ERROR)
        {
            TRACE_ERROR("Failed to delete directory during clean-up '%s'\r\n", cert_path);
            ret = ERROR_NOT_FOUND;
            break;
        }
    } while (0);

    osFreeMem(cert_path);
    osFreeMem(ca_file);

    return ret;
}

error_t esp32_patch_host(const char *patchedPath, const char *hostname, const char *oldrtnl, const char *oldapi)
{
    error_t ret = NO_ERROR;

    TRACE_INFO("Patching hostnames in '%s'\r\n", patchedPath);

    uint8_t *bin_data = NULL;
    do
    {
        uint32_t bin_size = 0;
        size_t total = 0;

        if (fsGetFileSize(patchedPath, &bin_size))
        {
            TRACE_ERROR("File does not exist '%s'\r\n", patchedPath);
            return ERROR_NOT_FOUND;
        }

        bin_data = osAllocMem(bin_size);

        FsFile *bin = fsOpenFile(patchedPath, FS_FILE_MODE_READ);
        if (!bin)
        {
            TRACE_ERROR("Failed to open firmware\r\n");
            ret = ERROR_NOT_FOUND;
            break;
        }

        while (total < bin_size)
        {
            size_t read = 0;
            if (fsReadFile(bin, &bin_data[total], bin_size - total, &read) != NO_ERROR)
            {
                TRACE_ERROR("Failed to read firmware\r\n");
                ret = ERROR_NOT_FOUND;
                break;
            }

            if (read > 0)
            {
                total += read;
            }
        }
        fsCloseFile(bin);

        int replaced = 0;
        if (!strcmp(oldrtnl, oldapi))
        {
            replaced = mem_replace(bin_data, bin_size, oldrtnl, hostname);
            TRACE_INFO(" replaced hostname %d times\r\n", replaced);
        }
        else
        {
            int replacedRtnl = mem_replace(bin_data, bin_size, oldrtnl, hostname);
            TRACE_INFO(" replaced RTNL host %d times\r\n", replacedRtnl);
            int replacedAPI = mem_replace(bin_data, bin_size, oldapi, hostname);
            TRACE_INFO(" replaced API host %d times\r\n", replacedAPI);
            replaced = replacedRtnl + replacedAPI;
        }

        if (replaced > 0)
        {
            bin = fsOpenFile(patchedPath, FS_FILE_MODE_WRITE);
            if (!bin)
            {
                TRACE_ERROR("Failed to open firmware for writing\r\n");
                ret = ERROR_NOT_FOUND;
                break;
            }
            if (fsWriteFile(bin, bin_data, bin_size) != NO_ERROR)
            {
                TRACE_ERROR("Failed to write firmware\r\n");
                ret = ERROR_NOT_FOUND;
                break;
            }
            fsCloseFile(bin);
        }
        else
        {
            TRACE_WARNING("No replacements made, file untouched\r\n");
        }

    } while (0);

    if (bin_data != NULL)
    {
        osFreeMem(bin_data);
    }
    return ret;
}
