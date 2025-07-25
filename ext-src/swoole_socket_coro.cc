/*
 +----------------------------------------------------------------------+
 | Swoole                                                               |
 +----------------------------------------------------------------------+
 | Copyright (c) 2012-2018 The Swoole Group                             |
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

#include "php_swoole_cxx.h"
#include "php_swoole_thread.h"

#include "swoole_string.h"
#include "swoole_socket.h"
#include "swoole_util.h"
#include "swoole_protocol.h"
#include "swoole_mqtt.h"

#include "thirdparty/php/sockets/php_sockets_cxx.h"

#include <string>

BEGIN_EXTERN_C()
#include "stubs/php_swoole_socket_coro_arginfo.h"
END_EXTERN_C()

using swoole::HttpProxy;
using swoole::PacketLength;
using swoole::Protocol;
using swoole::Socks5Proxy;
using swoole::coroutine::Socket;
using swoole::network::Address;

zend_class_entry *swoole_socket_coro_ce;
static zend_object_handlers swoole_socket_coro_handlers;

static zend_class_entry *swoole_socket_coro_exception_ce;
static zend_object_handlers swoole_socket_coro_exception_handlers;

struct SocketObject {
    Socket *socket;
    zval zstream;
    bool reference;
    zend_object std;
};

SW_EXTERN_C_BEGIN
static PHP_METHOD(swoole_socket_coro, __construct);
static PHP_METHOD(swoole_socket_coro, bind);
static PHP_METHOD(swoole_socket_coro, listen);
static PHP_METHOD(swoole_socket_coro, accept);
static PHP_METHOD(swoole_socket_coro, connect);
static PHP_METHOD(swoole_socket_coro, checkLiveness);
static PHP_METHOD(swoole_socket_coro, getBoundCid);
static PHP_METHOD(swoole_socket_coro, peek);
static PHP_METHOD(swoole_socket_coro, recv);
static PHP_METHOD(swoole_socket_coro, send);
static PHP_METHOD(swoole_socket_coro, readVector);
static PHP_METHOD(swoole_socket_coro, readVectorAll);
static PHP_METHOD(swoole_socket_coro, writeVector);
static PHP_METHOD(swoole_socket_coro, writeVectorAll);
static PHP_METHOD(swoole_socket_coro, sendFile);
static PHP_METHOD(swoole_socket_coro, recvAll);
static PHP_METHOD(swoole_socket_coro, sendAll);
static PHP_METHOD(swoole_socket_coro, recvPacket);
static PHP_METHOD(swoole_socket_coro, recvLine);
static PHP_METHOD(swoole_socket_coro, recvWithBuffer);
static PHP_METHOD(swoole_socket_coro, recvfrom);
static PHP_METHOD(swoole_socket_coro, sendto);
static PHP_METHOD(swoole_socket_coro, getOption);
static PHP_METHOD(swoole_socket_coro, setOption);
static PHP_METHOD(swoole_socket_coro, setProtocol);
#ifdef SW_USE_OPENSSL
static PHP_METHOD(swoole_socket_coro, sslHandshake);
#endif
static PHP_METHOD(swoole_socket_coro, shutdown);
static PHP_METHOD(swoole_socket_coro, close);
static PHP_METHOD(swoole_socket_coro, cancel);
static PHP_METHOD(swoole_socket_coro, getsockname);
static PHP_METHOD(swoole_socket_coro, getpeername);
static PHP_METHOD(swoole_socket_coro, isClosed);
static PHP_METHOD(swoole_socket_coro, import);
SW_EXTERN_C_END

// clang-format off
static const zend_function_entry swoole_socket_coro_methods[] =
{
    PHP_ME(swoole_socket_coro, __construct,    arginfo_class_Swoole_Coroutine_Socket___construct,    ZEND_ACC_PUBLIC)
    PHP_ME(swoole_socket_coro, bind,           arginfo_class_Swoole_Coroutine_Socket_bind,           ZEND_ACC_PUBLIC)
    PHP_ME(swoole_socket_coro, listen,         arginfo_class_Swoole_Coroutine_Socket_listen,         ZEND_ACC_PUBLIC)
    PHP_ME(swoole_socket_coro, accept,         arginfo_class_Swoole_Coroutine_Socket_accept,         ZEND_ACC_PUBLIC)
    PHP_ME(swoole_socket_coro, connect,        arginfo_class_Swoole_Coroutine_Socket_connect,        ZEND_ACC_PUBLIC)
    PHP_ME(swoole_socket_coro, checkLiveness,  arginfo_class_Swoole_Coroutine_Socket_checkLiveness,  ZEND_ACC_PUBLIC)
    PHP_ME(swoole_socket_coro, getBoundCid,    arginfo_class_Swoole_Coroutine_Socket_getBoundCid,    ZEND_ACC_PUBLIC)
    PHP_ME(swoole_socket_coro, peek,           arginfo_class_Swoole_Coroutine_Socket_peek,           ZEND_ACC_PUBLIC)
    PHP_ME(swoole_socket_coro, recv,           arginfo_class_Swoole_Coroutine_Socket_recv,           ZEND_ACC_PUBLIC)
    PHP_ME(swoole_socket_coro, recvAll,        arginfo_class_Swoole_Coroutine_Socket_recvAll,        ZEND_ACC_PUBLIC)
    PHP_ME(swoole_socket_coro, recvLine,       arginfo_class_Swoole_Coroutine_Socket_recvLine,       ZEND_ACC_PUBLIC)
    PHP_ME(swoole_socket_coro, recvWithBuffer, arginfo_class_Swoole_Coroutine_Socket_recvWithBuffer, ZEND_ACC_PUBLIC)
    PHP_ME(swoole_socket_coro, recvPacket,     arginfo_class_Swoole_Coroutine_Socket_recvPacket,     ZEND_ACC_PUBLIC)
    PHP_ME(swoole_socket_coro, send,           arginfo_class_Swoole_Coroutine_Socket_send,           ZEND_ACC_PUBLIC)
    PHP_ME(swoole_socket_coro, readVector,     arginfo_class_Swoole_Coroutine_Socket_readVector,     ZEND_ACC_PUBLIC)
    PHP_ME(swoole_socket_coro, readVectorAll,  arginfo_class_Swoole_Coroutine_Socket_readVectorAll,  ZEND_ACC_PUBLIC)
    PHP_ME(swoole_socket_coro, writeVector,    arginfo_class_Swoole_Coroutine_Socket_writeVector,    ZEND_ACC_PUBLIC)
    PHP_ME(swoole_socket_coro, writeVectorAll, arginfo_class_Swoole_Coroutine_Socket_writeVectorAll, ZEND_ACC_PUBLIC)
    PHP_ME(swoole_socket_coro, sendFile,       arginfo_class_Swoole_Coroutine_Socket_sendFile,       ZEND_ACC_PUBLIC)
    PHP_ME(swoole_socket_coro, sendAll,        arginfo_class_Swoole_Coroutine_Socket_sendAll,        ZEND_ACC_PUBLIC)
    PHP_ME(swoole_socket_coro, recvfrom,       arginfo_class_Swoole_Coroutine_Socket_recvfrom,       ZEND_ACC_PUBLIC)
    PHP_ME(swoole_socket_coro, sendto,         arginfo_class_Swoole_Coroutine_Socket_sendto,         ZEND_ACC_PUBLIC)
    PHP_ME(swoole_socket_coro, getOption,      arginfo_class_Swoole_Coroutine_Socket_getOption,      ZEND_ACC_PUBLIC)
    PHP_ME(swoole_socket_coro, setProtocol,    arginfo_class_Swoole_Coroutine_Socket_setProtocol,    ZEND_ACC_PUBLIC)
    PHP_ME(swoole_socket_coro, setOption,      arginfo_class_Swoole_Coroutine_Socket_setOption,      ZEND_ACC_PUBLIC)
#ifdef SW_USE_OPENSSL
    PHP_ME(swoole_socket_coro, sslHandshake,  arginfo_class_Swoole_Coroutine_Socket_sslHandshake,    ZEND_ACC_PUBLIC)
#endif
    PHP_ME(swoole_socket_coro, shutdown,      arginfo_class_Swoole_Coroutine_Socket_shutdown,        ZEND_ACC_PUBLIC)
    PHP_ME(swoole_socket_coro, cancel,        arginfo_class_Swoole_Coroutine_Socket_cancel,          ZEND_ACC_PUBLIC)
    PHP_ME(swoole_socket_coro, close,         arginfo_class_Swoole_Coroutine_Socket_close,           ZEND_ACC_PUBLIC)
    PHP_ME(swoole_socket_coro, getpeername,   arginfo_class_Swoole_Coroutine_Socket_getpeername,     ZEND_ACC_PUBLIC)
    PHP_ME(swoole_socket_coro, getsockname,   arginfo_class_Swoole_Coroutine_Socket_getsockname,     ZEND_ACC_PUBLIC)
    PHP_ME(swoole_socket_coro, isClosed,      arginfo_class_Swoole_Coroutine_Socket_isClosed,        ZEND_ACC_PUBLIC)
    PHP_ME(swoole_socket_coro, import,        arginfo_class_Swoole_Coroutine_Socket_import,          ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
    PHP_FE_END
};
// clang-format on

#define swoole_get_socket_coro(_sock, _zobject)                                                                        \
    SocketObject *_sock = socket_coro_fetch_object(Z_OBJ_P(_zobject));                                                 \
    if (UNEXPECTED(!sock->socket)) {                                                                                   \
        swoole_fatal_error(SW_ERROR_WRONG_OPERATION, "must call constructor first");                                   \
    }                                                                                                                  \
    if (UNEXPECTED(_sock->socket->is_closed())) {                                                                      \
        zend_update_property_long(swoole_socket_coro_ce, SW_Z8_OBJ_P(_zobject), ZEND_STRL("errCode"), EBADF);          \
        zend_update_property_string(                                                                                   \
            swoole_socket_coro_ce, SW_Z8_OBJ_P(_zobject), ZEND_STRL("errMsg"), strerror(EBADF));                       \
        RETURN_FALSE;                                                                                                  \
    }

static sw_inline SocketObject *socket_coro_fetch_object(zend_object *obj) {
    return reinterpret_cast<SocketObject *>(reinterpret_cast<char *>(obj) - swoole_socket_coro_handlers.offset);
}

/**
 * cannot execute close in the destructor, it may be shutting down,
 * executing close will try to resume other coroutines.
 */
static void socket_coro_free_object(zend_object *object) {
    auto *sock = socket_coro_fetch_object(object);
    if (!sock->reference && sock->socket) {
        if (!Z_ISUNDEF(sock->zstream)) {
            sock->socket->move_fd();
            zval_ptr_dtor(&sock->zstream);
        }
        delete sock->socket;
    }
    zend_object_std_dtor(&sock->std);
}

static zend_object *socket_coro_create_object(zend_class_entry *ce) {
    auto *sock = static_cast<SocketObject *>(zend_object_alloc(sizeof(SocketObject), ce));
    zend_object_std_init(&sock->std, ce);
    /* Even if you don't use properties yourself you should still call object_properties_init(),
     * because extending classes may use properties. (Generally a lot of the stuff you will do is
     * for the sake of not breaking extending classes). */
    object_properties_init(&sock->std, ce);
    sock->std.handlers = &swoole_socket_coro_handlers;
    return &sock->std;
}

