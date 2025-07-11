/*
  +----------------------------------------------------------------------+
  | Swoole                                                               |
  +----------------------------------------------------------------------+
  | This source file is subject to version 2.0 of the Apache license,    |
  | that is bundled with this package in the file LICENSE, and is        |
  | available through the world-wide-web at the following url:           |
  | http://www.apache.org/licenses/LICENSE-2.0.html                      |
  | If you did not receive a copy of the Apache2.0 license and are unable|
  | to obtain it through the world-wide-web, please send a note to       |
  | license@swoole.com so we can mail you a copy immediately.            |
  +----------------------------------------------------------------------+
  | Author: Tianfeng Han  <rango@swoole.com>                             |
  +----------------------------------------------------------------------+
*/

#include "php_swoole_http_server.h"
#include "swoole_util.h"

BEGIN_EXTERN_C()
#include "stubs/php_swoole_http_response_arginfo.h"
END_EXTERN_C()

using swoole::Connection;
using swoole::Server;
using swoole::String;
using swoole::substr_len;
using swoole::coroutine::Socket;

using HttpResponse = swoole::http::Response;
using HttpContext = swoole::http::Context;
using HttpCookie = swoole::http::Cookie;

namespace WebSocket = swoole::websocket;
namespace HttpServer = swoole::http_server;

zend_class_entry *swoole_http_response_ce;
static zend_object_handlers swoole_http_response_handlers;

static inline void http_header_key_format(char *key, int length) {
    int i, state = 0;
    for (i = 0; i < length; i++) {
        if (state == 0) {
            if (key[i] >= 97 && key[i] <= 122) {
                key[i] -= 32;
            }
            state = 1;
        } else if (key[i] == '-') {
            state = 0;
        } else {
            if (key[i] >= 65 && key[i] <= 90) {
                key[i] += 32;
            }
        }
    }
}

String *HttpContext::get_write_buffer() {
    if (co_socket) {
        return ((Socket *) private_data)->get_write_buffer();
    } else {
        if (!write_buffer) {
            write_buffer = new String(SW_BUFFER_SIZE_STD, sw_php_allocator());
        }
        return write_buffer;
    }
}

struct HttpResponseObject {
    HttpContext *ctx;
    zend_object std;
};

static sw_inline HttpResponseObject *php_swoole_http_response_fetch_object(zend_object *obj) {
    return (HttpResponseObject *) ((char *) obj - swoole_http_response_handlers.offset);
}

HttpContext *php_swoole_http_response_get_context(zval *zobject) {
    return php_swoole_http_response_fetch_object(Z_OBJ_P(zobject))->ctx;
}

void php_swoole_http_response_set_context(zval *zobject, HttpContext *ctx) {
    php_swoole_http_response_fetch_object(Z_OBJ_P(zobject))->ctx = ctx;
}

static void php_swoole_http_response_free_object(zend_object *object) {
    HttpResponseObject *response = php_swoole_http_response_fetch_object(object);
    HttpContext *ctx = response->ctx;
    zval ztmp; /* bool, not required to release it */

    if (ctx) {
        if (ctx->onAfterResponse) {
            ctx->onAfterResponse(ctx);
        }
        if (!ctx->end_ && (ctx->send_chunked || !ctx->send_header_) && !ctx->detached && sw_reactor()) {
            if (ctx->response.status == 0) {
                ctx->response.status = SW_HTTP_INTERNAL_SERVER_ERROR;
            }
            if (ctx->http2) {
                if (ctx->stream) {
                    ctx->http2_end(nullptr, &ztmp);
                }
            } else {
                if (ctx->is_available()) {
                    ctx->end(nullptr, &ztmp);
                }
            }
        }
        ctx->response.zobject = nullptr;
        ctx->free();
    }

    zend_object_std_dtor(&response->std);
}

static zend_object *php_swoole_http_response_create_object(zend_class_entry *ce) {
    HttpResponseObject *response = (HttpResponseObject *) zend_object_alloc(sizeof(HttpResponseObject), ce);
    zend_object_std_init(&response->std, ce);
    object_properties_init(&response->std, ce);
    response->std.handlers = &swoole_http_response_handlers;
    return &response->std;
}

SW_EXTERN_C_BEGIN
static PHP_METHOD(swoole_http_response, write);
static PHP_METHOD(swoole_http_response, end);
static PHP_METHOD(swoole_http_response, sendfile);
static PHP_METHOD(swoole_http_response, redirect);
static PHP_METHOD(swoole_http_response, cookie);
static PHP_METHOD(swoole_http_response, rawcookie);
static PHP_METHOD(swoole_http_response, header);
static PHP_METHOD(swoole_http_response, initHeader);
static PHP_METHOD(swoole_http_response, isWritable);
static PHP_METHOD(swoole_http_response, detach);
static PHP_METHOD(swoole_http_response, create);
/**
 * for WebSocket Client
 */
static PHP_METHOD(swoole_http_response, upgrade);
static PHP_METHOD(swoole_http_response, push);
static PHP_METHOD(swoole_http_response, recv);
static PHP_METHOD(swoole_http_response, close);
static PHP_METHOD(swoole_http_response, trailer);
static PHP_METHOD(swoole_http_response, ping);
static PHP_METHOD(swoole_http_response, goaway);
static PHP_METHOD(swoole_http_response, status);
SW_EXTERN_C_END

