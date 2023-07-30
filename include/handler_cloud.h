#ifndef _HANDLER_CLOUD_H
#define _HANDLER_CLOUD_H

#include "debug.h"

#include "core/net.h"
#include "core/ethernet.h"
#include "core/ip.h"
#include "core/tcp.h"
#include "http/http_server.h"
#include "http/http_server_misc.h"

#include "handler.h"
#include "proto/toniebox.pb.taf-header.pb-c.h"

#define BODY_BUFFER_SIZE 4096
#define TAF_HEADER_SIZE 4092

typedef struct
{
    char *contentPath;
    bool_t exists;
    bool_t valid;
    bool_t nocloud;
    bool_t live;
    bool_t updated;
    TonieboxAudioFileHeader *tafHeader;
} tonie_info_t;

#define PROX_STATUS_IDLE 0
#define PROX_STATUS_CONN 1
#define PROX_STATUS_HEAD 2
#define PROX_STATUS_BODY 3
#define PROX_STATUS_DONE 4

typedef enum
{
    NONE = 0,
    V1_TIME,
    V1_OTA,
    V1_CLAIM,
    V2_CONTENT,
    V1_FRESHNESS_CHECK,
    V1_LOG,
    V1_CLOUDRESET
} cloudapi_t;

typedef enum
{
    OTA_FIRMWARE_PD = 2,
    OTA_FIRMWARE_EU = 3,
    OTA_SERVICEPACK_CC3200 = 4,
    OTA_HTML_CONFIG = 5,
    OTA_SFX_BIN = 6,
} cloudapi_ota_t;

typedef struct
{
    const char_t *uri;
    const char_t *queryString;
    cloudapi_t api;
    char_t *buffer;
    size_t bufferPos;
    size_t bufferLen;
    uint32_t status;
    FsFile *file;
    tonie_info_t tonieInfo;
    HttpConnection *connection;
    client_ctx_t *client_ctx;
} cbr_ctx_t;

void getContentPathFromCharRUID(char ruid[17], char **pcontentPath, settings_t *settings);
void getContentPathFromUID(uint64_t uid, char **pcontentPath, settings_t *settings);
tonie_info_t getTonieInfo(const char *contentPath);
void freeTonieInfo(tonie_info_t *tonieInfo);

void httpPrepareHeader(HttpConnection *connection, const void *contentType, size_t contentLength);

error_t handleCloudTime(HttpConnection *connection, const char_t *uri, const char_t *queryString, client_ctx_t *client_ctx);
error_t handleCloudOTA(HttpConnection *connection, const char_t *uri, const char_t *queryString, client_ctx_t *client_ctx);
error_t handleCloudLog(HttpConnection *connection, const char_t *uri, const char_t *queryString, client_ctx_t *client_ctx);
error_t handleCloudClaim(HttpConnection *connection, const char_t *uri, const char_t *queryString, client_ctx_t *client_ctx);
error_t handleCloudContent(HttpConnection *connection, const char_t *uri, const char_t *queryString, client_ctx_t *client_ctx, bool_t noPassword);
error_t handleCloudContentV1(HttpConnection *connection, const char_t *uri, const char_t *queryString, client_ctx_t *client_ctx);
error_t handleCloudContentV2(HttpConnection *connection, const char_t *uri, const char_t *queryString, client_ctx_t *client_ctx);
error_t handleCloudFreshnessCheck(HttpConnection *connection, const char_t *uri, const char_t *queryString, client_ctx_t *client_ctx);
error_t handleCloudReset(HttpConnection *connection, const char_t *uri, const char_t *queryString, client_ctx_t *client_ctx);

#endif