static void socket_coro_register_constants(int module_number) {
    REGISTER_LONG_CONSTANT("AF_UNIX", AF_UNIX, CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("AF_INET", AF_INET, CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("AF_INET6", AF_INET6, CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("SOCK_STREAM", SOCK_STREAM, CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("SOCK_DGRAM", SOCK_DGRAM, CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("SOCK_RAW", SOCK_RAW, CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("SOCK_SEQPACKET", SOCK_SEQPACKET, CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("SOCK_RDM", SOCK_RDM, CONST_CS | CONST_PERSISTENT);

    REGISTER_LONG_CONSTANT("MSG_OOB", MSG_OOB, CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("MSG_WAITALL", MSG_WAITALL, CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("MSG_CTRUNC", MSG_CTRUNC, CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("MSG_TRUNC", MSG_TRUNC, CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("MSG_PEEK", MSG_PEEK, CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("MSG_DONTROUTE", MSG_DONTROUTE, CONST_CS | CONST_PERSISTENT);
#ifdef MSG_EOR
    REGISTER_LONG_CONSTANT("MSG_EOR", MSG_EOR, CONST_CS | CONST_PERSISTENT);
#endif
#ifdef MSG_EOF
    REGISTER_LONG_CONSTANT("MSG_EOF", MSG_EOF, CONST_CS | CONST_PERSISTENT);
#endif

#ifdef MSG_CONFIRM
    REGISTER_LONG_CONSTANT("MSG_CONFIRM", MSG_CONFIRM, CONST_CS | CONST_PERSISTENT);
#endif
#ifdef MSG_ERRQUEUE
    REGISTER_LONG_CONSTANT("MSG_ERRQUEUE", MSG_ERRQUEUE, CONST_CS | CONST_PERSISTENT);
#endif
#ifdef MSG_NOSIGNAL
    REGISTER_LONG_CONSTANT("MSG_NOSIGNAL", MSG_NOSIGNAL, CONST_CS | CONST_PERSISTENT);
#endif
#ifdef MSG_DONTWAIT
    REGISTER_LONG_CONSTANT("MSG_DONTWAIT", MSG_DONTWAIT, CONST_CS | CONST_PERSISTENT);
#endif
#ifdef MSG_MORE
    REGISTER_LONG_CONSTANT("MSG_MORE", MSG_MORE, CONST_CS | CONST_PERSISTENT);
#endif
#ifdef MSG_WAITFORONE
    REGISTER_LONG_CONSTANT("MSG_WAITFORONE", MSG_WAITFORONE, CONST_CS | CONST_PERSISTENT);
#endif
#ifdef MSG_CMSG_CLOEXEC
    REGISTER_LONG_CONSTANT("MSG_CMSG_CLOEXEC", MSG_CMSG_CLOEXEC, CONST_CS | CONST_PERSISTENT);
#endif

    REGISTER_LONG_CONSTANT("SO_DEBUG", SO_DEBUG, CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("SO_REUSEADDR", SO_REUSEADDR, CONST_CS | CONST_PERSISTENT);
#ifdef SO_REUSEPORT
    REGISTER_LONG_CONSTANT("SO_REUSEPORT", SO_REUSEPORT, CONST_CS | CONST_PERSISTENT);
#endif
    REGISTER_LONG_CONSTANT("SO_KEEPALIVE", SO_KEEPALIVE, CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("SO_DONTROUTE", SO_DONTROUTE, CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("SO_LINGER", SO_LINGER, CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("SO_BROADCAST", SO_BROADCAST, CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("SO_OOBINLINE", SO_OOBINLINE, CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("SO_SNDBUF", SO_SNDBUF, CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("SO_RCVBUF", SO_RCVBUF, CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("SO_SNDLOWAT", SO_SNDLOWAT, CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("SO_RCVLOWAT", SO_RCVLOWAT, CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("SO_SNDTIMEO", SO_SNDTIMEO, CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("SO_RCVTIMEO", SO_RCVTIMEO, CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("SO_TYPE", SO_TYPE, CONST_CS | CONST_PERSISTENT);
#ifdef SO_FAMILY
    REGISTER_LONG_CONSTANT("SO_FAMILY", SO_FAMILY, CONST_CS | CONST_PERSISTENT);
#endif
    REGISTER_LONG_CONSTANT("SO_ERROR", SO_ERROR, CONST_CS | CONST_PERSISTENT);
#ifdef SO_BINDTODEVICE
    REGISTER_LONG_CONSTANT("SO_BINDTODEVICE", SO_BINDTODEVICE, CONST_CS | CONST_PERSISTENT);
#endif
#ifdef SO_USER_COOKIE
    REGISTER_LONG_CONSTANT("SO_LABEL", SO_LABEL, CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("SO_PEERLABEL", SO_PEERLABEL, CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("SO_LISTENQLIMIT", SO_LISTENQLIMIT, CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("SO_LISTENQLEN", SO_LISTENQLEN, CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("SO_USER_COOKIE", SO_USER_COOKIE, CONST_CS | CONST_PERSISTENT);
#endif
    REGISTER_LONG_CONSTANT("SOL_SOCKET", SOL_SOCKET, CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("SOMAXCONN", SOMAXCONN, CONST_CS | CONST_PERSISTENT);
#ifdef TCP_NODELAY
    REGISTER_LONG_CONSTANT("TCP_NODELAY", TCP_NODELAY, CONST_CS | CONST_PERSISTENT);
#endif

    REGISTER_LONG_CONSTANT("MCAST_JOIN_GROUP", PHP_MCAST_JOIN_GROUP, CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("MCAST_LEAVE_GROUP", PHP_MCAST_LEAVE_GROUP, CONST_CS | CONST_PERSISTENT);
#ifdef HAS_MCAST_EXT
    REGISTER_LONG_CONSTANT("MCAST_BLOCK_SOURCE", PHP_MCAST_BLOCK_SOURCE, CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("MCAST_UNBLOCK_SOURCE", PHP_MCAST_UNBLOCK_SOURCE, CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("MCAST_JOIN_SOURCE_GROUP", PHP_MCAST_JOIN_SOURCE_GROUP, CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("MCAST_LEAVE_SOURCE_GROUP", PHP_MCAST_LEAVE_SOURCE_GROUP, CONST_CS | CONST_PERSISTENT);
#endif

    REGISTER_LONG_CONSTANT("IP_MULTICAST_IF", IP_MULTICAST_IF, CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("IP_MULTICAST_TTL", IP_MULTICAST_TTL, CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("IP_MULTICAST_LOOP", IP_MULTICAST_LOOP, CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("IPV6_MULTICAST_IF", IPV6_MULTICAST_IF, CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("IPV6_MULTICAST_HOPS", IPV6_MULTICAST_HOPS, CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("IPV6_MULTICAST_LOOP", IPV6_MULTICAST_LOOP, CONST_CS | CONST_PERSISTENT);

#ifdef IPV6_V6ONLY
    REGISTER_LONG_CONSTANT("IPV6_V6ONLY", IPV6_V6ONLY, CONST_CS | CONST_PERSISTENT);
#endif

#ifdef EPERM
    /* Operation not permitted */
    REGISTER_LONG_CONSTANT("SOCKET_EPERM", EPERM, CONST_CS | CONST_PERSISTENT);
#endif
#ifdef ENOENT
    /* No such file or directory */
    REGISTER_LONG_CONSTANT("SOCKET_ENOENT", ENOENT, CONST_CS | CONST_PERSISTENT);
#endif
#ifdef EINTR
    /* Interrupted system call */
    REGISTER_LONG_CONSTANT("SOCKET_EINTR", EINTR, CONST_CS | CONST_PERSISTENT);
#endif
#ifdef EIO
    /* I/O error */
    REGISTER_LONG_CONSTANT("SOCKET_EIO", EIO, CONST_CS | CONST_PERSISTENT);
#endif
#ifdef ENXIO
    /* No such device or address */
    REGISTER_LONG_CONSTANT("SOCKET_ENXIO", ENXIO, CONST_CS | CONST_PERSISTENT);
#endif
#ifdef E2BIG
    /* Arg list too long */
    REGISTER_LONG_CONSTANT("SOCKET_E2BIG", E2BIG, CONST_CS | CONST_PERSISTENT);
#endif
#ifdef EBADF
    /* Bad file number */
    REGISTER_LONG_CONSTANT("SOCKET_EBADF", EBADF, CONST_CS | CONST_PERSISTENT);
#endif
#ifdef EAGAIN
    /* Try again */
    REGISTER_LONG_CONSTANT("SOCKET_EAGAIN", EAGAIN, CONST_CS | CONST_PERSISTENT);
#endif
#ifdef ENOMEM
    /* Out of memory */
    REGISTER_LONG_CONSTANT("SOCKET_ENOMEM", ENOMEM, CONST_CS | CONST_PERSISTENT);
#endif
#ifdef EACCES
    /* Permission denied */
    REGISTER_LONG_CONSTANT("SOCKET_EACCES", EACCES, CONST_CS | CONST_PERSISTENT);
#endif
#ifdef EFAULT
    /* Bad address */
    REGISTER_LONG_CONSTANT("SOCKET_EFAULT", EFAULT, CONST_CS | CONST_PERSISTENT);
#endif
#ifdef ENOTBLK
    /* Block device required */
    REGISTER_LONG_CONSTANT("SOCKET_ENOTBLK", ENOTBLK, CONST_CS | CONST_PERSISTENT);
#endif
#ifdef EBUSY
    /* Device or resource busy */
    REGISTER_LONG_CONSTANT("SOCKET_EBUSY", EBUSY, CONST_CS | CONST_PERSISTENT);
#endif
#ifdef EEXIST
    /* File exists */
    REGISTER_LONG_CONSTANT("SOCKET_EEXIST", EEXIST, CONST_CS | CONST_PERSISTENT);
#endif
#ifdef EXDEV
    /* Cross-device link */
    REGISTER_LONG_CONSTANT("SOCKET_EXDEV", EXDEV, CONST_CS | CONST_PERSISTENT);
#endif
#ifdef ENODEV
    /* No such device */
    REGISTER_LONG_CONSTANT("SOCKET_ENODEV", ENODEV, CONST_CS | CONST_PERSISTENT);
#endif
#ifdef ENOTDIR
    /* Not a directory */
    REGISTER_LONG_CONSTANT("SOCKET_ENOTDIR", ENOTDIR, CONST_CS | CONST_PERSISTENT);
#endif
#ifdef EISDIR
    /* Is a directory */
    REGISTER_LONG_CONSTANT("SOCKET_EISDIR", EISDIR, CONST_CS | CONST_PERSISTENT);
#endif
#ifdef EINVAL
    /* Invalid argument */
    REGISTER_LONG_CONSTANT("SOCKET_EINVAL", EINVAL, CONST_CS | CONST_PERSISTENT);
#endif
#ifdef ENFILE
    /* File table overflow */
    REGISTER_LONG_CONSTANT("SOCKET_ENFILE", ENFILE, CONST_CS | CONST_PERSISTENT);
#endif
#ifdef EMFILE
    /* Too many open files */
    REGISTER_LONG_CONSTANT("SOCKET_EMFILE", EMFILE, CONST_CS | CONST_PERSISTENT);
#endif
#ifdef ENOTTY
    /* Not a typewriter */
    REGISTER_LONG_CONSTANT("SOCKET_ENOTTY", ENOTTY, CONST_CS | CONST_PERSISTENT);
#endif
#ifdef ENOSPC
    /* No space left on device */
    REGISTER_LONG_CONSTANT("SOCKET_ENOSPC", ENOSPC, CONST_CS | CONST_PERSISTENT);
#endif
#ifdef ESPIPE
    /* Illegal seek */
    REGISTER_LONG_CONSTANT("SOCKET_ESPIPE", ESPIPE, CONST_CS | CONST_PERSISTENT);
#endif
#ifdef EROFS
    /* Read-only file system */
    REGISTER_LONG_CONSTANT("SOCKET_EROFS", EROFS, CONST_CS | CONST_PERSISTENT);
#endif
#ifdef EMLINK
    /* Too many links */
    REGISTER_LONG_CONSTANT("SOCKET_EMLINK", EMLINK, CONST_CS | CONST_PERSISTENT);
#endif
#ifdef EPIPE
    /* Broken pipe */
    REGISTER_LONG_CONSTANT("SOCKET_EPIPE", EPIPE, CONST_CS | CONST_PERSISTENT);
#endif
#ifdef ENAMETOOLONG
    /* File name too long */
    REGISTER_LONG_CONSTANT("SOCKET_ENAMETOOLONG", ENAMETOOLONG, CONST_CS | CONST_PERSISTENT);
#endif
#ifdef ENOLCK
    /* No record locks available */
    REGISTER_LONG_CONSTANT("SOCKET_ENOLCK", ENOLCK, CONST_CS | CONST_PERSISTENT);
#endif
#ifdef ENOSYS
    /* Function not implemented */
    REGISTER_LONG_CONSTANT("SOCKET_ENOSYS", ENOSYS, CONST_CS | CONST_PERSISTENT);
#endif
#ifdef ENOTEMPTY
    /* Directory not empty */
    REGISTER_LONG_CONSTANT("SOCKET_ENOTEMPTY", ENOTEMPTY, CONST_CS | CONST_PERSISTENT);
#endif
#ifdef ELOOP
    /* Too many symbolic links encountered */
    REGISTER_LONG_CONSTANT("SOCKET_ELOOP", ELOOP, CONST_CS | CONST_PERSISTENT);
#endif
#ifdef EWOULDBLOCK
    /* Operation would block */
    REGISTER_LONG_CONSTANT("SOCKET_EWOULDBLOCK", EWOULDBLOCK, CONST_CS | CONST_PERSISTENT);
#endif
#ifdef ENOMSG
    /* No message of desired type */
    REGISTER_LONG_CONSTANT("SOCKET_ENOMSG", ENOMSG, CONST_CS | CONST_PERSISTENT);
#endif
#ifdef EIDRM
    /* Identifier removed */
    REGISTER_LONG_CONSTANT("SOCKET_EIDRM", EIDRM, CONST_CS | CONST_PERSISTENT);
#endif
#ifdef ECHRNG
    /* Channel number out of range */
    REGISTER_LONG_CONSTANT("SOCKET_ECHRNG", ECHRNG, CONST_CS | CONST_PERSISTENT);
#endif
#ifdef EL2NSYNC
    /* Level 2 not synchronized */
    REGISTER_LONG_CONSTANT("SOCKET_EL2NSYNC", EL2NSYNC, CONST_CS | CONST_PERSISTENT);
#endif
#ifdef EL3HLT
    /* Level 3 halted */
    REGISTER_LONG_CONSTANT("SOCKET_EL3HLT", EL3HLT, CONST_CS | CONST_PERSISTENT);
#endif
#ifdef EL3RST
    /* Level 3 reset */
    REGISTER_LONG_CONSTANT("SOCKET_EL3RST", EL3RST, CONST_CS | CONST_PERSISTENT);
#endif
#ifdef ELNRNG
    /* Link number out of range */
    REGISTER_LONG_CONSTANT("SOCKET_ELNRNG", ELNRNG, CONST_CS | CONST_PERSISTENT);
#endif
#ifdef EUNATCH
    /* Protocol driver not attached */
    REGISTER_LONG_CONSTANT("SOCKET_EUNATCH", EUNATCH, CONST_CS | CONST_PERSISTENT);
#endif
#ifdef ENOCSI
    /* No CSI structure available */
    REGISTER_LONG_CONSTANT("SOCKET_ENOCSI", ENOCSI, CONST_CS | CONST_PERSISTENT);
#endif
#ifdef EL2HLT
    /* Level 2 halted */
    REGISTER_LONG_CONSTANT("SOCKET_EL2HLT", EL2HLT, CONST_CS | CONST_PERSISTENT);
#endif
#ifdef EBADE
    /* Invalid exchange */
    REGISTER_LONG_CONSTANT("SOCKET_EBADE", EBADE, CONST_CS | CONST_PERSISTENT);
#endif
#ifdef EBADR
    /* Invalid request descriptor */
    REGISTER_LONG_CONSTANT("SOCKET_EBADR", EBADR, CONST_CS | CONST_PERSISTENT);
#endif
#ifdef EXFULL
    /* Exchange full */
    REGISTER_LONG_CONSTANT("SOCKET_EXFULL", EXFULL, CONST_CS | CONST_PERSISTENT);
#endif
#ifdef ENOANO
    /* No anode */
    REGISTER_LONG_CONSTANT("SOCKET_ENOANO", ENOANO, CONST_CS | CONST_PERSISTENT);
#endif
#ifdef EBADRQC
    /* Invalid request code */
    REGISTER_LONG_CONSTANT("SOCKET_EBADRQC", EBADRQC, CONST_CS | CONST_PERSISTENT);
#endif
#ifdef EBADSLT
    /* Invalid slot */
    REGISTER_LONG_CONSTANT("SOCKET_EBADSLT", EBADSLT, CONST_CS | CONST_PERSISTENT);
#endif
#ifdef ENOSTR
    /* Device not a stream */
    REGISTER_LONG_CONSTANT("SOCKET_ENOSTR", ENOSTR, CONST_CS | CONST_PERSISTENT);
#endif
#ifdef ENODATA
    /* No data available */
    REGISTER_LONG_CONSTANT("SOCKET_ENODATA", ENODATA, CONST_CS | CONST_PERSISTENT);
#endif
#ifdef ETIME
    /* Timer expired */
    REGISTER_LONG_CONSTANT("SOCKET_ETIME", ETIME, CONST_CS | CONST_PERSISTENT);
#endif
#ifdef ENOSR
    /* Out of streams resources */
    REGISTER_LONG_CONSTANT("SOCKET_ENOSR", ENOSR, CONST_CS | CONST_PERSISTENT);
#endif
#ifdef ENONET
    /* Machine is not on the network */
    REGISTER_LONG_CONSTANT("SOCKET_ENONET", ENONET, CONST_CS | CONST_PERSISTENT);
#endif
#ifdef EREMOTE
    /* Object is remote */
    REGISTER_LONG_CONSTANT("SOCKET_EREMOTE", EREMOTE, CONST_CS | CONST_PERSISTENT);
#endif
#ifdef ENOLINK
    /* Link has been severed */
    REGISTER_LONG_CONSTANT("SOCKET_ENOLINK", ENOLINK, CONST_CS | CONST_PERSISTENT);
#endif
#ifdef EADV
    /* Advertise error */
    REGISTER_LONG_CONSTANT("SOCKET_EADV", EADV, CONST_CS | CONST_PERSISTENT);
#endif
#ifdef ESRMNT
    /* Srmount error */
    REGISTER_LONG_CONSTANT("SOCKET_ESRMNT", ESRMNT, CONST_CS | CONST_PERSISTENT);
#endif
#ifdef ECOMM
    /* Communication error on send */
    REGISTER_LONG_CONSTANT("SOCKET_ECOMM", ECOMM, CONST_CS | CONST_PERSISTENT);
#endif
#ifdef EPROTO
    /* Protocol error */
    REGISTER_LONG_CONSTANT("SOCKET_EPROTO", EPROTO, CONST_CS | CONST_PERSISTENT);
#endif
#ifdef EMULTIHOP
    /* Multihop attempted */
    REGISTER_LONG_CONSTANT("SOCKET_EMULTIHOP", EMULTIHOP, CONST_CS | CONST_PERSISTENT);
#endif
#ifdef EBADMSG
    /* Not a data message */
    REGISTER_LONG_CONSTANT("SOCKET_EBADMSG", EBADMSG, CONST_CS | CONST_PERSISTENT);
#endif
#ifdef ENOTUNIQ
    /* Name not unique on network */
    REGISTER_LONG_CONSTANT("SOCKET_ENOTUNIQ", ENOTUNIQ, CONST_CS | CONST_PERSISTENT);
#endif
#ifdef EBADFD
    /* File descriptor in bad state */
    REGISTER_LONG_CONSTANT("SOCKET_EBADFD", EBADFD, CONST_CS | CONST_PERSISTENT);
#endif
#ifdef EREMCHG
    /* Remote address changed */
    REGISTER_LONG_CONSTANT("SOCKET_EREMCHG", EREMCHG, CONST_CS | CONST_PERSISTENT);
#endif
#ifdef ERESTART
    /* Interrupted system call should be restarted */
    REGISTER_LONG_CONSTANT("SOCKET_ERESTART", ERESTART, CONST_CS | CONST_PERSISTENT);
#endif
#ifdef ESTRPIPE
    /* Streams pipe error */
    REGISTER_LONG_CONSTANT("SOCKET_ESTRPIPE", ESTRPIPE, CONST_CS | CONST_PERSISTENT);
#endif
#ifdef EUSERS
    /* Too many users */
    REGISTER_LONG_CONSTANT("SOCKET_EUSERS", EUSERS, CONST_CS | CONST_PERSISTENT);
#endif
#ifdef ENOTSOCK
    /* Socket operation on non-socket */
    REGISTER_LONG_CONSTANT("SOCKET_ENOTSOCK", ENOTSOCK, CONST_CS | CONST_PERSISTENT);
#endif
#ifdef EDESTADDRREQ
    /* Destination address required */
    REGISTER_LONG_CONSTANT("SOCKET_EDESTADDRREQ", EDESTADDRREQ, CONST_CS | CONST_PERSISTENT);
#endif
#ifdef EMSGSIZE
    /* Message too long */
    REGISTER_LONG_CONSTANT("SOCKET_EMSGSIZE", EMSGSIZE, CONST_CS | CONST_PERSISTENT);
#endif
#ifdef EPROTOTYPE
    /* Protocol wrong type for socket */
    REGISTER_LONG_CONSTANT("SOCKET_EPROTOTYPE", EPROTOTYPE, CONST_CS | CONST_PERSISTENT);
#endif
#ifdef ENOPROTOOPT
    /* Protocol not available */
    REGISTER_LONG_CONSTANT("SOCKET_ENOPROTOOPT", ENOPROTOOPT, CONST_CS | CONST_PERSISTENT);
#endif
#ifdef EPROTONOSUPPORT
    /* Protocol not supported */
    REGISTER_LONG_CONSTANT("SOCKET_EPROTONOSUPPORT", EPROTONOSUPPORT, CONST_CS | CONST_PERSISTENT);
#endif
#ifdef ESOCKTNOSUPPORT
    /* Socket type not supported */
    REGISTER_LONG_CONSTANT("SOCKET_ESOCKTNOSUPPORT", ESOCKTNOSUPPORT, CONST_CS | CONST_PERSISTENT);
#endif
#ifdef EOPNOTSUPP
    /* Operation not supported on transport endpoint */
    REGISTER_LONG_CONSTANT("SOCKET_EOPNOTSUPP", EOPNOTSUPP, CONST_CS | CONST_PERSISTENT);
#endif
#ifdef EPFNOSUPPORT
    /* Protocol family not supported */
    REGISTER_LONG_CONSTANT("SOCKET_EPFNOSUPPORT", EPFNOSUPPORT, CONST_CS | CONST_PERSISTENT);
#endif
#ifdef EAFNOSUPPORT
    /* Address family not supported by protocol */
    REGISTER_LONG_CONSTANT("SOCKET_EAFNOSUPPORT", EAFNOSUPPORT, CONST_CS | CONST_PERSISTENT);
#endif
#ifdef EADDRINUSE
    /* Address already in use */
    REGISTER_LONG_CONSTANT("SOCKET_EADDRINUSE", EADDRINUSE, CONST_CS | CONST_PERSISTENT);
#endif
#ifdef EADDRNOTAVAIL
    /* Cannot assign requested address */
    REGISTER_LONG_CONSTANT("SOCKET_EADDRNOTAVAIL", EADDRNOTAVAIL, CONST_CS | CONST_PERSISTENT);
#endif
#ifdef ENETDOWN
    /* Network is down */
    REGISTER_LONG_CONSTANT("SOCKET_ENETDOWN", ENETDOWN, CONST_CS | CONST_PERSISTENT);
#endif
#ifdef ENETUNREACH
    /* Network is unreachable */
    REGISTER_LONG_CONSTANT("SOCKET_ENETUNREACH", ENETUNREACH, CONST_CS | CONST_PERSISTENT);
#endif
#ifdef ENETRESET
    /* Network dropped connection because of reset */
    REGISTER_LONG_CONSTANT("SOCKET_ENETRESET", ENETRESET, CONST_CS | CONST_PERSISTENT);
#endif
#ifdef ECONNABORTED
    /* Software caused connection abort */
    REGISTER_LONG_CONSTANT("SOCKET_ECONNABORTED", ECONNABORTED, CONST_CS | CONST_PERSISTENT);
#endif
#ifdef ECONNRESET
    /* Connection reset by peer */
    REGISTER_LONG_CONSTANT("SOCKET_ECONNRESET", ECONNRESET, CONST_CS | CONST_PERSISTENT);
#endif
#ifdef ENOBUFS
    /* No buffer space available */
    REGISTER_LONG_CONSTANT("SOCKET_ENOBUFS", ENOBUFS, CONST_CS | CONST_PERSISTENT);
#endif
#ifdef EISCONN
    /* Transport endpoint is already connected */
    REGISTER_LONG_CONSTANT("SOCKET_EISCONN", EISCONN, CONST_CS | CONST_PERSISTENT);
#endif
#ifdef ENOTCONN
    /* Transport endpoint is not connected */
    REGISTER_LONG_CONSTANT("SOCKET_ENOTCONN", ENOTCONN, CONST_CS | CONST_PERSISTENT);
#endif
#ifdef ESHUTDOWN
    /* Cannot send after transport endpoint shutdown */
    REGISTER_LONG_CONSTANT("SOCKET_ESHUTDOWN", ESHUTDOWN, CONST_CS | CONST_PERSISTENT);
#endif
#ifdef ETOOMANYREFS
    /* Too many references: cannot splice */
    REGISTER_LONG_CONSTANT("SOCKET_ETOOMANYREFS", ETOOMANYREFS, CONST_CS | CONST_PERSISTENT);
#endif
#ifdef ETIMEDOUT
    /* Connection timed out */
    REGISTER_LONG_CONSTANT("SOCKET_ETIMEDOUT", ETIMEDOUT, CONST_CS | CONST_PERSISTENT);
#endif
#ifdef ECONNREFUSED
    /* Connection refused */
    REGISTER_LONG_CONSTANT("SOCKET_ECONNREFUSED", ECONNREFUSED, CONST_CS | CONST_PERSISTENT);
#endif
#ifdef EHOSTDOWN
    /* Host is down */
    REGISTER_LONG_CONSTANT("SOCKET_EHOSTDOWN", EHOSTDOWN, CONST_CS | CONST_PERSISTENT);
#endif
#ifdef EHOSTUNREACH
    /* No route to host */
    REGISTER_LONG_CONSTANT("SOCKET_EHOSTUNREACH", EHOSTUNREACH, CONST_CS | CONST_PERSISTENT);
#endif
#ifdef EALREADY
    /* Operation already in progress */
    REGISTER_LONG_CONSTANT("SOCKET_EALREADY", EALREADY, CONST_CS | CONST_PERSISTENT);
#endif
#ifdef EINPROGRESS
    /* Operation now in progress */
    REGISTER_LONG_CONSTANT("SOCKET_EINPROGRESS", EINPROGRESS, CONST_CS | CONST_PERSISTENT);
#endif
#ifdef EISNAM
    /* Is a named type file */
    REGISTER_LONG_CONSTANT("SOCKET_EISNAM", EISNAM, CONST_CS | CONST_PERSISTENT);
#endif
#ifdef EREMOTEIO
    /* Remote I/O error */
    REGISTER_LONG_CONSTANT("SOCKET_EREMOTEIO", EREMOTEIO, CONST_CS | CONST_PERSISTENT);
#endif
#ifdef EDQUOT
    /* Quota exceeded */
    REGISTER_LONG_CONSTANT("SOCKET_EDQUOT", EDQUOT, CONST_CS | CONST_PERSISTENT);
#endif
#ifdef ENOMEDIUM
    /* No medium found */
    REGISTER_LONG_CONSTANT("SOCKET_ENOMEDIUM", ENOMEDIUM, CONST_CS | CONST_PERSISTENT);
#endif
#ifdef EMEDIUMTYPE
    /* Wrong medium type */
    REGISTER_LONG_CONSTANT("SOCKET_EMEDIUMTYPE", EMEDIUMTYPE, CONST_CS | CONST_PERSISTENT);
#endif

    REGISTER_LONG_CONSTANT("IPPROTO_IP", IPPROTO_IP, CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("IPPROTO_IPV6", IPPROTO_IPV6, CONST_CS | CONST_PERSISTENT);

    REGISTER_LONG_CONSTANT("SOL_TCP", IPPROTO_TCP, CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("SOL_UDP", IPPROTO_UDP, CONST_CS | CONST_PERSISTENT);

    REGISTER_LONG_CONSTANT("IPV6_UNICAST_HOPS", IPV6_UNICAST_HOPS, CONST_CS | CONST_PERSISTENT);

    REGISTER_LONG_CONSTANT("AI_PASSIVE", AI_PASSIVE, CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("AI_CANONNAME", AI_CANONNAME, CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("AI_NUMERICHOST", AI_NUMERICHOST, CONST_CS | CONST_PERSISTENT);
#if HAVE_AI_V4MAPPED
    REGISTER_LONG_CONSTANT("AI_V4MAPPED", AI_V4MAPPED, CONST_CS | CONST_PERSISTENT);
#endif
#if HAVE_AI_ALL
    REGISTER_LONG_CONSTANT("AI_ALL", AI_ALL, CONST_CS | CONST_PERSISTENT);
#endif
    REGISTER_LONG_CONSTANT("AI_ADDRCONFIG", AI_ADDRCONFIG, CONST_CS | CONST_PERSISTENT);
#if HAVE_AI_IDN
    REGISTER_LONG_CONSTANT("AI_IDN", AI_IDN, CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("AI_CANONIDN", AI_CANONIDN, CONST_CS | CONST_PERSISTENT);
    // REGISTER_LONG_CONSTANT("AI_IDN_ALLOW_UNASSIGNED", AI_IDN_ALLOW_UNASSIGNED, CONST_CS | CONST_PERSISTENT);
    // REGISTER_LONG_CONSTANT("AI_IDN_USE_STD3_ASCII_RULES", AI_IDN_USE_STD3_ASCII_RULES, CONST_CS | CONST_PERSISTENT);
#endif
#ifdef AI_NUMERICSERV
    REGISTER_LONG_CONSTANT("AI_NUMERICSERV", AI_NUMERICSERV, CONST_CS | CONST_PERSISTENT);
#endif
}

void php_swoole_socket_coro_minit(int module_number) {
    SW_INIT_CLASS_ENTRY(swoole_socket_coro, "Swoole\\Coroutine\\Socket", "Co\\Socket", swoole_socket_coro_methods);
    SW_SET_CLASS_NOT_SERIALIZABLE(swoole_socket_coro);
    SW_SET_CLASS_CLONEABLE(swoole_socket_coro, sw_zend_class_clone_deny);
    SW_SET_CLASS_UNSET_PROPERTY_HANDLER(swoole_socket_coro, sw_zend_class_unset_property_deny);
    SW_SET_CLASS_CUSTOM_OBJECT(
        swoole_socket_coro, socket_coro_create_object, socket_coro_free_object, SocketObject, std);

    zend_declare_property_long(swoole_socket_coro_ce, ZEND_STRL("fd"), -1, ZEND_ACC_PUBLIC | ZEND_ACC_READONLY);
    zend_declare_property_long(swoole_socket_coro_ce, ZEND_STRL("domain"), 0, ZEND_ACC_PUBLIC | ZEND_ACC_READONLY);
    zend_declare_property_long(swoole_socket_coro_ce, ZEND_STRL("type"), 0, ZEND_ACC_PUBLIC | ZEND_ACC_READONLY);
    zend_declare_property_long(swoole_socket_coro_ce, ZEND_STRL("protocol"), 0, ZEND_ACC_PUBLIC | ZEND_ACC_READONLY);
    zend_declare_property_long(swoole_socket_coro_ce, ZEND_STRL("errCode"), 0, ZEND_ACC_PUBLIC);
    zend_declare_property_string(swoole_socket_coro_ce, ZEND_STRL("errMsg"), "", ZEND_ACC_PUBLIC);

#ifdef SWOOLE_SOCKETS_SUPPORT
    zend_declare_property_bool(swoole_socket_coro_ce, ZEND_STRL("__ext_sockets_nonblock"), 0, ZEND_ACC_PUBLIC);
    zend_declare_property_long(swoole_socket_coro_ce, ZEND_STRL("__ext_sockets_timeout"), 0, ZEND_ACC_PUBLIC);
#endif

    SW_INIT_CLASS_ENTRY_EX(swoole_socket_coro_exception,
                           "Swoole\\Coroutine\\Socket\\Exception",
                           "Co\\Socket\\Exception",
                           nullptr,
                           swoole_exception);

    if (!zend_hash_str_find_ptr(&module_registry, ZEND_STRL("sockets"))) {
        socket_coro_register_constants(module_number);
    }
#ifdef ECANCELED
    SW_REGISTER_LONG_CONSTANT("SOCKET_ECANCELED", ECANCELED);
#endif
#ifdef TCP_INFO
    SW_REGISTER_LONG_CONSTANT("TCP_INFO", TCP_INFO);
#endif
}

static sw_inline void socket_coro_sync_properties(zval *zobject, SocketObject *sock) {
    zend_update_property_long(swoole_socket_coro_ce, SW_Z8_OBJ_P(zobject), ZEND_STRL("errCode"), sock->socket->errCode);
    zend_update_property_string(swoole_socket_coro_ce, SW_Z8_OBJ_P(zobject), ZEND_STRL("errMsg"), sock->socket->errMsg);
}

static void sw_inline socket_coro_init(zval *zobject, SocketObject *sock) {
    sock->socket->set_zero_copy(true);
    sock->socket->set_buffer_allocator(sw_zend_string_allocator());
    zend_update_property_long(swoole_socket_coro_ce, SW_Z8_OBJ_P(zobject), ZEND_STRL("fd"), sock->socket->get_fd());
    zend_update_property_long(
        swoole_socket_coro_ce, SW_Z8_OBJ_P(zobject), ZEND_STRL("domain"), sock->socket->get_sock_domain());
    zend_update_property_long(
        swoole_socket_coro_ce, SW_Z8_OBJ_P(zobject), ZEND_STRL("type"), sock->socket->get_sock_type());
    zend_update_property_long(
        swoole_socket_coro_ce, SW_Z8_OBJ_P(zobject), ZEND_STRL("protocol"), sock->socket->get_sock_protocol());
}

SW_API bool php_swoole_export_socket(zval *zobject, Socket *_socket) {
    zend_object *object = socket_coro_create_object(swoole_socket_coro_ce);
    if (!object) {
        return false;
    }

    auto *sock = socket_coro_fetch_object(object);
    sock->reference = true;
    sock->socket = _socket;

    ZVAL_OBJ(zobject, object);
    socket_coro_init(zobject, sock);
    return true;
}

SW_API zend_object *php_swoole_dup_socket(int fd, swSocketType type) {
    php_swoole_check_reactor();
    int new_fd = dup(fd);
    if (new_fd < 0) {
        php_swoole_sys_error(E_WARNING, "dup(%d) failed", fd);
        return nullptr;
    }
    return php_swoole_create_socket_from_fd(new_fd, type);
}

SW_API zend_object *php_swoole_create_socket(swSocketType type) {
    zval zobject;
    zend_object *object = socket_coro_create_object(swoole_socket_coro_ce);
    auto *sock = socket_coro_fetch_object(object);

    sock->socket = new Socket(type);
    if (UNEXPECTED(sock->socket->get_fd() < 0)) {
        php_swoole_sys_error(E_WARNING, "new Socket() failed");
        delete sock->socket;
        sock->socket = nullptr;
        OBJ_RELEASE(object);
        return nullptr;
    }

    ZVAL_OBJ(&zobject, object);
    socket_coro_init(&zobject, sock);
    return object;
}

SW_API void php_swoole_socket_set_error_properties(zval *zobject, int code, const char *msg) {
    swoole_set_last_error(code);
    zend_update_property_long(Z_OBJCE_P(zobject), SW_Z8_OBJ_P(zobject), ZEND_STRL("errCode"), code);
    zend_update_property_string(Z_OBJCE_P(zobject), SW_Z8_OBJ_P(zobject), ZEND_STRL("errMsg"), msg);
}

SW_API void php_swoole_socket_set_error_properties(zval *zobject, int code) {
    php_swoole_socket_set_error_properties(zobject, code, swoole_strerror(code));
}

SW_API void php_swoole_socket_set_error_properties(zval *zobject, Socket *socket) {
    php_swoole_socket_set_error_properties(zobject, socket->errCode, socket->errMsg);
}

static zend_object *create_socket_object(Socket *socket) {
    zval zobject;
    auto *object = socket_coro_create_object(swoole_socket_coro_ce);
    auto *sock = socket_coro_fetch_object(object);

    sock->socket = socket;
    if (UNEXPECTED(sock->socket->get_fd() < 0)) {
        php_swoole_sys_error(E_WARNING, "new Socket() failed");
        delete sock->socket;
        sock->socket = nullptr;
        OBJ_RELEASE(object);
        return nullptr;
    }

    ZVAL_OBJ(&zobject, object);
    socket_coro_init(&zobject, sock);
    return object;
}

SW_API zend_object *php_swoole_create_socket_from_fd(int fd, swSocketType type) {
    return create_socket_object(new Socket(fd, type));
}

SW_API zend_object *php_swoole_create_socket_from_fd(int fd, int _domain, int _type, int _protocol) {
    return create_socket_object(new Socket(fd, _domain, _type, _protocol));
}

SW_API Socket *php_swoole_get_socket(zval *zobject) {
    SW_ASSERT(Z_OBJCE_P(zobject) == swoole_socket_coro_ce);
    auto *sock = socket_coro_fetch_object(Z_OBJ_P(zobject));
    return sock->socket;
}

SW_API bool php_swoole_socket_is_closed(zval *zobject) {
    auto *_sock = socket_coro_fetch_object(Z_OBJ_P(zobject));
    return _sock->socket == nullptr || _sock->socket->is_closed();
}

SW_API void php_swoole_init_socket_object(zval *zobject, Socket *socket) {
    auto *object = socket_coro_create_object(swoole_socket_coro_ce);
    auto *sock = socket_coro_fetch_object(object);
    sock->socket = socket;
    ZVAL_OBJ(zobject, object);
    socket_coro_init(zobject, sock);
}

SW_API bool php_swoole_socket_set_protocol(Socket *sock, zval *zset) {
    HashTable *vht = Z_ARRVAL_P(zset);
    zval *ztmp;
    bool ret = true;

    /**
     * ssl
     */
#ifdef SW_USE_OPENSSL
    if (php_swoole_array_get_value(vht, "open_ssl", ztmp)) {
        if (zval_is_true(ztmp)) {
            sock->enable_ssl_encrypt();
        }
    }
    if (sock->ssl_is_enable()) {
        if (!php_swoole_socket_set_ssl(sock, zset)) {
            ret = false;
        }
    }
#endif
    /**
     * protocol
     */
    // buffer: eof check
    if (php_swoole_array_get_value(vht, "open_eof_check", ztmp)) {
        sock->open_eof_check = zval_is_true(ztmp);
    }
    // buffer: split package with eof
    if (php_swoole_array_get_value(vht, "open_eof_split", ztmp)) {
        sock->protocol.split_by_eof = zval_is_true(ztmp);
        if (sock->protocol.split_by_eof) {
            sock->open_eof_check = true;
        }
    }
    // package eof
    if (php_swoole_array_get_value(vht, "package_eof", ztmp)) {
        zend::String str_v(ztmp);
        if (str_v.len() == 0) {
            php_swoole_fatal_error(E_ERROR, "package_eof cannot be an empty string");
            ret = false;
        } else if (str_v.len() > SW_DATA_EOF_MAXLEN) {
            php_swoole_fatal_error(E_ERROR, "package_eof max length is %d", SW_DATA_EOF_MAXLEN);
            ret = false;
        } else {
            sock->protocol.package_eof_len = str_v.len();
            memcpy(sock->protocol.package_eof, str_v.val(), str_v.len());
        }
    }
    if (php_swoole_array_get_value(vht, "open_fastcgi_protocol", ztmp)) {
#define FCGI_HEADER_LEN 8
#define FCGI_MAX_LENGTH 0xffff
        sock->open_length_check = zval_is_true(ztmp);
        sock->protocol.package_length_size = FCGI_HEADER_LEN;
        sock->protocol.package_length_offset = 0;
        sock->protocol.package_body_offset = 0;
        sock->protocol.get_package_length =
            [](const Protocol *protocol, swoole::network::Socket *conn, PacketLength *pl) {
                const auto *p = reinterpret_cast<const uint8_t *>(pl->buf);
                ssize_t length = 0;
                if (pl->buf_size >= FCGI_HEADER_LEN) {
                    length = ((p[4] << 8) | p[5]) + p[6];
                    if (length > FCGI_MAX_LENGTH) {
                        length = -1;
                    } else {
                        length += FCGI_HEADER_LEN;
                    }
                }
                return length;
            };
    }
    // open mqtt protocol
    if (php_swoole_array_get_value(vht, "open_mqtt_protocol", ztmp)) {
        sock->open_length_check = zval_is_true(ztmp);
        if (zval_is_true(ztmp)) {
            swoole::mqtt::set_protocol(&sock->protocol);
        }
    }
    // open length check
    if (php_swoole_array_get_value(vht, "open_length_check", ztmp)) {
        sock->open_length_check = zval_is_true(ztmp);
        sock->protocol.get_package_length = Protocol::default_length_func;
    }
    // package length size
    if (php_swoole_array_get_value(vht, "package_length_type", ztmp)) {
        zend::String str_v(ztmp);
        sock->protocol.package_length_type = str_v.val()[0];
        sock->protocol.package_length_size = swoole_type_size(sock->protocol.package_length_type);
        if (sock->protocol.package_length_size == 0) {
            php_swoole_fatal_error(E_WARNING,
                                   "Unknown package_length_type name '%c', see pack(). Link: http://php.net/pack",
                                   sock->protocol.package_length_type);
            ret = false;
        }
    }
    // package length offset
    if (php_swoole_array_get_value(vht, "package_length_offset", ztmp)) {
        zend_long v = zval_get_long(ztmp);
        sock->protocol.package_length_offset = SW_MAX(0, SW_MIN(v, UINT16_MAX));
    }
    // package body start
    if (php_swoole_array_get_value(vht, "package_body_offset", ztmp)) {
        zend_long v = zval_get_long(ztmp);
        sock->protocol.package_body_offset = SW_MAX(0, SW_MIN(v, UINT16_MAX));
    }
    // length function
    if (php_swoole_array_get_value(vht, "package_length_func", ztmp)) {
        auto cb = sw_callable_create(ztmp);
        if (cb) {
            sock->protocol.get_package_length = php_swoole_length_func;
            if (sock->protocol.private_data_1) {
                sw_callable_free(sock->protocol.private_data_1);
            }
            sock->protocol.private_data_1 = cb;
            sock->protocol.package_length_size = 0;
            sock->protocol.package_length_type = '\0';
            sock->protocol.package_length_offset = SW_IPC_BUFFER_SIZE;
        }
    }
    /**
     * package max length
     */
    if (php_swoole_array_get_value(vht, "package_max_length", ztmp)) {
        zend_long v = php_swoole_parse_to_size(ztmp);
        sock->protocol.package_max_length = SW_MAX(0, SW_MIN(v, UINT32_MAX));
    } else {
        sock->protocol.package_max_length = SW_INPUT_BUFFER_SIZE;
    }

    return ret;
}

SW_API bool php_swoole_socket_set(Socket *cli, zval *zset) {
    HashTable *vht = Z_ARRVAL_P(zset);
    zval *ztmp;
    bool ret = true;

    /**
     * timeout
     */
    if (php_swoole_array_get_value(vht, "timeout", ztmp)) {
        cli->set_timeout(zval_get_double(ztmp));
    }
    if (php_swoole_array_get_value(vht, "connect_timeout", ztmp)) {
        cli->set_timeout(zval_get_double(ztmp), SW_TIMEOUT_CONNECT);
    }
    if (php_swoole_array_get_value(vht, "read_timeout", ztmp)) {
        cli->set_timeout(zval_get_double(ztmp), SW_TIMEOUT_READ);
    }
    if (php_swoole_array_get_value(vht, "write_timeout", ztmp)) {
        cli->set_timeout(zval_get_double(ztmp), SW_TIMEOUT_WRITE);
    }
    std::string _bind_address;
    int _bind_port = 0;
    if (php_swoole_array_get_value(vht, "bind_port", ztmp)) {
        zend_long v = zval_get_long(ztmp);
        _bind_port = SW_MAX(0, SW_MIN(v, UINT16_MAX));
    }
    if (php_swoole_array_get_value(vht, "bind_address", ztmp)) {
        zend::String tmp = ztmp;
        _bind_address = tmp.to_std_string();
    }
    if (!_bind_address.empty() && !cli->bind(_bind_address, _bind_port)) {
        ret = false;
    }
    /**
     * socket send/recv buffer size
     */
    if (php_swoole_array_get_value(vht, "socket_buffer_size", ztmp)) {
        zend_long size = php_swoole_parse_to_size(ztmp);
        if (size <= 0) {
            php_swoole_fatal_error(E_WARNING, "socket buffer size must be greater than 0, got " ZEND_LONG_FMT, size);
            ret = false;
        } else {
            cli->set_option(SOL_SOCKET, SO_RCVBUF, size) && cli->set_option(SOL_SOCKET, SO_SNDBUF, size);
        }
    }
    /**
     * client: tcp_nodelay
     */
    if (php_swoole_array_get_value(vht, "open_tcp_nodelay", ztmp)) {
        if (cli->get_type() == SW_SOCK_TCP || cli->get_type() != SW_SOCK_TCP6) {
            cli->get_socket()->set_tcp_nodelay(zval_is_true(ztmp));
        }
    }
    /**
     * openssl and protocol options
     */
    if (!php_swoole_socket_set_protocol(cli, zset)) {
        ret = false;
    }
    /**
     * socks5 proxy
     */
    if (php_swoole_array_get_value(vht, "socks5_host", ztmp)) {
        zend::String host(ztmp);
        if (php_swoole_array_get_value(vht, "socks5_port", ztmp)) {
            std::string user, pwd;
            auto socks5_port = zval_get_long(ztmp);
            if (php_swoole_array_get_value(vht, "socks5_username", ztmp)) {
                user = zend::String(ztmp).to_std_string();
                if (!user.empty() && php_swoole_array_get_value(vht, "socks5_password", ztmp)) {
                    pwd = zend::String(ztmp).to_std_string();
                } else {
                    php_swoole_fatal_error(E_WARNING, "socks5_password should not be null");
                    ret = false;
                }
            }
            cli->set_socks5_proxy(host.to_std_string(), socks5_port, user, pwd);
        } else {
            php_swoole_fatal_error(E_WARNING, "socks5_port should not be null");
            ret = false;
        }
    }
    /**
     * http proxy
     */
    else if (php_swoole_array_get_value(vht, "http_proxy_host", ztmp)) {
        zend::String host(ztmp);
        if (php_swoole_array_get_value(vht, "http_proxy_port", ztmp)) {
            std::string user, pwd;
            auto http_proxy_port = zval_get_long(ztmp);
            if (php_swoole_array_get_value(vht, "http_proxy_username", ztmp) ||
                php_swoole_array_get_value(vht, "http_proxy_user", ztmp)) {
                user = zend::String(ztmp).to_std_string();
                if (!user.empty() && php_swoole_array_get_value(vht, "http_proxy_password", ztmp)) {
                    pwd = zend::String(ztmp).to_std_string();
                } else {
                    php_swoole_fatal_error(E_WARNING, "socks5_password should not be null");
                    ret = false;
                }
            }
            cli->set_http_proxy(host.to_std_string(), http_proxy_port, user, pwd);
        } else {
            php_swoole_fatal_error(E_WARNING, "http_proxy_port should not be null");
            ret = false;
        }
    }

    return ret;
}

#ifdef SW_USE_OPENSSL
SW_API bool php_swoole_socket_set_ssl(Socket *sock, zval *zset) {
    HashTable *vht = Z_ARRVAL_P(zset);
    zval *ztmp;

    if (php_swoole_array_get_value(vht, "ssl_protocols", ztmp)) {
        zend_long v = zval_get_long(ztmp);
        sock->set_ssl_protocols(v);
    }
    if (php_swoole_array_get_value(vht, "ssl_compress", ztmp)) {
        sock->set_ssl_disable_compress(!zval_is_true(ztmp));
    } else if (php_swoole_array_get_value(vht, "ssl_disable_compression", ztmp)) {
        sock->set_ssl_disable_compress(!zval_is_true(ztmp));
    }
    if (php_swoole_array_get_value(vht, "ssl_cert_file", ztmp)) {
        zend::String str_v(ztmp);
        if (!sock->set_ssl_cert_file(str_v.to_std_string())) {
            php_swoole_fatal_error(E_WARNING, "ssl cert file[%s] not found", str_v.val());
            return false;
        }
    }
    if (php_swoole_array_get_value(vht, "ssl_key_file", ztmp)) {
        zend::String str_v(ztmp);
        if (!sock->set_ssl_key_file(str_v.to_std_string())) {
            php_swoole_fatal_error(E_WARNING, "ssl key file[%s] not found", str_v.val());
            return false;
        }
    }
    if (!sock->get_ssl_cert_file().empty() && sock->get_ssl_key_file().empty()) {
        php_swoole_fatal_error(E_WARNING, "ssl require key file");
    }
    if (!sock->get_ssl_key_file().empty() && sock->get_ssl_cert_file().empty()) {
        php_swoole_fatal_error(E_WARNING, "ssl require cert file");
    }
    if (php_swoole_array_get_value(vht, "ssl_passphrase", ztmp)) {
        sock->set_ssl_passphrase(zend::String(ztmp).to_std_string());
    }
#ifdef SSL_CTRL_SET_TLSEXT_HOSTNAME
    if (php_swoole_array_get_value(vht, "ssl_host_name", ztmp)) {
        sock->set_tls_host_name(zend::String(ztmp).to_std_string());
    }
#endif
    if (php_swoole_array_get_value(vht, "ssl_verify_peer", ztmp)) {
        sock->set_ssl_verify_peer(zval_is_true(ztmp));
    }
    if (php_swoole_array_get_value(vht, "ssl_allow_self_signed", ztmp)) {
        sock->set_ssl_allow_self_signed(zval_is_true(ztmp));
    }
    if (php_swoole_array_get_value(vht, "ssl_cafile", ztmp)) {
        sock->set_ssl_cafile(zend::String(ztmp).to_std_string());
    }
    if (php_swoole_array_get_value(vht, "ssl_capath", ztmp)) {
        sock->set_ssl_capath(zend::String(ztmp).to_std_string());
    }
    if (php_swoole_array_get_value(vht, "ssl_verify_depth", ztmp)) {
        zend_long v = zval_get_long(ztmp);
        sock->set_ssl_verify_depth(SW_MAX(0, SW_MIN(v, UINT8_MAX)));
    }
    if (php_swoole_array_get_value(vht, "ssl_ciphers", ztmp)) {
        sock->set_ssl_ciphers(zend::String(ztmp).to_std_string());
    }
    if (php_swoole_array_get_value(vht, "ssl_ecdh_curve", ztmp)) {
        sock->set_ssl_ecdh_curve(zend::String(ztmp).to_std_string());
    }
#ifdef OPENSSL_IS_BORINGSSL
    if (php_swoole_array_get_value(vht, "ssl_grease", ztmp)) {
        zend_long v = zval_get_long(ztmp);
        sock->set_ssl_grease(SW_MAX(0, SW_MIN(v, UINT8_MAX)));
    }
#endif
    return true;
}
#endif

PHP_FUNCTION(swoole_coroutine_socketpair) {
    zend_long domain, type, protocol;
    php_socket_t pair[2];

    ZEND_PARSE_PARAMETERS_START(3, 3)
    Z_PARAM_LONG(domain)
    Z_PARAM_LONG(type)
    Z_PARAM_LONG(protocol)
    ZEND_PARSE_PARAMETERS_END_EX(RETURN_FALSE);

    if (0 != socketpair((int) domain, (int) type, (int) protocol, pair)) {
        php_swoole_sys_error(E_WARNING, "failed to create socket");
        RETURN_FALSE;
    }

    php_swoole_check_reactor();

    auto sock_type = swoole::network::Socket::convert_to_type(domain, type);

    zend_object *s1 = php_swoole_create_socket_from_fd(pair[0], sock_type);
    if (s1 == nullptr) {
        RETURN_FALSE;
    }

    zend_object *s2 = php_swoole_create_socket_from_fd(pair[1], sock_type);
    if (s2 == nullptr) {
        OBJ_RELEASE(s1);
        RETURN_FALSE;
    }

    zval zobject1, zobject2;
    ZVAL_OBJ(&zobject1, s1);
    ZVAL_OBJ(&zobject2, s2);

    array_init(return_value);
    add_next_index_zval(return_value, &zobject1);
    add_next_index_zval(return_value, &zobject2);
}

static PHP_METHOD(swoole_socket_coro, __construct) {
    zend_long domain, type, protocol = IPPROTO_IP;

    ZEND_PARSE_PARAMETERS_START_EX(ZEND_PARSE_PARAMS_THROW, 2, 3)
    Z_PARAM_LONG(domain)
    Z_PARAM_LONG(type)
    Z_PARAM_OPTIONAL
    Z_PARAM_LONG(protocol)
    ZEND_PARSE_PARAMETERS_END_EX(RETURN_FALSE);

    auto *sock = socket_coro_fetch_object(Z_OBJ_P(ZEND_THIS));
    if (sock->socket) {
        zend_throw_error(nullptr, "Constructor of %s can only be called once", SW_Z_OBJCE_NAME_VAL_P(ZEND_THIS));
        RETURN_FALSE;
    }

    php_swoole_check_reactor();
    sock->socket = new Socket((int) domain, (int) type, (int) protocol);
    if (UNEXPECTED(sock->socket->get_fd() < 0)) {
        zend_throw_exception_ex(
            swoole_socket_coro_exception_ce, errno, "new Socket() failed. Error: %s [%d]", strerror(errno), errno);
        delete sock->socket;
        sock->socket = nullptr;
        RETURN_FALSE;
    }
    socket_coro_init(ZEND_THIS, sock);
}

static PHP_METHOD(swoole_socket_coro, bind) {
    char *address;
    size_t l_address;
    zend_long port = 0;

    ZEND_PARSE_PARAMETERS_START(1, 2)
    Z_PARAM_STRING(address, l_address)
    Z_PARAM_OPTIONAL
    Z_PARAM_LONG(port)
    ZEND_PARSE_PARAMETERS_END_EX(RETURN_FALSE);

    swoole_get_socket_coro(sock, ZEND_THIS);

    if (!sock->socket->bind(std::string(address, l_address), port)) {
        socket_coro_sync_properties(ZEND_THIS, sock);
        RETURN_FALSE;
    }
    RETURN_TRUE;
}

static PHP_METHOD(swoole_socket_coro, listen) {
    zend_long backlog = SW_BACKLOG;

    ZEND_PARSE_PARAMETERS_START(0, 1)
    Z_PARAM_OPTIONAL
    Z_PARAM_LONG(backlog)
    ZEND_PARSE_PARAMETERS_END_EX(RETURN_FALSE);

    swoole_get_socket_coro(sock, ZEND_THIS);

    if (!sock->socket->listen(backlog)) {
        socket_coro_sync_properties(ZEND_THIS, sock);
        RETURN_FALSE;
    }
    RETURN_TRUE;
}

static PHP_METHOD(swoole_socket_coro, accept) {
    double timeout = 0;

    ZEND_PARSE_PARAMETERS_START(0, 1)
    Z_PARAM_OPTIONAL
    Z_PARAM_DOUBLE(timeout)
    ZEND_PARSE_PARAMETERS_END_EX(RETURN_FALSE);

    swoole_get_socket_coro(sock, ZEND_THIS);

    Socket *conn = sock->socket->accept(timeout);
    if (conn) {
        zend_object *client = socket_coro_create_object(swoole_socket_coro_ce);
        auto *client_sock = socket_coro_fetch_object(client);
        client_sock->socket = conn;
        ZVAL_OBJ(return_value, &client_sock->std);
        socket_coro_init(return_value, client_sock);
        // It must be copied once to avoid destroying the function when the connection closes.
        if (sock->socket->protocol.private_data_1) {
            auto *cb = static_cast<zend::Callable *>(sock->socket->protocol.private_data_1);
            conn->protocol.private_data_1 = cb->dup();
        }
    } else {
        socket_coro_sync_properties(ZEND_THIS, sock);
        RETURN_FALSE;
    }
}

static PHP_METHOD(swoole_socket_coro, connect) {
    char *host;
    size_t l_host;
    zend_long port = 0;
    double timeout = 0;

    ZEND_PARSE_PARAMETERS_START(1, 3)
    Z_PARAM_STRING(host, l_host)
    Z_PARAM_OPTIONAL
    Z_PARAM_LONG(port)
    Z_PARAM_DOUBLE(timeout)
    ZEND_PARSE_PARAMETERS_END_EX(RETURN_FALSE);

    swoole_get_socket_coro(sock, ZEND_THIS);

    if (sock->socket->is_port_required()) {
        if (ZEND_NUM_ARGS() == 1) {
            php_swoole_error(E_WARNING, "Socket of type AF_INET/AF_INET6 requires port argument");
            RETURN_FALSE;
        } else if (port == 0 || port >= 65536) {
            php_swoole_error(E_WARNING, "Invalid port argument[" ZEND_LONG_FMT "]", port);
            RETURN_FALSE;
        }
    }
    Socket::TimeoutSetter ts(sock->socket, timeout, SW_TIMEOUT_CONNECT);
    if (!sock->socket->connect(std::string(host, l_host), port)) {
        socket_coro_sync_properties(ZEND_THIS, sock);
        RETURN_FALSE;
    }
    RETURN_TRUE;
}

static PHP_METHOD(swoole_socket_coro, checkLiveness) {
    swoole_get_socket_coro(sock, ZEND_THIS);

    bool liveness = sock->socket->check_liveness();
    socket_coro_sync_properties(ZEND_THIS, sock);
    RETURN_BOOL(liveness);
}

static PHP_METHOD(swoole_socket_coro, getBoundCid) {
    zend_long event;
    ZEND_PARSE_PARAMETERS_START(1, 1)
    Z_PARAM_LONG(event)
    ZEND_PARSE_PARAMETERS_END_EX(RETURN_FALSE);

    swoole_get_socket_coro(sock, ZEND_THIS);
    RETURN_LONG(sock->socket->get_bound_cid((swEventType) event));
}

static PHP_METHOD(swoole_socket_coro, peek) {
    zend_long length = SW_BUFFER_SIZE_BIG;

    ZEND_PARSE_PARAMETERS_START(0, 1)
    Z_PARAM_OPTIONAL
    Z_PARAM_LONG(length)
    ZEND_PARSE_PARAMETERS_END_EX(RETURN_FALSE);

    if (UNEXPECTED(length <= 0)) {
        length = SW_BUFFER_SIZE_BIG;
    }

    swoole_get_socket_coro(sock, ZEND_THIS);

    zend_string *buf = zend_string_alloc(length, false);
    ssize_t bytes = sock->socket->peek(ZSTR_VAL(buf), length);
    socket_coro_sync_properties(ZEND_THIS, sock);
    if (UNEXPECTED(bytes < 0)) {
        zend_string_free(buf);
        RETURN_FALSE;
    } else if (UNEXPECTED(bytes == 0)) {
        zend_string_free(buf);
        RETURN_EMPTY_STRING();
    } else {
        RETURN_STR(sw_zend_string_recycle(buf, length, bytes));
    }
}

enum RecvMode {
    SOCKET_RECV,
    SOCKET_RECV_ALL,
    SOCKET_RECV_LINE,
    SOCKET_RECV_WITH_BUFFER,
};

static inline void socket_coro_recv(INTERNAL_FUNCTION_PARAMETERS, RecvMode type) {
    zend_long length = SW_BUFFER_SIZE_BIG;
    double timeout = 0;

    ZEND_PARSE_PARAMETERS_START(0, 2)
    Z_PARAM_OPTIONAL
    Z_PARAM_LONG(length)
    Z_PARAM_DOUBLE(timeout)
    ZEND_PARSE_PARAMETERS_END_EX(RETURN_FALSE);

    if (UNEXPECTED(length <= 0)) {
        length = SW_BUFFER_SIZE_BIG;
    }

    swoole_get_socket_coro(sock, ZEND_THIS);

    zend_string *buf = zend_string_alloc(length, false);
    Socket::TimeoutSetter ts(sock->socket, timeout, SW_TIMEOUT_READ);
    ssize_t bytes = -1;
    switch (type) {
    case SOCKET_RECV:
        bytes = sock->socket->recv(ZSTR_VAL(buf), length);
        break;
    case SOCKET_RECV_ALL:
        bytes = sock->socket->recv_all(ZSTR_VAL(buf), length);
        break;
    case SOCKET_RECV_LINE:
        bytes = sock->socket->recv_line(ZSTR_VAL(buf), length);
        break;
    case SOCKET_RECV_WITH_BUFFER:
        bytes = sock->socket->recv_with_buffer(ZSTR_VAL(buf), length);
        break;
    default:
        assert(0);
        break;
    }
    socket_coro_sync_properties(ZEND_THIS, sock);
    if (UNEXPECTED(bytes < 0)) {
        zend_string_free(buf);
        RETURN_FALSE;
    } else if (UNEXPECTED(bytes == 0)) {
        zend_string_free(buf);
        RETURN_EMPTY_STRING();
    } else {
        RETURN_STR(sw_zend_string_recycle(buf, length, bytes));
    }
}

static PHP_METHOD(swoole_socket_coro, recv) {
    socket_coro_recv(INTERNAL_FUNCTION_PARAM_PASSTHRU, SOCKET_RECV);
}

static PHP_METHOD(swoole_socket_coro, recvAll) {
    socket_coro_recv(INTERNAL_FUNCTION_PARAM_PASSTHRU, SOCKET_RECV_ALL);
}

static PHP_METHOD(swoole_socket_coro, recvLine) {
    socket_coro_recv(INTERNAL_FUNCTION_PARAM_PASSTHRU, SOCKET_RECV_LINE);
}

static PHP_METHOD(swoole_socket_coro, recvWithBuffer) {
    socket_coro_recv(INTERNAL_FUNCTION_PARAM_PASSTHRU, SOCKET_RECV_WITH_BUFFER);
}

static PHP_METHOD(swoole_socket_coro, recvPacket) {
    double timeout = 0;

    ZEND_PARSE_PARAMETERS_START(0, 1)
    Z_PARAM_OPTIONAL
    Z_PARAM_DOUBLE(timeout)
    ZEND_PARSE_PARAMETERS_END_EX(RETURN_FALSE);

    swoole_get_socket_coro(sock, ZEND_THIS);
    ssize_t retval = sock->socket->recv_packet(timeout);
    socket_coro_sync_properties(ZEND_THIS, sock);
    if (retval < 0) {
        RETURN_FALSE;
    } else if (retval == 0) {
        sock->socket->get_read_buffer()->clear();
        RETURN_EMPTY_STRING();
    } else {
        auto strval = sock->socket->pop_packet();
        if (strval == nullptr) {
            sock->socket->set_err(ENOMEM);
            RETURN_FALSE;
        } else {
            zend::assign_zend_string_by_val(return_value, strval, retval);
        }
    }
}

static sw_inline void socket_coro_send(INTERNAL_FUNCTION_PARAMETERS, const bool all) {
    char *data;
    size_t length;
    double timeout = 0;

    ZEND_PARSE_PARAMETERS_START(1, 2)
    Z_PARAM_STRING(data, length)
    Z_PARAM_OPTIONAL
    Z_PARAM_DOUBLE(timeout)
    ZEND_PARSE_PARAMETERS_END_EX(RETURN_FALSE);

    swoole_get_socket_coro(sock, ZEND_THIS);

    Socket::TimeoutSetter ts(sock->socket, timeout, SW_TIMEOUT_WRITE);
    ssize_t retval = all ? sock->socket->send_all(data, length) : sock->socket->send(data, length);
    socket_coro_sync_properties(ZEND_THIS, sock);
    if (UNEXPECTED(retval < 0)) {
        RETURN_FALSE;
    } else {
        RETURN_LONG(retval);
    }
}

static PHP_METHOD(swoole_socket_coro, send) {
    socket_coro_send(INTERNAL_FUNCTION_PARAM_PASSTHRU, false);
}

static void socket_coro_write_vector(INTERNAL_FUNCTION_PARAMETERS, const bool all) {
    zval *ziov = nullptr;
    zval *zelement = nullptr;
    double timeout = 0;
    int iovcnt = 0;
    int iov_index = 0;

    ZEND_PARSE_PARAMETERS_START(1, 2)
    Z_PARAM_ARRAY(ziov)
    Z_PARAM_OPTIONAL
    Z_PARAM_DOUBLE(timeout)
    ZEND_PARSE_PARAMETERS_END_EX(RETURN_FALSE);

    swoole_get_socket_coro(sock, ZEND_THIS);

    ON_SCOPE_EXIT {
        socket_coro_sync_properties(ZEND_THIS, sock);
    };

    HashTable *vht = Z_ARRVAL_P(ziov);
    iovcnt = zend_array_count(vht);

    if (iovcnt > IOV_MAX) {
        sw_tg_buffer()->length = sw_snprintf(sw_tg_buffer()->str, sw_tg_buffer()->size, SW_IOV_MAX_ERROR_MSG, IOV_MAX);
        sock->socket->set_err(EINVAL, sw_tg_buffer()->to_std_string());
        RETURN_FALSE;
    }

    std::unique_ptr<iovec[]> iov(new iovec[iovcnt]);

    SW_HASHTABLE_FOREACH_START(vht, zelement)
    if (!ZVAL_IS_STRING(zelement)) {
        zend_throw_exception_ex(swoole_socket_coro_exception_ce,
                                EINVAL,
                                "Item #[%d] must be of type string, %s given",
                                iov_index,
                                zend_get_type_by_const(Z_TYPE_P(zelement)));
        RETURN_FALSE;
    }
    if (Z_STRLEN_P(zelement) == 0) {
        zend_throw_exception_ex(
            swoole_socket_coro_exception_ce, EINVAL, "Item #[%d] cannot be empty string", iov_index);
        RETURN_FALSE;
    }
    iov[iov_index].iov_base = Z_STRVAL_P(zelement);
    iov[iov_index].iov_len = Z_STRLEN_P(zelement);
    iov_index++;
    SW_HASHTABLE_FOREACH_END();

    swoole::network::IOVector io_vector((struct iovec *) iov.get(), iovcnt);

    Socket::TimeoutSetter ts(sock->socket, timeout, SW_TIMEOUT_WRITE);
    ssize_t retval = all ? sock->socket->writev_all(&io_vector) : sock->socket->writev(&io_vector);
    if (UNEXPECTED(retval < 0)) {
        RETURN_FALSE;
    } else {
        RETURN_LONG(retval);
    }
}

static PHP_METHOD(swoole_socket_coro, writeVector) {
    socket_coro_write_vector(INTERNAL_FUNCTION_PARAM_PASSTHRU, false);
}

static PHP_METHOD(swoole_socket_coro, writeVectorAll) {
    socket_coro_write_vector(INTERNAL_FUNCTION_PARAM_PASSTHRU, true);
}

static void socket_coro_read_vector(INTERNAL_FUNCTION_PARAMETERS, const bool all) {
    zval *ziov = nullptr;
    zval *zelement = nullptr;
    double timeout = 0;
    int iovcnt = 0;
    int iov_index = 0;
    ssize_t total_length = 0;

    ZEND_PARSE_PARAMETERS_START(1, 2)
    Z_PARAM_ARRAY(ziov)
    Z_PARAM_OPTIONAL
    Z_PARAM_DOUBLE(timeout)
    ZEND_PARSE_PARAMETERS_END_EX(RETURN_FALSE);

    swoole_get_socket_coro(sock, ZEND_THIS);

    ON_SCOPE_EXIT {
        socket_coro_sync_properties(ZEND_THIS, sock);
    };

    HashTable *vht = Z_ARRVAL_P(ziov);
    iovcnt = zend_array_count(vht);

    if (iovcnt > IOV_MAX) {
        sw_tg_buffer()->length = sw_snprintf(sw_tg_buffer()->str, sw_tg_buffer()->size, SW_IOV_MAX_ERROR_MSG, IOV_MAX);
        sock->socket->set_err(EINVAL, sw_tg_buffer()->to_std_string());
        RETURN_FALSE;
    }

    std::unique_ptr<iovec[]> iov(new iovec[iovcnt]);

    SW_HASHTABLE_FOREACH_START(vht, zelement) {
        if (!ZVAL_IS_LONG(zelement)) {
            zend_throw_exception_ex(swoole_socket_coro_exception_ce,
                                    EINVAL,
                                    "Item #[%d] must be of type int, %s given",
                                    iov_index,
                                    zend_get_type_by_const(Z_TYPE_P(zelement)));
            RETURN_FALSE;
        }
        if (Z_LVAL_P(zelement) < 0) {
            zend_throw_exception_ex(
                swoole_socket_coro_exception_ce, EINVAL, "Item #[%d] must be greater than 0", iov_index);
            RETURN_FALSE;
        }
        size_t iov_len = Z_LVAL_P(zelement);

        iov[iov_index].iov_base = zend_string_alloc(iov_len, false)->val;
        iov[iov_index].iov_len = iov_len;
        iov_index++;
        total_length += iov_len;
    }
    SW_HASHTABLE_FOREACH_END();

    swoole::network::IOVector io_vector((struct iovec *) iov.get(), iovcnt);

    Socket::TimeoutSetter ts(sock->socket, timeout, SW_TIMEOUT_READ);
    ssize_t retval = all ? sock->socket->readv_all(&io_vector) : sock->socket->readv(&io_vector);

    auto free_func = [](const iovec *iov, int iovcnt, int iov_index) {
        for (; iov_index < iovcnt; iov_index++) {
            zend_string_free(zend::fetch_zend_string_by_val((char *) iov[iov_index].iov_base));
        }
    };

    if (UNEXPECTED(retval < 0)) {
        free_func(iov.get(), iovcnt, 0);
        RETURN_FALSE;
    } else if (UNEXPECTED(retval == 0)) {
        free_func(iov.get(), iovcnt, 0);
        RETURN_EMPTY_ARRAY();
    } else {
        array_init(return_value);
        size_t real_count;

        if (retval < total_length) {
            /**
             * Free the extra memory.
             * For example iov is [5, 5, 5], but we get ['hello', 'world'], we should free the last iov.
             */
            iov_index = io_vector.get_index();
            size_t offset_bytes = io_vector.get_offset_bytes();

            real_count = iov_index + 1;
            zend_string *str = zend::fetch_zend_string_by_val((char *) iov[iov_index].iov_base);
            iov[iov_index].iov_base = sw_zend_string_recycle(str, iov[iov_index].iov_len, offset_bytes)->val;
            iov[iov_index].iov_len = offset_bytes;
            free_func(iov.get(), iovcnt, real_count);
        } else {
            real_count = iovcnt;
        }

        SW_LOOP_N(real_count) {
            ((char *) iov[i].iov_base)[iov[i].iov_len] = '\0';
            add_next_index_str(return_value, zend::fetch_zend_string_by_val((char *) iov[i].iov_base));
        }
    }
}

static PHP_METHOD(swoole_socket_coro, readVector) {
    socket_coro_read_vector(INTERNAL_FUNCTION_PARAM_PASSTHRU, false);
}

static PHP_METHOD(swoole_socket_coro, readVectorAll) {
    socket_coro_read_vector(INTERNAL_FUNCTION_PARAM_PASSTHRU, true);
}

static PHP_METHOD(swoole_socket_coro, sendFile) {
    char *file;
    size_t file_len;
    zend_long offset = 0;
    zend_long length = 0;

    ZEND_PARSE_PARAMETERS_START(1, 3)
    Z_PARAM_STRING(file, file_len)
    Z_PARAM_OPTIONAL
    Z_PARAM_LONG(offset)
    Z_PARAM_LONG(length)
    ZEND_PARSE_PARAMETERS_END_EX(RETURN_FALSE);

    if (file_len == 0) {
        php_swoole_fatal_error(E_WARNING, "file to send is empty");
        RETURN_FALSE;
    }

    swoole_get_socket_coro(sock, ZEND_THIS);
    if (!sock->socket->sendfile(file, offset, length)) {
        socket_coro_sync_properties(ZEND_THIS, sock);
        RETVAL_FALSE;
    } else {
        RETVAL_TRUE;
    }
}

static PHP_METHOD(swoole_socket_coro, sendAll) {
    socket_coro_send(INTERNAL_FUNCTION_PARAM_PASSTHRU, true);
}

static PHP_METHOD(swoole_socket_coro, recvfrom) {
    zval *peername;
    double timeout = 0;

    ZEND_PARSE_PARAMETERS_START(1, 2)
    Z_PARAM_ZVAL_EX(peername, 0, 1)
    Z_PARAM_OPTIONAL
    Z_PARAM_DOUBLE(timeout)
    ZEND_PARSE_PARAMETERS_END_EX(RETURN_FALSE);

    swoole_get_socket_coro(sock, ZEND_THIS);

    zend_string *buf = zend_string_alloc(SW_BUFFER_SIZE_BIG, false);
    Socket::TimeoutSetter ts(sock->socket, timeout, SW_TIMEOUT_READ);
    ssize_t bytes = sock->socket->recvfrom(ZSTR_VAL(buf), SW_BUFFER_SIZE_BIG);
    socket_coro_sync_properties(ZEND_THIS, sock);
    if (bytes < 0) {
        zend_string_free(buf);
        RETURN_FALSE;
    } else if (bytes == 0) {
        zend_string_free(buf);
        RETURN_EMPTY_STRING();
    } else {
        zval_dtor(peername);
        array_init(peername);
        add_assoc_string(peername, "address", (char *) sock->socket->get_addr());
        add_assoc_long(peername, "port", sock->socket->get_port());

        ZSTR_LEN(buf) = bytes;
        ZSTR_VAL(buf)[bytes] = 0;
        RETURN_STR(buf);
    }
}

static PHP_METHOD(swoole_socket_coro, sendto) {
    char *data;
    size_t l_data;
    char *addr;
    size_t l_addr;
    zend_long port = 0;

    ZEND_PARSE_PARAMETERS_START(3, 3)
    Z_PARAM_STRING(addr, l_addr)
    Z_PARAM_LONG(port)
    Z_PARAM_STRING(data, l_data)
    ZEND_PARSE_PARAMETERS_END_EX(RETURN_FALSE);

    swoole_get_socket_coro(sock, ZEND_THIS);

    ssize_t retval = sock->socket->sendto(std::string(addr, l_addr), port, data, l_data);
    socket_coro_sync_properties(ZEND_THIS, sock);
    if (retval < 0) {
        RETURN_FALSE;
    } else {
        RETURN_LONG(retval);
    }
}

static PHP_METHOD(swoole_socket_coro, shutdown) {
    zend_long how = SHUT_RDWR;

    ZEND_PARSE_PARAMETERS_START(0, 1)
    Z_PARAM_OPTIONAL
    Z_PARAM_LONG(how)
    ZEND_PARSE_PARAMETERS_END_EX(RETURN_FALSE);

    swoole_get_socket_coro(sock, ZEND_THIS);

    if (!sock->socket->shutdown(how)) {
        socket_coro_sync_properties(ZEND_THIS, sock);
        RETURN_FALSE;
    }
    RETURN_TRUE;
}

static PHP_METHOD(swoole_socket_coro, close) {
    swoole_get_socket_coro(sock, ZEND_THIS);
    if (sock->reference) {
        php_swoole_error(E_WARNING, "cannot close the referenced resource");
        RETURN_FALSE;
    }
    if (sock->socket->protocol.private_data_1) {
        sw_callable_free(sock->socket->protocol.private_data_1);
        sock->socket->protocol.private_data_1 = nullptr;
    }
    if (!Z_ISUNDEF(sock->zstream)) {
        php_stream *stream = nullptr;
        php_stream_from_zval_no_verify(stream, &sock->zstream);
        if (stream != nullptr) {
            /* close & destroy stream, incl. removing it from the rsrc list;
             * resource stored in php_sock->zstream will become invalid */
            php_stream_free(stream,
                            PHP_STREAM_FREE_KEEP_RSRC | PHP_STREAM_FREE_CLOSE |
                                (stream->is_persistent ? PHP_STREAM_FREE_CLOSE_PERSISTENT : 0));
        }
        ZVAL_UNDEF(&sock->zstream);
        sock->socket->move_fd();
    } else {
        sock->socket->close();
    }
    RETURN_TRUE;
}

static PHP_METHOD(swoole_socket_coro, getsockname) {
    swoole_get_socket_coro(sock, ZEND_THIS);

    if (!sock->socket->getsockname()) {
        socket_coro_sync_properties(ZEND_THIS, sock);
        RETURN_FALSE;
    }

    array_init(return_value);
    add_assoc_string(return_value, "address", sock->socket->get_addr());
    add_assoc_long(return_value, "port", sock->socket->get_port());
}

static PHP_METHOD(swoole_socket_coro, getpeername) {
    swoole_get_socket_coro(sock, ZEND_THIS);

    Address sa;
    if (!sock->socket->getpeername(&sa)) {
        socket_coro_sync_properties(ZEND_THIS, sock);
        RETURN_FALSE;
    }

    array_init(return_value);
    add_assoc_string(return_value, "address", sa.get_addr());
    add_assoc_long(return_value, "port", sa.get_port());
}

static PHP_METHOD(swoole_socket_coro, getOption) {
    struct linger linger_val;
    socklen_t optlen;
    int other_val;
    zend_long level, optname;

    ZEND_PARSE_PARAMETERS_START(2, 2)
    Z_PARAM_LONG(level)
    Z_PARAM_LONG(optname)
    ZEND_PARSE_PARAMETERS_END_EX(RETURN_FALSE);

    swoole_get_socket_coro(sock, ZEND_THIS);
    auto _socket = sock->socket->get_socket();

    if (level == IPPROTO_IP) {
        switch (optname) {
        case IP_MULTICAST_IF: {
            struct in_addr if_addr;
            unsigned int if_index;
            optlen = sizeof(if_addr);
            if (_socket->get_option(level, optname, &if_addr, &optlen) != 0) {
                php_swoole_sys_error(E_WARNING,
                                     "getsockopt(%d, " ZEND_LONG_FMT ", " ZEND_LONG_FMT ")",
                                     sock->socket->get_fd(),
                                     level,
                                     optname);
                RETURN_FALSE;
            }
            if (php_add4_to_if_index(&if_addr, sock->socket, &if_index) == SUCCESS) {
                RETURN_LONG((zend_long) if_index);
            } else {
                RETURN_FALSE;
            }
        }
        }
    } else if (level == IPPROTO_IPV6) {
        int ret = php_do_getsockopt_ipv6_rfc3542(sock->socket, level, optname, return_value);
        if (ret == SUCCESS) {
            return;
        } else if (ret == FAILURE) {
            RETURN_FALSE;
        } /* else continue */
    }

    /* sol_socket options and general case */
    switch (optname) {
    case SO_LINGER: {
        optlen = sizeof(linger_val);

        if (_socket->get_option(level, optname, (char *) &linger_val, &optlen) != 0) {
            php_swoole_sys_error(E_WARNING,
                                 "getsockopt(%d, " ZEND_LONG_FMT ", " ZEND_LONG_FMT ")",
                                 sock->socket->get_fd(),
                                 level,
                                 optname);
            RETURN_FALSE;
        }

        array_init(return_value);
        add_assoc_long(return_value, "l_onoff", linger_val.l_onoff);
        add_assoc_long(return_value, "l_linger", linger_val.l_linger);
        break;
    }
    case SO_RCVTIMEO:
    case SO_SNDTIMEO: {
        double timeout = sock->socket->get_timeout(optname == SO_RCVTIMEO ? SW_TIMEOUT_READ : SW_TIMEOUT_WRITE);
        array_init(return_value);
        int sec = (int) timeout;
        add_assoc_long(return_value, "sec", (int) timeout);
        add_assoc_long(return_value, "usec", (timeout - (double) sec) * 1000000);
        break;
    }
#ifdef TCP_INFO
    case TCP_INFO: {
        tcp_info info;
        socklen_t len = sizeof(info);
        if (_socket->get_option(SOL_TCP, TCP_INFO, &info, &len) < 0) {
            php_swoole_sys_error(E_WARNING, "getsockopt(%d, SOL_TCP, TCP_INFO)", sock->socket->get_fd());
        } else {
            array_init(return_value);
            auto info_map = sw_socket_parse_tcp_info(&info);
            for (const auto &iter : info_map) {
                add_assoc_long_ex(return_value, iter.first.c_str(), iter.first.length(), (zend_long) iter.second);
            }
        }
        break;
    }
#endif
    default: {
        optlen = sizeof(other_val);

        if (_socket->get_option(level, optname, (char *) &other_val, &optlen) != 0) {
            php_swoole_sys_error(E_WARNING,
                                 "getsockopt(%d, " ZEND_LONG_FMT ", " ZEND_LONG_FMT ")",
                                 sock->socket->get_fd(),
                                 level,
                                 optname);
            RETURN_FALSE;
        }
        if (optlen == 1) {
            char _val;
            memcpy(&_val, &other_val, optlen);
            RETURN_LONG(_val);
        } else {
            RETURN_LONG(other_val);
        }
        break;
    }
    }
}

static PHP_METHOD(swoole_socket_coro, setOption) {
    zval *arg4;
    struct linger lv;
    int ov, optlen, retval;
    struct timeval tv;
    zend_long level, optname;
    char *opt_ptr;
    HashTable *opt_ht;
    zval *l_onoff, *l_linger;
    zval *sec, *usec;

    ZEND_PARSE_PARAMETERS_START(3, 3)
    Z_PARAM_LONG(level)
    Z_PARAM_LONG(optname)
    Z_PARAM_ZVAL(arg4)
    ZEND_PARSE_PARAMETERS_END_EX(RETURN_FALSE);

    swoole_get_socket_coro(sock, ZEND_THIS);

#define HANDLE_SUBCALL(res)                                                                                            \
    do {                                                                                                               \
        if (res == 1) {                                                                                                \
            goto default_case;                                                                                         \
        } else if (res == SUCCESS) {                                                                                   \
            RETURN_TRUE;                                                                                               \
        } else {                                                                                                       \
            RETURN_FALSE;                                                                                              \
        }                                                                                                              \
    } while (0)

    if (level == IPPROTO_IP) {
        int res = php_do_setsockopt_ip_mcast(sock->socket, level, optname, arg4);
        HANDLE_SUBCALL(res);
    } else if (level == IPPROTO_IPV6) {
        int res = php_do_setsockopt_ipv6_mcast(sock->socket, level, optname, arg4);
        if (res == 1) {
            res = php_do_setsockopt_ipv6_rfc3542(sock->socket, level, optname, arg4);
        }
        HANDLE_SUBCALL(res);
    }

    switch (optname) {
    case SO_LINGER: {
        const char l_onoff_key[] = "l_onoff";
        const char l_linger_key[] = "l_linger";

        convert_to_array_ex(arg4);
        opt_ht = Z_ARRVAL_P(arg4);

        if ((l_onoff = zend_hash_str_find(opt_ht, l_onoff_key, sizeof(l_onoff_key) - 1)) == nullptr) {
            php_error_docref(nullptr, E_WARNING, "no key \"%s\" passed in optval", l_onoff_key);
            RETURN_FALSE;
        }
        if ((l_linger = zend_hash_str_find(opt_ht, l_linger_key, sizeof(l_linger_key) - 1)) == nullptr) {
            php_error_docref(nullptr, E_WARNING, "no key \"%s\" passed in optval", l_linger_key);
            RETURN_FALSE;
        }

        convert_to_long_ex(l_onoff);
        convert_to_long_ex(l_linger);

        lv.l_onoff = (unsigned short) Z_LVAL_P(l_onoff);
        lv.l_linger = (unsigned short) Z_LVAL_P(l_linger);

        optlen = sizeof(lv);
        opt_ptr = (char *) &lv;
        break;
    }

    case SO_RCVTIMEO:
    case SO_SNDTIMEO: {
        constexpr char sec_key[] = "sec";
        constexpr char usec_key[] = "usec";

        convert_to_array_ex(arg4);
        opt_ht = Z_ARRVAL_P(arg4);

        if ((sec = zend_hash_str_find(opt_ht, sec_key, sizeof(sec_key) - 1)) == nullptr) {
            php_error_docref(nullptr, E_WARNING, "no key \"%s\" passed in optval", sec_key);
            RETURN_FALSE;
        }
        if ((usec = zend_hash_str_find(opt_ht, usec_key, sizeof(usec_key) - 1)) == nullptr) {
            php_error_docref(nullptr, E_WARNING, "no key \"%s\" passed in optval", usec_key);
            RETURN_FALSE;
        }

        convert_to_long_ex(sec);
        convert_to_long_ex(usec);
        tv.tv_sec = Z_LVAL_P(sec);
        tv.tv_usec = Z_LVAL_P(usec);
        sock->socket->set_timeout(&tv,
                                  optname == SO_RCVTIMEO ? SW_TIMEOUT_READ : SW_TIMEOUT_CONNECT | SW_TIMEOUT_WRITE);
        RETURN_TRUE;
        break;
    }
#ifdef SO_BINDTODEVICE
    case SO_BINDTODEVICE: {
        if (Z_TYPE_P(arg4) == IS_STRING) {
            opt_ptr = Z_STRVAL_P(arg4);
            optlen = Z_STRLEN_P(arg4);
        } else {
            opt_ptr = (char *) "";
            optlen = 0;
        }
        break;
    }
#endif

    default:
    default_case:
        convert_to_long_ex(arg4);
        ov = Z_LVAL_P(arg4);

        optlen = sizeof(ov);
        opt_ptr = (char *) &ov;
        break;
    }

    retval = sock->socket->get_socket()->set_option(level, optname, opt_ptr, optlen);
    if (retval != 0) {
        php_swoole_sys_error(E_WARNING, "setsockopt(%d) failed", sock->socket->get_fd());
        RETURN_FALSE;
    }

    RETURN_TRUE;
}

static PHP_METHOD(swoole_socket_coro, cancel) {
    swoole_get_socket_coro(sock, ZEND_THIS);
    zend_long event = SW_EVENT_READ;

    ZEND_PARSE_PARAMETERS_START(0, 1)
    Z_PARAM_OPTIONAL
    Z_PARAM_LONG(event)
    ZEND_PARSE_PARAMETERS_END_EX(RETURN_FALSE);

    RETURN_BOOL(sock->socket->cancel(event == SW_EVENT_READ ? SW_EVENT_READ : SW_EVENT_WRITE));
}

static PHP_METHOD(swoole_socket_coro, setProtocol) {
    swoole_get_socket_coro(sock, ZEND_THIS);
    zval *zset;

    ZEND_PARSE_PARAMETERS_START(1, 1)
    Z_PARAM_ARRAY(zset)
    ZEND_PARSE_PARAMETERS_END_EX(RETURN_FALSE);

    if (php_swoole_array_length(zset) == 0) {
        RETURN_FALSE;
    } else {
        RETURN_BOOL(php_swoole_socket_set_protocol(sock->socket, zset));
    }
}

#ifdef SW_USE_OPENSSL
static PHP_METHOD(swoole_socket_coro, sslHandshake) {
    swoole_get_socket_coro(sock, ZEND_THIS);

    RETURN_BOOL(sock->socket->ssl_handshake());
}
#endif

static PHP_METHOD(swoole_socket_coro, isClosed) {
    RETURN_BOOL(php_swoole_socket_is_closed(ZEND_THIS));
}

static PHP_METHOD(swoole_socket_coro, import) {
    zval *zstream;
    php_stream *stream;

    ZEND_PARSE_PARAMETERS_START(1, 1)
    Z_PARAM_RESOURCE(zstream)
    ZEND_PARSE_PARAMETERS_END();

    php_stream_from_zval(stream, zstream);

    swSocketType type = SW_SOCK_TCP;
    int socket_fd;

    if (php_stream_cast(stream, PHP_STREAM_AS_SOCKETD, (void **) &socket_fd, 1)) {
        /* error supposedly already shown */
        RETURN_FALSE;
    }

    int sock_domain = AF_INET, sock_type = SOCK_STREAM;
    php_sockaddr_storage addr;
    socklen_t addr_len = sizeof(addr);

#ifdef SO_DOMAIN
    socklen_t sock_domain_len = sizeof(sock_domain);
    if (getsockopt(socket_fd, SOL_SOCKET, SO_DOMAIN, &sock_domain, &sock_domain_len) == 0) {
    } else
#endif
        if (getsockname(socket_fd, (struct sockaddr *) &addr, &addr_len) == 0) {
        sock_domain = addr.ss_family;
    } else {
        php_swoole_sys_error(E_WARNING, "getsockname() failed");
        RETURN_FALSE;
    }

#ifdef SO_TYPE
    socklen_t sock_type_len = sizeof(sock_type);
    if (getsockopt(socket_fd, SOL_SOCKET, SO_TYPE, &sock_type, &sock_type_len) < 0) {
        php_swoole_sys_error(E_WARNING, "getsockopt(SOL_SOCKET, SO_TYPE) failed");
        RETURN_FALSE;
    }
#endif

    type = swoole::network::Socket::convert_to_type(sock_domain, sock_type);

    /* determine blocking mode */
    int t = fcntl(socket_fd, F_GETFL);
    if (t < 0) {
        php_swoole_sys_error(E_WARNING, "fcntl(F_GETFL) failed");
        RETURN_FALSE;
    }
    zend_object *object = php_swoole_create_socket_from_fd(socket_fd, type);
    SocketObject *sock = socket_coro_fetch_object(object);

    ZVAL_COPY(&sock->zstream, zstream);
    php_stream_set_option(stream, PHP_STREAM_OPTION_READ_BUFFER, PHP_STREAM_BUFFER_NONE, NULL);
    sock->socket->get_socket()->nonblock = (t & O_NONBLOCK);

    RETURN_OBJ(object);
}