// clang-format off
const zend_function_entry swoole_http_response_methods[] =
{
    PHP_ME(swoole_http_response, initHeader,                        arginfo_class_Swoole_Http_Response_initHeader,   ZEND_ACC_PUBLIC)
    PHP_ME(swoole_http_response, isWritable,                        arginfo_class_Swoole_Http_Response_isWritable,   ZEND_ACC_PUBLIC)
    PHP_ME(swoole_http_response, cookie,                            arginfo_class_Swoole_Http_Response_cookie,       ZEND_ACC_PUBLIC)
    PHP_MALIAS(swoole_http_response, setCookie, cookie,             arginfo_class_Swoole_Http_Response_cookie,       ZEND_ACC_PUBLIC)
    PHP_ME(swoole_http_response, rawcookie,                         arginfo_class_Swoole_Http_Response_cookie,       ZEND_ACC_PUBLIC)
    PHP_MALIAS(swoole_http_response, setRawCookie, rawcookie,       arginfo_class_Swoole_Http_Response_cookie,       ZEND_ACC_PUBLIC)
    PHP_ME(swoole_http_response, status,                            arginfo_class_Swoole_Http_Response_status,       ZEND_ACC_PUBLIC)
    PHP_MALIAS(swoole_http_response, setStatusCode, status,         arginfo_class_Swoole_Http_Response_status,       ZEND_ACC_PUBLIC)
    PHP_ME(swoole_http_response, header,                            arginfo_class_Swoole_Http_Response_header,       ZEND_ACC_PUBLIC)
    PHP_MALIAS(swoole_http_response, setHeader, header,             arginfo_class_Swoole_Http_Response_header,       ZEND_ACC_PUBLIC)
    PHP_ME(swoole_http_response, trailer,                           arginfo_class_Swoole_Http_Response_trailer,      ZEND_ACC_PUBLIC)
    PHP_ME(swoole_http_response, ping,                              arginfo_class_Swoole_Http_Response_ping,         ZEND_ACC_PUBLIC)
    PHP_ME(swoole_http_response, goaway,                            arginfo_class_Swoole_Http_Response_goaway,       ZEND_ACC_PUBLIC)
    PHP_ME(swoole_http_response, write,                             arginfo_class_Swoole_Http_Response_write,        ZEND_ACC_PUBLIC)
    PHP_ME(swoole_http_response, end,                               arginfo_class_Swoole_Http_Response_end,          ZEND_ACC_PUBLIC)
    PHP_ME(swoole_http_response, sendfile,                          arginfo_class_Swoole_Http_Response_sendfile,     ZEND_ACC_PUBLIC)
    PHP_ME(swoole_http_response, redirect,                          arginfo_class_Swoole_Http_Response_redirect,     ZEND_ACC_PUBLIC)
    PHP_ME(swoole_http_response, detach,                            arginfo_class_Swoole_Http_Response_detach,       ZEND_ACC_PUBLIC)
    PHP_ME(swoole_http_response, create,                            arginfo_class_Swoole_Http_Response_create,       ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
    /**
     * WebSocket
     */
    PHP_ME(swoole_http_response, upgrade,    arginfo_class_Swoole_Http_Response_upgrade,    ZEND_ACC_PUBLIC)
    PHP_ME(swoole_http_response, push,       arginfo_class_Swoole_Http_Response_push,       ZEND_ACC_PUBLIC)
    PHP_ME(swoole_http_response, recv,       arginfo_class_Swoole_Http_Response_recv,       ZEND_ACC_PUBLIC)
    PHP_ME(swoole_http_response, close,      arginfo_class_Swoole_Http_Response_close,      ZEND_ACC_PUBLIC)
    PHP_FE_END
};
// clang-format on

void php_swoole_http_response_minit(int module_number) {
    SW_INIT_CLASS_ENTRY(swoole_http_response, "Swoole\\Http\\Response", nullptr, swoole_http_response_methods);
    SW_SET_CLASS_NOT_SERIALIZABLE(swoole_http_response);
    SW_SET_CLASS_CLONEABLE(swoole_http_response, sw_zend_class_clone_deny);
    SW_SET_CLASS_UNSET_PROPERTY_HANDLER(swoole_http_response, sw_zend_class_unset_property_deny);
    SW_SET_CLASS_CUSTOM_OBJECT(swoole_http_response,
                               php_swoole_http_response_create_object,
                               php_swoole_http_response_free_object,
                               HttpResponseObject,
                               std);

    zend_declare_property_long(swoole_http_response_ce, ZEND_STRL("fd"), 0, ZEND_ACC_PUBLIC);
    zend_declare_property_null(swoole_http_response_ce, ZEND_STRL("socket"), ZEND_ACC_PUBLIC);
    zend_declare_property_null(swoole_http_response_ce, ZEND_STRL("header"), ZEND_ACC_PUBLIC);
    zend_declare_property_null(swoole_http_response_ce, ZEND_STRL("cookie"), ZEND_ACC_PUBLIC);
    zend_declare_property_null(swoole_http_response_ce, ZEND_STRL("trailer"), ZEND_ACC_PUBLIC);
}

static PHP_METHOD(swoole_http_response, write) {
    zval *zdata;

    ZEND_PARSE_PARAMETERS_START(1, 1)
    Z_PARAM_ZVAL(zdata)
    ZEND_PARSE_PARAMETERS_END_EX(RETURN_FALSE);

    HttpContext *ctx = php_swoole_http_response_get_and_check_context(ZEND_THIS);
    if (UNEXPECTED(!ctx)) {
        RETURN_FALSE;
    }

#ifdef SW_HAVE_COMPRESSION
    // Why not enable compression?
    // If both compression and chunked encoding are enabled,
    // then the content stream is first compressed, then chunked;
    // so the chunk encoding itself is not compressed,
    // **and the data in each chunk is not compressed individually.**
    // The remote endpoint then decodes the stream by concatenating the chunks and decompressing the result.
    ctx->accept_compression = 0;
#endif

    if (ctx->http2) {
        ctx->http2_write(zdata, return_value);
    } else {
        ctx->write(zdata, return_value);
    }
}

static int parse_header_name(const char *key, size_t keylen) {
    if (SW_STRCASEEQ(key, keylen, "Server")) {
        return HTTP_HEADER_SERVER;
    } else if (SW_STRCASEEQ(key, keylen, "Connection")) {
        return HTTP_HEADER_CONNECTION;
    } else if (SW_STRCASEEQ(key, keylen, "Date")) {
        return HTTP_HEADER_DATE;
    } else if (SW_STRCASEEQ(key, keylen, "Content-Length")) {
        return HTTP_HEADER_CONTENT_LENGTH;
    } else if (SW_STRCASEEQ(key, keylen, "Content-Type")) {
        return HTTP_HEADER_CONTENT_TYPE;
    } else if (SW_STRCASEEQ(key, keylen, "Transfer-Encoding")) {
        return HTTP_HEADER_TRANSFER_ENCODING;
    } else if (SW_STRCASEEQ(key, keylen, "Content-Encoding")) {
        return HTTP_HEADER_CONTENT_ENCODING;
    }
    return 0;
}

static void http_set_date_header(String *response) {
    static SW_THREAD_LOCAL struct {
        time_t time;
        zend_string *date = nullptr;
    } cache{};

    time_t now = time(nullptr);
    if (now != cache.time) {
        if (cache.date) {
            zend_string_release(cache.date);
        }

        cache.time = now;
        cache.date = php_format_date((char *) ZEND_STRL(SW_HTTP_DATE_FORMAT), now, 0);
    }
    response->append(ZEND_STRL("Date: "));
    response->append(ZSTR_VAL(cache.date), ZSTR_LEN(cache.date));
    response->append(ZEND_STRL("\r\n"));
}

static void add_custom_header(String *http_buffer, const char *key, size_t l_key, zval *value) {
    if (ZVAL_IS_NULL(value)) {
        return;
    }

    zend::String str_value(value);
    str_value.rtrim();
    if (swoole_http_has_crlf(str_value.val(), str_value.len())) {
        return;
    }

    http_buffer->append(key, l_key);
    http_buffer->append(SW_STRL(": "));
    http_buffer->append(str_value.val(), str_value.len());
    http_buffer->append(SW_STRL("\r\n"));
}

void HttpContext::build_header(String *http_buffer, const char *body, size_t length) {
    assert(send_header_ == 0);

    // http status line
    http_buffer->append(ZEND_STRL("HTTP/1.1 "));
    if (response.reason) {
        http_buffer->append(response.status);
        http_buffer->append(ZEND_STRL(" "));
        http_buffer->append(response.reason, strlen(response.reason));
    } else {
        const char *status = HttpServer::get_status_message(response.status);
        http_buffer->append((char *) status, strlen(status));
    }
    http_buffer->append(ZEND_STRL("\r\n"));

    // http headers
    uint32_t header_flags = 0x0;
    zval *zheader =
        sw_zend_read_property_ex(swoole_http_response_ce, response.zobject, SW_ZSTR_KNOWN(SW_ZEND_STR_HEADER), 0);
    if (ZVAL_IS_ARRAY(zheader)) {
#ifdef SW_HAVE_COMPRESSION
        zend_string *content_type = nullptr;
#endif
        zval *zvalue;
        zend_string *string_key;
        zend_ulong num_key;

        ZEND_HASH_FOREACH_KEY_VAL(Z_ARRVAL_P(zheader), num_key, string_key, zvalue) {
            if (!string_key) {
                string_key = zend_long_to_str(num_key);
            } else {
                zend_string_addref(string_key);
            }
            zend::String key(string_key, false);

            int key_header = parse_header_name(ZSTR_VAL(string_key), ZSTR_LEN(string_key));

            if (key_header > 0) {
#ifdef SW_HAVE_COMPRESSION
                if (key_header == HTTP_HEADER_CONTENT_TYPE && accept_compression && compression_types) {
                    content_type = zval_get_string(zvalue);
                }
                if (key_header == HTTP_HEADER_CONTENT_ENCODING && ZVAL_IS_STRING(zvalue) && Z_STRLEN_P(zvalue) == 0) {
                    accept_compression = 0;
                }
                // https://github.com/swoole/swoole-src/issues/4857
                // https://github.com/swoole/swoole-src/issues/5026
                if (key_header == HTTP_HEADER_CONTENT_LENGTH && accept_compression) {
                    swoole_error_log(SW_LOG_WARNING,
                                     SW_ERROR_HTTP_CONFLICT_HEADER,
                                     "The client has set 'Accept-Encoding', 'Content-Length' will be ignored");
                    continue;
                }
#endif
                // https://github.com/swoole/swoole-src/issues/4857
                // https://github.com/swoole/swoole-src/issues/5026
                if (key_header == HTTP_HEADER_CONTENT_LENGTH && send_chunked) {
                    swoole_error_log(SW_LOG_WARNING,
                                     SW_ERROR_HTTP_CONFLICT_HEADER,
                                     "You have set 'Transfer-Encoding', 'Content-Length' will be ignored");
                    continue;
                }

                header_flags |= key_header;
                if (ZVAL_IS_STRING(zvalue) && Z_STRLEN_P(zvalue) == 0) {
                    continue;
                }
            }
            if (ZVAL_IS_ARRAY(zvalue)) {
                zval *zvalue_2;
                SW_HASHTABLE_FOREACH_START(Z_ARRVAL_P(zvalue), zvalue_2) {
                    add_custom_header(http_buffer, ZSTR_VAL(string_key), ZSTR_LEN(string_key), zvalue_2);
                }
                SW_HASHTABLE_FOREACH_END();
            } else {
                add_custom_header(http_buffer, ZSTR_VAL(string_key), ZSTR_LEN(string_key), zvalue);
            }
        }
        ZEND_HASH_FOREACH_END();

#ifdef SW_HAVE_COMPRESSION
        if (accept_compression && compression_types) {
            std::string str_content_type = content_type ? std::string(ZSTR_VAL(content_type), ZSTR_LEN(content_type))
                                                        : std::string(ZEND_STRL(SW_HTTP_DEFAULT_CONTENT_TYPE));
            accept_compression = compression_types->find(str_content_type) != compression_types->end();
            if (content_type) {
                zend_string_release(content_type);
            }
        }
#endif
    }

    // http cookies
    zval *zcookie =
        sw_zend_read_property_ex(swoole_http_response_ce, response.zobject, SW_ZSTR_KNOWN(SW_ZEND_STR_COOKIE), 0);
    if (ZVAL_IS_ARRAY(zcookie)) {
        zval *zvalue;
        SW_HASHTABLE_FOREACH_START(Z_ARRVAL_P(zcookie), zvalue) {
            if (Z_TYPE_P(zvalue) != IS_STRING || swoole_http_has_crlf(Z_STRVAL_P(zvalue), Z_STRLEN_P(zvalue))) {
                continue;
            }
            http_buffer->append(ZEND_STRL("Set-Cookie: "));
            http_buffer->append(Z_STRVAL_P(zvalue), Z_STRLEN_P(zvalue));
            http_buffer->append(ZEND_STRL("\r\n"));
        }
        SW_HASHTABLE_FOREACH_END();
    }

    if (!(header_flags & HTTP_HEADER_SERVER)) {
        http_buffer->append(ZEND_STRL("Server: " SW_HTTP_SERVER_SOFTWARE "\r\n"));
    }

    if (!(header_flags & HTTP_HEADER_DATE)) {
        http_set_date_header(http_buffer);
    }

    // websocket protocol (subsequent header info is unnecessary)
    if (upgrade == 1) {
        http_buffer->append(ZEND_STRL("\r\n"));
        send_header_ = 1;
        return;
    }
    if (!(header_flags & HTTP_HEADER_CONNECTION)) {
        if (keepalive) {
            http_buffer->append(ZEND_STRL("Connection: keep-alive\r\n"));
        } else {
            http_buffer->append(ZEND_STRL("Connection: close\r\n"));
        }
    }
    if (!(header_flags & HTTP_HEADER_CONTENT_TYPE)) {
        http_buffer->append(ZEND_STRL("Content-Type: " SW_HTTP_DEFAULT_CONTENT_TYPE "\r\n"));
    }
    if (send_chunked) {
        SW_ASSERT(length == 0);
        if (!(header_flags & HTTP_HEADER_TRANSFER_ENCODING)) {
            http_buffer->append(ZEND_STRL("Transfer-Encoding: chunked\r\n"));
        }
    }
    // Content-Length
    else if (length > 0 || parser.method != HTTP_HEAD) {
#ifdef SW_HAVE_COMPRESSION
        if (compress(body, length)) {
            length = zlib_buffer->length;
            const char *content_encoding = get_content_encoding();
            http_buffer->append(ZEND_STRL("Content-Encoding: "));
            http_buffer->append((char *) content_encoding, strlen(content_encoding));
            http_buffer->append(ZEND_STRL("\r\n"));
        }
#endif
        if (!(header_flags & HTTP_HEADER_CONTENT_LENGTH)) {
            http_buffer->append(ZEND_STRL("Content-Length: "));

            char result[128];
            int convert_result = swoole_itoa(result, length);
            http_buffer->append(result, convert_result);
            http_buffer->append(ZEND_STRL("\r\n"));
        }
    }

    http_buffer->append(ZEND_STRL("\r\n"));
    send_header_ = 1;
}

ssize_t HttpContext::build_trailer(String *http_buffer) {
    char *buf = sw_tg_buffer()->str;
    size_t l_buf = sw_tg_buffer()->size;
    int n;
    ssize_t ret = 0;

    zval *ztrailer =
        sw_zend_read_property_ex(swoole_http_response_ce, response.zobject, SW_ZSTR_KNOWN(SW_ZEND_STR_TRAILER), 0);
    uint32_t size = php_swoole_array_length_safe(ztrailer);

    if (size > 0) {
        const char *key;
        uint32_t keylen;
        int type;
        zval *zvalue;

        SW_HASHTABLE_FOREACH_START2(Z_ARRVAL_P(ztrailer), key, keylen, type, zvalue) {
            if (UNEXPECTED(!key || ZVAL_IS_NULL(zvalue))) {
                continue;
            }

            if (!ZVAL_IS_NULL(zvalue)) {
                zend::String str_value(zvalue);
                n = sw_snprintf(
                    buf, l_buf, "%.*s: %.*s\r\n", (int) keylen, key, (int) str_value.len(), str_value.val());
                http_buffer->append(buf, n);
                ret += n;
            }
        }
        SW_HASHTABLE_FOREACH_END();
        (void) type;
        http_buffer->append(ZEND_STRL("\r\n"));
    }

    return ret;
}

#ifdef SW_HAVE_ZLIB
voidpf php_zlib_alloc(voidpf opaque, uInt items, uInt size) {
    return (voidpf) safe_emalloc(items, size, 0);
}

void php_zlib_free(voidpf opaque, voidpf address) {
    efree((void *) address);
}
#endif

#ifdef SW_HAVE_BROTLI
void *php_brotli_alloc(void *opaque, size_t size) {
    return emalloc(size);
}

void php_brotli_free(void *opaque, void *address) {
    efree(address);
}
#endif

#ifdef SW_HAVE_COMPRESSION
bool HttpContext::compress(const char *data, size_t length) {
#ifdef SW_HAVE_ZLIB
    int encoding;
#endif

    if (!accept_compression || length < compression_min_length) {
        return false;
    }

    if (0) {
        return false;
    }
#ifdef SW_HAVE_ZLIB
    // gzip: 0x1f
    else if (compression_method == HTTP_COMPRESS_GZIP) {
        encoding = 0x1f;
    }
    // deflate: -0xf
    else if (compression_method == HTTP_COMPRESS_DEFLATE) {
        encoding = -0xf;
    }
#endif
#ifdef SW_HAVE_BROTLI
    else if (compression_method == HTTP_COMPRESS_BR) {
        if (compression_level < BROTLI_MIN_QUALITY) {
            compression_level = BROTLI_MIN_QUALITY;
        } else if (compression_level > BROTLI_MAX_QUALITY) {
            compression_level = BROTLI_MAX_QUALITY;
        }

        size_t memory_size = BrotliEncoderMaxCompressedSize(length);
        zlib_buffer = std::make_shared<String>(memory_size);

        size_t input_size = length;
        const uint8_t *input_buffer = (const uint8_t *) data;
        size_t encoded_size = zlib_buffer->size;
        uint8_t *encoded_buffer = (uint8_t *) zlib_buffer->str;

        if (BROTLI_TRUE != BrotliEncoderCompress(compression_level,
                                                 BROTLI_DEFAULT_WINDOW,
                                                 BROTLI_DEFAULT_MODE,
                                                 input_size,
                                                 input_buffer,
                                                 &encoded_size,
                                                 encoded_buffer)) {
            swoole_warning("BrotliEncoderCompress() failed");
            return false;
        } else {
            zlib_buffer->length = encoded_size;
            content_compressed = 1;
            return true;
        }
    }
#endif
#ifdef SW_HAVE_ZSTD
    else if (compression_method == HTTP_COMPRESS_ZSTD) {
        int zstd_compress_level = compression_level;
        int zstd_max_level = ZSTD_maxCLevel();
        int zstd_min_level = ZSTD_minCLevel();
        zstd_compress_level = (zstd_compress_level > zstd_max_level)
                                  ? zstd_max_level
                                  : (zstd_compress_level < zstd_min_level ? zstd_min_level : zstd_compress_level);

        size_t compress_size = ZSTD_compressBound(length);
        zlib_buffer = std::make_shared<String>(compress_size);
        size_t zstd_compress_result =
            ZSTD_compress((void *) zlib_buffer->str, compress_size, (void *) data, length, zstd_compress_level);

        if (ZSTD_isError(zstd_compress_result)) {
            swoole_warning("ZSTD_compress() failed, Error: [%s]", ZSTD_getErrorName(zstd_compress_result));
            return false;
        }

        zlib_buffer->length = zstd_compress_result;
        content_compressed = 1;
        return true;
    }
#endif
    else {
        swoole_warning("Unknown compression method");
        return false;
    }
#ifdef SW_HAVE_ZLIB
    if (compression_level < Z_NO_COMPRESSION) {
        compression_level = Z_DEFAULT_COMPRESSION;
    } else if (compression_level == Z_NO_COMPRESSION) {
        compression_level = Z_BEST_SPEED;
    } else if (compression_level > Z_BEST_COMPRESSION) {
        compression_level = Z_BEST_COMPRESSION;
    }

    size_t memory_size = ((size_t)((double) length * (double) 1.015)) + 10 + 8 + 4 + 1;
    zlib_buffer = std::make_shared<String>(memory_size);

    z_stream zstream = {};
    int status;

    zstream.zalloc = php_zlib_alloc;
    zstream.zfree = php_zlib_free;

    status = deflateInit2(&zstream, compression_level, Z_DEFLATED, encoding, MAX_MEM_LEVEL, Z_DEFAULT_STRATEGY);
    if (status != Z_OK) {
        swoole_warning("deflateInit2() failed, Error: [%d]", status);
        return false;
    }

    zstream.next_in = (Bytef *) data;
    zstream.avail_in = length;
    zstream.next_out = (Bytef *) zlib_buffer->str;
    zstream.avail_out = zlib_buffer->size;

    status = deflate(&zstream, Z_FINISH);
    deflateEnd(&zstream);
    if (status != Z_STREAM_END) {
        swoole_warning("deflate() failed, Error: [%d]", status);
        return false;
    }

    zlib_buffer->length = zstream.total_out;
    zlib_buffer->offset = 0;
    content_compressed = 1;
    return true;
#endif
}
#endif

static PHP_METHOD(swoole_http_response, initHeader) {
    HttpContext *ctx = php_swoole_http_response_get_and_check_context(ZEND_THIS);
    if (UNEXPECTED(!ctx)) {
        RETURN_FALSE;
    }
    zval *zresponse_object = ctx->response.zobject;
    swoole_http_init_and_read_property(
        swoole_http_response_ce, zresponse_object, &ctx->response.zheader, ZEND_STRL("header"));
    swoole_http_init_and_read_property(
        swoole_http_response_ce, zresponse_object, &ctx->response.zcookie, ZEND_STRL("cookie"));
    swoole_http_init_and_read_property(
        swoole_http_response_ce, zresponse_object, &ctx->response.ztrailer, ZEND_STRL("trailer"));
    RETURN_TRUE;
}

static PHP_METHOD(swoole_http_response, isWritable) {
    HttpContext *ctx = php_swoole_http_response_get_context(ZEND_THIS);
    if (!ctx || (ctx->end_ || ctx->detached)) {
        RETURN_FALSE;
    }
    RETURN_TRUE;
}

static PHP_METHOD(swoole_http_response, end) {
    HttpContext *ctx = php_swoole_http_response_get_and_check_context(ZEND_THIS);
    if (UNEXPECTED(!ctx)) {
        RETURN_FALSE;
    }

    zval *zdata = nullptr;

    ZEND_PARSE_PARAMETERS_START(0, 1)
    Z_PARAM_OPTIONAL
    Z_PARAM_ZVAL_EX(zdata, 1, 0)
    ZEND_PARSE_PARAMETERS_END_EX(RETURN_FALSE);

    if (ctx->onAfterResponse) {
        ctx->onAfterResponse(ctx);
    }

    if (swoole_isset_hook((enum swGlobalHookType) PHP_SWOOLE_HOOK_AFTER_RESPONSE)) {
        swoole_call_hook((enum swGlobalHookType) PHP_SWOOLE_HOOK_AFTER_RESPONSE, ctx);
    }

    if (ctx->http2) {
        ctx->http2_end(zdata, return_value);
    } else {
        ctx->end(zdata, return_value);
    }
}

void HttpContext::send_trailer(zval *return_value) {
    String *http_buffer = get_write_buffer();

    http_buffer->clear();
    if (build_trailer(http_buffer) == 0) {
        return;
    }
    if (!send(this, http_buffer->str, http_buffer->length)) {
        end_ = 1;
        close(this);
        RETURN_FALSE;
    }
}

bool HttpContext::send_file(const char *file, uint32_t l_file, off_t offset, size_t length) {
    zval *zheader =
        sw_zend_read_and_convert_property_array(swoole_http_response_ce, response.zobject, ZEND_STRL("header"), 0);
    if (!zend_hash_str_exists(Z_ARRVAL_P(zheader), ZEND_STRL("Content-Type"))) {
        add_assoc_string(zheader, "Content-Type", (char *) swoole::mime_type::get(file).c_str());
    }

    if (!send_header_) {
#ifdef SW_HAVE_COMPRESSION
        accept_compression = 0;
#endif
        String *http_buffer = get_write_buffer();
        http_buffer->clear();

        build_header(http_buffer, nullptr, length);

        if (!send(this, http_buffer->str, http_buffer->length)) {
            send_header_ = 0;
            return false;
        }
    }

    if (length > 0 && !sendfile(this, file, l_file, offset, length)) {
        close(this);
        return false;
    }

    end_ = 1;

    if (!keepalive) {
        close(this);
    }
    return true;
}

void HttpContext::write(zval *zdata, zval *return_value) {
    String *http_buffer = get_write_buffer();

    if (!send_header_) {
        send_chunked = 1;
        http_buffer->clear();
        build_header(http_buffer, nullptr, 0);
        if (!send(this, http_buffer->str, http_buffer->length)) {
            send_chunked = 0;
            send_header_ = 0;
            RETURN_FALSE;
        }
    }

    char *data = nullptr;
    size_t length = php_swoole_get_send_data(zdata, &data);

    if (length == 0) {
        php_swoole_error_ex(E_WARNING, SW_ERROR_NO_PAYLOAD, "the data sent must not be empty");
        RETURN_FALSE;
    }

    http_buffer->clear();
    char *hex_string = swoole_dec2hex(length, 16);
    int hex_len = strlen(hex_string);
    //"%.*s\r\n%.*s\r\n", hex_len, hex_string, body.length, body.str
    http_buffer->append(hex_string, hex_len);
    http_buffer->append(ZEND_STRL("\r\n"));
    http_buffer->append(data, length);
    http_buffer->append(ZEND_STRL("\r\n"));
    sw_free(hex_string);

    RETURN_BOOL(send(this, http_buffer->str, http_buffer->length));
}

void HttpContext::end(zval *zdata, zval *return_value) {
    if (send_chunked) {
        if (zdata && Z_STRLEN_P(zdata) > 0) {
            zval retval;
            write(zdata, &retval);
            if (ZVAL_IS_FALSE(&retval)) {
                RETURN_FALSE;
            }
        }
        if (send_trailer_) {
            if (!send(this, ZEND_STRL("0\r\n"))) {
                RETURN_FALSE;
            }
            send_trailer(return_value);
            send_trailer_ = 0;
        } else {
            if (!send(this, ZEND_STRL("0\r\n\r\n"))) {
                RETURN_FALSE;
            }
        }
        send_chunked = 0;
    } else {
        char *data = nullptr;
        size_t length = zdata ? php_swoole_get_send_data(zdata, &data) : 0;

        String *http_buffer = get_write_buffer();
        http_buffer->clear();

#ifdef SW_HAVE_ZLIB
        if (upgrade) {
            Server *serv = nullptr;
            Connection *conn = nullptr;
            if (!co_socket) {
                serv = (Server *) private_data;
                conn = serv->get_connection_verify(fd);
            }
            bool enable_websocket_compression = co_socket ? websocket_compression : serv->websocket_compression;
            bool accept_websocket_compression = false;
            zval *pData;
            if (enable_websocket_compression && request.zobject &&
                (pData = zend_hash_str_find(Z_ARRVAL_P(request.zheader), ZEND_STRL("sec-websocket-extensions"))) &&
                Z_TYPE_P(pData) == IS_STRING) {
                std::string value(Z_STRVAL_P(pData), Z_STRLEN_P(pData));
                if (value.substr(0, value.find_first_of(';')) == "permessage-deflate") {
                    accept_websocket_compression = true;
                    set_header(ZEND_STRL("Sec-Websocket-Extensions"), ZEND_STRL(SW_WEBSOCKET_EXTENSION_DEFLATE), false);
                }
            }
            websocket_compression = accept_websocket_compression;
            if (conn) {
                conn->websocket_compression = accept_websocket_compression;
            }
        }
#endif

        build_header(http_buffer, data, length);

        if (length > 0) {
#ifdef SW_HAVE_COMPRESSION
            if (content_compressed) {
                data = zlib_buffer->str;
                length = zlib_buffer->length;
            }
#endif
            // send twice to reduce memory copy
            if (length > SW_HTTP_MAX_APPEND_DATA) {
                if (!send(this, http_buffer->str, http_buffer->length)) {
                    send_header_ = 0;
                    RETURN_FALSE;
                }
                if (!send(this, data, length)) {
                    end_ = 1;
                    close(this);
                    RETURN_FALSE;
                }
                goto _skip_copy;
            } else {
                if (http_buffer->append(data, length) < 0) {
                    send_header_ = 0;
                    RETURN_FALSE;
                }
            }
        }

        if (!send(this, http_buffer->str, http_buffer->length)) {
            end_ = 1;
            close(this);
            RETURN_FALSE;
        }
    }

_skip_copy:
    if (upgrade && !co_socket) {
        Server *serv = (Server *) private_data;
        Connection *conn = serv->get_connection_verify(fd);

        if (conn && conn->websocket_status == websocket::STATUS_HANDSHAKE) {
            if (response.status == 101) {
                conn->websocket_status = websocket::STATUS_ACTIVE;
            } else {
                /* connection should be closed when handshake failed */
                conn->websocket_status = websocket::STATUS_NONE;
                keepalive = 0;
            }
        }
    }
    if (!keepalive) {
        close(this);
    }
    end_ = 1;
    RETURN_TRUE;
}

bool HttpContext::set_header(const char *k, size_t klen, const char *v, size_t vlen, bool format) {
    zend::Variable ztmp(v, vlen);
    return set_header(k, klen, ztmp.ptr(), format);
}

bool HttpContext::set_header(const char *k, size_t klen, const std::string &v, bool format) {
    zend::Variable ztmp(v);
    return set_header(k, klen, ztmp.ptr(), format);
}

bool HttpContext::set_header(const char *k, size_t klen, zval *zvalue, bool format) {
    if (UNEXPECTED(klen > SW_HTTP_HEADER_KEY_SIZE - 1)) {
        php_swoole_error(E_WARNING, "header key is too long");
        return false;
    }

    if (swoole_http_has_crlf(k, klen)) {
        return false;
    }

    zval *zheader = swoole_http_init_and_read_property(
        swoole_http_response_ce, response.zobject, &response.zheader, ZEND_STRL("header"));
    if (format) {
        swoole_strlcpy(sw_tg_buffer()->str, k, SW_HTTP_HEADER_KEY_SIZE);
        if (http2) {
            swoole_strtolower(sw_tg_buffer()->str, klen);
        } else {
            http_header_key_format(sw_tg_buffer()->str, klen);
        }
        k = sw_tg_buffer()->str;
    }
    zend::array_set(zheader, k, klen, zvalue);
    return true;
}

static PHP_METHOD(swoole_http_response, sendfile) {
    HttpContext *ctx = php_swoole_http_response_get_and_check_context(ZEND_THIS);
    if (UNEXPECTED(!ctx)) {
        RETURN_FALSE;
    }

    if (ctx->send_chunked) {
        php_swoole_fatal_error(E_WARNING, "can't use sendfile when HTTP chunk is enabled");
        RETURN_FALSE;
    }

    char *file;
    size_t l_file;
    zend_long offset = 0;
    zend_long length = 0;

    ZEND_PARSE_PARAMETERS_START(1, 3)
    Z_PARAM_STRING(file, l_file)
    Z_PARAM_OPTIONAL
    Z_PARAM_LONG(offset)
    Z_PARAM_LONG(length)
    ZEND_PARSE_PARAMETERS_END_EX(RETURN_FALSE);

    if (l_file == 0) {
        php_swoole_error(E_WARNING, "file name is empty");
        RETURN_FALSE;
    }

    struct stat file_stat;
    if (stat(file, &file_stat) < 0) {
        php_swoole_sys_error(E_WARNING, "stat(%s) failed", file);
        RETURN_FALSE;
    }
    if (!S_ISREG(file_stat.st_mode)) {
        php_swoole_error(E_WARNING, "parameter $file[%s] given is not a regular file", file);
        swoole_set_last_error(SW_ERROR_SERVER_IS_NOT_REGULAR_FILE);
        RETURN_FALSE;
    }
    if (file_stat.st_size < offset) {
        php_swoole_error(E_WARNING, "parameter $offset[" ZEND_LONG_FMT "] exceeds the file size", offset);
        RETURN_FALSE;
    }
    if (length > file_stat.st_size - offset) {
        php_swoole_error(E_WARNING, "parameter $length[" ZEND_LONG_FMT "] exceeds the file size", length);
        RETURN_FALSE;
    }
    if (length == 0) {
        length = file_stat.st_size - offset;
    }

    if (ctx->onAfterResponse) {
        ctx->onAfterResponse(ctx);
    }
    if (swoole_isset_hook((enum swGlobalHookType) PHP_SWOOLE_HOOK_AFTER_RESPONSE)) {
        swoole_call_hook((enum swGlobalHookType) PHP_SWOOLE_HOOK_AFTER_RESPONSE, ctx);
    }
    if (ctx->http2) {
        RETURN_BOOL(ctx->http2_send_file(file, l_file, offset, length));
    } else {
        RETURN_BOOL(ctx->send_file(file, l_file, offset, length));
    }
}

static bool inline php_swoole_http_response_create_cookie(HttpCookie *cookie, zval *zobject) {
    HttpContext *ctx = php_swoole_http_response_get_and_check_context(zobject);

    zend_string *cookie_str = cookie->toString();
    if (!cookie_str) {
        cookie->reset();
        return false;
    }

    add_next_index_str(
        swoole_http_init_and_read_property(
            swoole_http_response_ce, ctx->response.zobject, &ctx->response.zcookie, SW_ZSTR_KNOWN(SW_ZEND_STR_COOKIE)),
        cookie_str);

    return true;
}

static void php_swoole_http_response_set_cookie(INTERNAL_FUNCTION_PARAMETERS, const bool encode) {
    zval *name_or_object;
    zend_string *value = nullptr, *path = nullptr, *domain = nullptr, *sameSite = nullptr, *priority = nullptr;
    zend_long expires = 0;
    zend_bool secure = false, httpOnly = false, partitioned = false;
    bool result;

    ZEND_PARSE_PARAMETERS_START(1, 10)
    Z_PARAM_ZVAL(name_or_object)
    Z_PARAM_OPTIONAL
    Z_PARAM_STR(value)
    Z_PARAM_LONG(expires)
    Z_PARAM_STR(path)
    Z_PARAM_STR(domain)
    Z_PARAM_BOOL(secure)
    Z_PARAM_BOOL(httpOnly)
    Z_PARAM_STR(sameSite)
    Z_PARAM_STR(priority)
    Z_PARAM_BOOL(partitioned)
    ZEND_PARSE_PARAMETERS_END_EX(RETURN_FALSE);

    if (ZVAL_IS_STRING(name_or_object)) {
        HttpCookie cookie(encode);
        (&cookie)
            ->withName(Z_STR_P(name_or_object))
            ->withValue(value)
            ->withExpires(expires)
            ->withPath(path)
            ->withDomain(domain)
            ->withSecure(secure)
            ->withHttpOnly(httpOnly)
            ->withSameSite(sameSite)
            ->withPriority(priority)
            ->withPartitioned(partitioned);
        result = php_swoole_http_response_create_cookie(&cookie, ZEND_THIS);
    } else if (ZVAL_IS_OBJECT(name_or_object)) {
        HttpCookie *cookie = php_swoole_http_get_cooke_safety(name_or_object);
        result = php_swoole_http_response_create_cookie(cookie, ZEND_THIS);
    } else {
        php_swoole_error(E_WARNING, "The first argument must be a string or an cookie object");
        result = false;
    }

    RETURN_BOOL(result);
}

static PHP_METHOD(swoole_http_response, cookie) {
    php_swoole_http_response_set_cookie(INTERNAL_FUNCTION_PARAM_PASSTHRU, true);
}

static PHP_METHOD(swoole_http_response, rawcookie) {
    php_swoole_http_response_set_cookie(INTERNAL_FUNCTION_PARAM_PASSTHRU, false);
}

static PHP_METHOD(swoole_http_response, status) {
    zend_long http_status;
    char *reason = nullptr;
    size_t reason_len = 0;

    ZEND_PARSE_PARAMETERS_START(1, 2)
    Z_PARAM_LONG(http_status)
    Z_PARAM_OPTIONAL
    Z_PARAM_STRING(reason, reason_len)
    ZEND_PARSE_PARAMETERS_END_EX(RETURN_FALSE);

    HttpContext *ctx = php_swoole_http_response_get_and_check_context(ZEND_THIS);
    if (UNEXPECTED(!ctx)) {
        RETURN_FALSE;
    }

    ctx->response.status = http_status;
    ctx->response.reason = reason_len > 0 ? estrndup(reason, reason_len) : nullptr;
    RETURN_TRUE;
}

static PHP_METHOD(swoole_http_response, header) {
    char *k;
    size_t klen;
    zval *zvalue;
    zend_bool format = 1;

    ZEND_PARSE_PARAMETERS_START(2, 3)
    Z_PARAM_STRING(k, klen)
    Z_PARAM_ZVAL(zvalue)
    Z_PARAM_OPTIONAL
    Z_PARAM_BOOL(format)
    ZEND_PARSE_PARAMETERS_END_EX(RETURN_FALSE);

    HttpContext *ctx = php_swoole_http_response_get_and_check_context(ZEND_THIS);
    if (UNEXPECTED(!ctx)) {
        RETURN_FALSE;
    }
    RETURN_BOOL(ctx->set_header(k, klen, zvalue, format));
}

static PHP_METHOD(swoole_http_response, trailer) {
    char *k, *v;
    size_t klen, vlen;
    char key_buf[SW_HTTP_HEADER_KEY_SIZE];

    ZEND_PARSE_PARAMETERS_START(2, 2)
    Z_PARAM_STRING(k, klen)
    Z_PARAM_STRING_EX(v, vlen, 1, 0)
    ZEND_PARSE_PARAMETERS_END_EX(RETURN_FALSE);

    HttpContext *ctx = php_swoole_http_response_get_and_check_context(ZEND_THIS);
    if (!ctx) {
        RETURN_FALSE;
    }
    if (UNEXPECTED(klen > SW_HTTP_HEADER_KEY_SIZE - 1)) {
        php_swoole_error(E_WARNING, "trailer key is too long");
        RETURN_FALSE;
    }
    zval *ztrailer = swoole_http_init_and_read_property(
        swoole_http_response_ce, ctx->response.zobject, &ctx->response.ztrailer, ZEND_STRL("trailer"));
    swoole_strlcpy(key_buf, k, sizeof(key_buf));
    swoole_strtolower(key_buf, klen);
    if (UNEXPECTED(!v)) {
        add_assoc_null_ex(ztrailer, key_buf, klen);
    } else {
        add_assoc_stringl_ex(ztrailer, key_buf, klen, v, vlen);
    }
    ctx->send_trailer_ = 1;
    RETURN_TRUE;
}

static PHP_METHOD(swoole_http_response, ping) {
    HttpContext *ctx = php_swoole_http_response_get_and_check_context(ZEND_THIS);
    if (UNEXPECTED(!ctx)) {
        RETURN_FALSE;
    }
    if (UNEXPECTED(!ctx->http2)) {
        php_swoole_fatal_error(E_WARNING, "fd[%ld] is not a HTTP2 conncetion", ctx->fd);
        RETURN_FALSE;
    }
    SW_CHECK_RETURN(swoole_http2_server_ping(ctx));
}

static PHP_METHOD(swoole_http_response, goaway) {
    HttpContext *ctx = php_swoole_http_response_get_and_check_context(ZEND_THIS);
    if (UNEXPECTED(!ctx)) {
        RETURN_FALSE;
    }
    if (UNEXPECTED(!ctx->http2)) {
        php_swoole_fatal_error(E_WARNING, "fd[%ld] is not a HTTP2 conncetion", ctx->fd);
        RETURN_FALSE;
    }

    zend_long error_code = SW_HTTP2_ERROR_NO_ERROR;
    char *debug_data = nullptr;
    size_t debug_data_len = 0;

    ZEND_PARSE_PARAMETERS_START(0, 2)
    Z_PARAM_OPTIONAL
    Z_PARAM_LONG(error_code)
    Z_PARAM_STRING(debug_data, debug_data_len)
    ZEND_PARSE_PARAMETERS_END_EX(RETURN_FALSE);

    SW_CHECK_RETURN(swoole_http2_server_goaway(ctx, error_code, debug_data, debug_data_len));
}

static PHP_METHOD(swoole_http_response, upgrade) {
    HttpContext *ctx = php_swoole_http_response_get_and_check_context(ZEND_THIS);
    if (UNEXPECTED(!ctx)) {
        RETURN_FALSE;
    }
    if (UNEXPECTED(!ctx->co_socket)) {
        php_swoole_fatal_error(E_WARNING, "async server dose not support protocol upgrade");
        RETURN_FALSE;
    }
    RETVAL_BOOL(swoole_websocket_handshake(ctx));
}

static PHP_METHOD(swoole_http_response, push) {
    HttpContext *ctx = php_swoole_http_response_get_context(ZEND_THIS);
    if (UNEXPECTED(!ctx)) {
        swoole_set_last_error(SW_ERROR_SESSION_CLOSED);
        RETURN_FALSE;
    }
    if (UNEXPECTED(!ctx->co_socket || !ctx->upgrade)) {
        php_swoole_fatal_error(E_WARNING, "fd[%ld] is not a websocket conncetion", ctx->fd);
        RETURN_FALSE;
    }

    zval *zdata;
    zend_long opcode = WebSocket::OPCODE_TEXT;
    zval *zflags = nullptr;
    zend_long flags = WebSocket::FLAG_FIN;

    ZEND_PARSE_PARAMETERS_START(1, 3)
    Z_PARAM_ZVAL(zdata)
    Z_PARAM_OPTIONAL
    Z_PARAM_LONG(opcode)
    Z_PARAM_ZVAL_EX(zflags, 1, 0)
    ZEND_PARSE_PARAMETERS_END_EX(RETURN_FALSE);

    if (zflags != nullptr) {
        flags = zval_get_long(zflags);
    }

    String *http_buffer = ctx->get_write_buffer();
    http_buffer->clear();
    if (php_swoole_websocket_frame_is_object(zdata)) {
        if (php_swoole_websocket_frame_object_pack(http_buffer, zdata, 0, ctx->websocket_compression) < 0) {
            RETURN_FALSE;
        }
    } else {
        if (php_swoole_websocket_frame_pack(
                http_buffer, zdata, opcode, flags & WebSocket::FLAGS_ALL, 0, ctx->websocket_compression) < 0) {
            RETURN_FALSE;
        }
    }
    RETURN_BOOL(ctx->send(ctx, http_buffer->str, http_buffer->length));
}

static PHP_METHOD(swoole_http_response, close) {
    HttpContext *ctx = php_swoole_http_response_get_context(ZEND_THIS);
    if (UNEXPECTED(!ctx)) {
        swoole_set_last_error(SW_ERROR_SESSION_CLOSED);
        RETURN_FALSE;
    }
    RETURN_BOOL(ctx->close(ctx));
}

static PHP_METHOD(swoole_http_response, recv) {
    HttpContext *ctx = php_swoole_http_response_get_context(ZEND_THIS);
    if (UNEXPECTED(!ctx)) {
        swoole_set_last_error(SW_ERROR_SESSION_CLOSED);
        RETURN_FALSE;
    }
    if (UNEXPECTED(!ctx->co_socket || !ctx->upgrade)) {
        php_swoole_fatal_error(E_WARNING, "fd[%ld] is not a websocket conncetion", ctx->fd);
        RETURN_FALSE;
    }

    double timeout = 0;

    ZEND_PARSE_PARAMETERS_START_EX(ZEND_PARSE_PARAMS_THROW, 0, 1)
    Z_PARAM_OPTIONAL
    Z_PARAM_DOUBLE(timeout)
    ZEND_PARSE_PARAMETERS_END_EX(RETURN_FALSE);

    Socket *sock = (Socket *) ctx->private_data;
    ssize_t retval = sock->recv_packet(timeout);
    String _tmp;

    if (retval < 0) {
        swoole_set_last_error(sock->errCode);
        RETURN_FALSE;
    } else if (retval == 0) {
        RETURN_EMPTY_STRING();
    } else {
        _tmp.str = sock->get_read_buffer()->str;
        _tmp.length = retval;

#ifdef SW_HAVE_ZLIB
        php_swoole_websocket_frame_unpack_ex(&_tmp, return_value, ctx->websocket_compression);
#else
        php_swoole_websocket_frame_unpack(&_tmp, return_value);
#endif
        zend_update_property_long(
            swoole_websocket_frame_ce, SW_Z8_OBJ_P(return_value), ZEND_STRL("fd"), sock->get_fd());
    }
}

static PHP_METHOD(swoole_http_response, detach) {
    HttpContext *ctx = php_swoole_http_response_get_and_check_context(ZEND_THIS);
    if (!ctx) {
        RETURN_FALSE;
    }
    ctx->detached = 1;
    RETURN_TRUE;
}

static PHP_METHOD(swoole_http_response, create) {
    zval *zobject = nullptr;
    zval *zrequest = nullptr;
    zend_long fd = -1;
    Server *serv = nullptr;
    Socket *sock = nullptr;
    HttpContext *ctx = nullptr;

    ZEND_PARSE_PARAMETERS_START(1, 2)
    Z_PARAM_ZVAL(zobject)
    Z_PARAM_OPTIONAL
    Z_PARAM_LONG(fd)
    ZEND_PARSE_PARAMETERS_END_EX(RETURN_FALSE);

    if (ZVAL_IS_OBJECT(zobject)) {
    _type_detect:
        if (instanceof_function(Z_OBJCE_P(zobject), swoole_server_ce)) {
            serv = php_swoole_server_get_and_check_server(zobject);
            if (serv->get_connection_verify(fd) == nullptr) {
                php_swoole_fatal_error(E_WARNING, "parameter $2 (%ld) must be valid connection session_id", (long) fd);
                RETURN_FALSE;
            }
        } else if (sw_zval_is_co_socket(zobject)) {
            sock = php_swoole_get_socket(zobject);
            fd = sock->get_fd();
        } else {
        _bad_type:
            php_swoole_fatal_error(E_WARNING, "parameter $1 must be instanceof Server or Coroutine\\Socket");
            RETURN_FALSE;
        }
    } else if (ZVAL_IS_ARRAY(zobject)) {
        zrequest = zend_hash_index_find(Z_ARR_P(zobject), 1);
        if (!ZVAL_IS_OBJECT(zrequest) || !instanceof_function(Z_OBJCE_P(zrequest), swoole_http_request_ce)) {
            php_swoole_fatal_error(E_WARNING, "parameter $1.second must be instanceof Http\\Request");
            RETURN_FALSE;
        }
        zobject = zend_hash_index_find(Z_ARR_P(zobject), 0);
        if (!ZVAL_IS_OBJECT(zobject)) {
            goto _bad_type;
        } else {
            ctx = php_swoole_http_request_get_context(zrequest);
            goto _type_detect;
        }
    } else {
        fd = zval_get_long(zobject);
        serv = sw_server();
    }

    if (serv && !serv->is_started()) {
        php_swoole_fatal_error(E_WARNING, "server is not running");
        RETURN_FALSE;
    }

    if (!ctx) {
        ctx = new HttpContext();
        ctx->keepalive = 1;

        if (serv) {
            ctx->init(serv);
        } else if (sock) {
            ctx->init(sock);
            swoole_llhttp_parser_init(&ctx->parser, HTTP_REQUEST, (void *) ctx);
        } else {
            delete ctx;
            assert(0);
            RETURN_FALSE;
        }
    } else {
        if (serv) {
            ctx->bind(serv);
        } else if (sock) {
            ctx->bind(sock);
        } else {
            assert(0);
            RETURN_FALSE;
        }
    }

    object_init_ex(return_value, swoole_http_response_ce);
    php_swoole_http_response_set_context(return_value, ctx);
    ctx->fd = fd;
    ctx->response.zobject = return_value;
    sw_copy_to_stack(ctx->response.zobject, ctx->response._zobject);
    zend_update_property_long(swoole_http_response_ce, SW_Z8_OBJ_P(return_value), ZEND_STRL("fd"), fd);
    if (ctx->co_socket) {
        zend_update_property_ex(
            swoole_http_response_ce, SW_Z8_OBJ_P(ctx->response.zobject), SW_ZSTR_KNOWN(SW_ZEND_STR_SOCKET), zobject);
    }
    if (zrequest) {
        zend_update_property_long(swoole_http_request_ce, SW_Z8_OBJ_P(ctx->request.zobject), ZEND_STRL("fd"), fd);
    }
}

static PHP_METHOD(swoole_http_response, redirect) {
    zval *zurl;
    zval *zhttp_code = nullptr;

    ZEND_PARSE_PARAMETERS_START(1, 2)
    Z_PARAM_ZVAL(zurl)
    Z_PARAM_OPTIONAL
    Z_PARAM_ZVAL_EX(zhttp_code, 1, 0)
    ZEND_PARSE_PARAMETERS_END_EX(RETURN_FALSE);

    HttpContext *ctx = php_swoole_http_response_get_and_check_context(ZEND_THIS);
    if (UNEXPECTED(!ctx)) {
        RETURN_FALSE;
    }

    // status
    if (zhttp_code) {
        ctx->response.status = zval_get_long(zhttp_code);
    } else {
        ctx->response.status = 302;
    }

    zval zkey;
    ZVAL_STRINGL(&zkey, "Location", 8);
    sw_zend_call_method_with_2_params(ZEND_THIS, nullptr, nullptr, "header", return_value, &zkey, zurl);
    zval_ptr_dtor(&zkey);
    if (!Z_BVAL_P(return_value)) {
        return;
    }
    ctx->end(nullptr, return_value);
}
