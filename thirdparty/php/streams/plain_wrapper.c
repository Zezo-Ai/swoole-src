/*
  +----------------------------------------------------------------------+
  | Copyright (c) The PHP Group                                          |
  +----------------------------------------------------------------------+
  | This source file is subject to version 3.01 of the PHP license,      |
  | that is bundled with this package in the file LICENSE, and is        |
  | available through the world-wide-web at the following url:           |
  | https://www.php.net/license/3_01.txt                                 |
  | If you did not receive a copy of the PHP license and are unable to   |
  | obtain it through the world-wide-web, please send a note to          |
  | license@php.net so we can mail you a copy immediately.               |
  +----------------------------------------------------------------------+
  | Authors: Wez Furlong <wez@thebrainroom.com>                          |
  +----------------------------------------------------------------------+
*/

#include "php_swoole.h"
#include "php_open_temporary_file.h"
#include "ext/standard/file.h"
#include "ext/standard/flock_compat.h"
#include "ext/standard/php_filestat.h"

#include <fcntl.h>

#if PHP_VERSION_ID >= 80400
#ifdef HAVE_SYS_WAIT_H
#include <sys/wait.h>
#endif
#ifdef HAVE_SYS_FILE_H
#include <sys/file.h>
#endif
#else
#if HAVE_SYS_WAIT_H
#include <sys/wait.h>
#endif
#if HAVE_SYS_FILE_H
#include <sys/file.h>
#endif
#endif

#ifdef HAVE_SYS_MMAN_H
#include <sys/mman.h>
#endif

#include "thirdparty/php/streams/php_streams_int.h"

#include "swoole_file_hook.h"

#if PHP_VERSION_ID >= 80200
#define sw_php_stream_fopen_from_fd_rel(fd, mode, persistent_id, zero_position)                                         \
    _sw_php_stream_fopen_from_fd((fd), (mode), (persistent_id), (zero_position) STREAMS_REL_CC)
#else
#define sw_php_stream_fopen_from_fd_rel(fd, mode, persistent_id)                                                        \
    _sw_php_stream_fopen_from_fd((fd), (mode), (persistent_id) STREAMS_REL_CC)
#endif

#define sw_php_stream_fopen_from_fd_int_rel(fd, mode, persistent_id)                                                    \
    _sw_php_stream_fopen_from_fd_int((fd), (mode), (persistent_id) STREAMS_REL_CC)

#define sw_php_stream_fopen_rel(path, mode, opened_path, options)                                                       \
    _sw_php_stream_fopen(path, mode, opened_path, options STREAMS_REL_CC)

#if PHP_VERSION_ID < 80100
/* This is a workaround for GCC bug 69602: https://gcc.gnu.org/bugzilla/show_bug.cgi?id=69602 */
#if EAGAIN != EWOULDBLOCK
# define PHP_IS_TRANSIENT_ERROR(err) (err == EAGAIN || err == EWOULDBLOCK)
#else
# define PHP_IS_TRANSIENT_ERROR(err) (err == EAGAIN)
#endif
#endif

#ifndef PHP_WIN32
extern int php_get_uid_by_name(const char *name, uid_t *uid);
extern int php_get_gid_by_name(const char *name, gid_t *gid);
#endif

#if defined(PHP_WIN32)
#define PLAIN_WRAP_BUF_SIZE(st) (((st) > UINT_MAX) ? UINT_MAX : (unsigned int) (st))

#if PHP_VERSION_ID >= 80100
#define fsync _commit
#define fdatasync fsync
#endif

#else
#define PLAIN_WRAP_BUF_SIZE(st) (st)

#if PHP_VERSION_ID >= 80100
#if !defined(HAVE_FDATASYNC)
#define fdatasync fsync
#elif defined(__APPLE__)
// The symbol is present, however not in the headers
extern int fdatasync(int);
#endif
#endif

#endif

static php_stream_size_t sw_php_stdiop_write(php_stream *stream, const char *buf, size_t count);
static php_stream_size_t sw_php_stdiop_read(php_stream *stream, char *buf, size_t count);
static int sw_php_stdiop_close(php_stream *stream, int close_handle);
static int sw_php_stdiop_stat(php_stream *stream, php_stream_statbuf *ssb);
static int sw_php_stdiop_flush(php_stream *stream);
static int sw_php_stdiop_seek(php_stream *stream, zend_off_t offset, int whence, zend_off_t *newoffset);
static int sw_php_stdiop_set_option(php_stream *stream, int option, int value, void *ptrparam);
static int sw_php_stdiop_cast(php_stream *stream, int castas, void **ret);
static void php_stream_mode_sanitize_fdopen_fopencookie(php_stream *stream, char *result);
static php_stream *_sw_php_stream_fopen_from_fd_int(int fd, const char *mode, const char *persistent_id STREAMS_DC);
#if PHP_VERSION_ID >= 80200
static php_stream *_sw_php_stream_fopen_from_fd(int fd, const char *mode, const char *persistent_id, bool zero_position STREAMS_DC);
#else
static php_stream *_sw_php_stream_fopen_from_fd(int fd, const char *mode, const char *persistent_id STREAMS_DC);
#endif
static int sw_php_mkdir(const char *dir, zend_long mode);

static int sw_php_stream_parse_fopen_modes(const char *mode, int *open_flags) {
    int flags;

    switch (mode[0]) {
    case 'r':
        flags = 0;
        break;
    case 'w':
        flags = O_TRUNC | O_CREAT;
        break;
    case 'a':
        flags = O_CREAT | O_APPEND;
        break;
    case 'x':
        flags = O_CREAT | O_EXCL;
        break;
    case 'c':
        flags = O_CREAT;
        break;
    default:
        /* unknown mode */
        return FAILURE;
    }

    if (strchr(mode, '+')) {
        flags |= O_RDWR;
    } else if (flags) {
        flags |= O_WRONLY;
    } else {
        flags |= O_RDONLY;
    }

#if defined(O_CLOEXEC)
    if (strchr(mode, 'e')) {
        flags |= O_CLOEXEC;
    }
#endif

#if defined(O_NONBLOCK)
    if (strchr(mode, 'n')) {
        flags |= O_NONBLOCK;
    }
#endif

#if defined(_O_TEXT) && defined(O_BINARY)
    if (strchr(mode, 't')) {
        flags |= _O_TEXT;
    } else {
        flags |= O_BINARY;
    }
#endif

    *open_flags = flags;
    return SUCCESS;
}


/* {{{ ------- STDIO stream implementation -------*/

typedef struct {
    FILE *file;
    int fd;					/* underlying file descriptor */
    unsigned is_process_pipe:1;	                /* use pclose instead of fclose */
    unsigned is_pipe:1;		                /* stream is an actual pipe, currently Windows only*/
    unsigned cached_fstat:1;	                /* sb is valid */
    unsigned is_pipe_blocking:1;                /* allow blocking read() on pipes, currently Windows only */
    unsigned no_forced_fstat:1;                 /* Use fstat cache even if forced */
    unsigned is_seekable:1;		        /* don't try and seek, if not set */
    unsigned can_poll:1;
    unsigned _reserved:25;

    int lock_flag;			        /* stores the lock state */
    zend_string *temp_name;	                /* if non-null, this is the path to a temporary file that
						 * is to be deleted when the stream is closed */
#ifdef HAVE_FLUSHIO
    char last_op;
#endif

#ifdef HAVE_MMAP
    char *last_mapped_addr;
    size_t last_mapped_len;
#endif
#ifdef PHP_WIN32
    char *last_mapped_addr;
    HANDLE file_mapping;
#endif

    zend_stat_t sb;
} php_stdio_stream_data;
#define PHP_STDIOP_GET_FD(anfd, data)	anfd = (data)->file ? fileno((data)->file) : (data)->fd

static php_stream_ops sw_php_stream_stdio_ops = {
    sw_php_stdiop_write,
    sw_php_stdiop_read,
    sw_php_stdiop_close,
    sw_php_stdiop_flush,
    "STDIO/coroutine",
    sw_php_stdiop_seek,
    sw_php_stdiop_cast,
    sw_php_stdiop_stat,
    sw_php_stdiop_set_option,
};

static int do_fstat(php_stdio_stream_data *d, int force) {
    if (!d->cached_fstat || force) {
        int fd;
        int r;

        PHP_STDIOP_GET_FD(fd, d);
        r = fstat(fd, &d->sb);
        d->cached_fstat = r == 0;

        return r;
    }
    return 0;
}

static php_stream *_sw_php_stream_fopen_from_fd_int(int fd, const char *mode, const char *persistent_id STREAMS_DC) {
    php_stdio_stream_data *self = (php_stdio_stream_data *) pemalloc_rel_orig(sizeof(*self), persistent_id);
    memset(self, 0, sizeof(*self));
    self->file = NULL;
    self->is_seekable = 1;
    self->is_pipe = 0;
    self->lock_flag = LOCK_UN;
    self->is_process_pipe = 0;
    self->temp_name = NULL;
    self->fd = fd;
#ifdef PHP_WIN32
    self->is_pipe_blocking = 0;
#endif
    return php_stream_alloc_rel(&sw_php_stream_stdio_ops, self, persistent_id, mode);
}

static void _sw_detect_is_seekable(php_stdio_stream_data *self) {
#if defined(S_ISFIFO) && defined(S_ISCHR)
    if (self->fd >= 0 && do_fstat(self, 0) == 0) {
        self->is_seekable = !(S_ISFIFO(self->sb.st_mode) || S_ISCHR(self->sb.st_mode));
        self->is_pipe = S_ISFIFO(self->sb.st_mode);
        self->can_poll = S_ISFIFO(self->sb.st_mode) || S_ISSOCK(self->sb.st_mode) || S_ISCHR(self->sb.st_mode);
        if (self->can_poll) {
            swoole_coroutine_socket_create(self->fd);
        }
    }
#elif defined(PHP_WIN32)
#if PHP_VERSION_ID >= 80300
    uintptr_t handle = _get_osfhandle(self->fd);

    if (handle != (uintptr_t)INVALID_HANDLE_VALUE) {
#else
    zend_uintptr_t handle = _get_osfhandle(self->fd);

    if (handle != (zend_uintptr_t)INVALID_HANDLE_VALUE) {
#endif
        DWORD file_type = GetFileType((HANDLE)handle);

        self->is_seekable = !(file_type == FILE_TYPE_PIPE || file_type == FILE_TYPE_CHAR);
        self->is_pipe = file_type == FILE_TYPE_PIPE;

        /* Additional check needed to distinguish between pipes and sockets. */
        if (self->is_pipe && !GetNamedPipeInfo((HANDLE) handle, NULL, NULL, NULL, NULL)) {
            self->is_pipe = 0;
        }
    }
#endif
}

#if PHP_VERSION_ID >= 80200
static php_stream *_sw_php_stream_fopen_from_fd(int fd, const char *mode, const char *persistent_id, bool zero_position STREAMS_DC) {
#else
static php_stream *_sw_php_stream_fopen_from_fd(int fd, const char *mode, const char *persistent_id STREAMS_DC) {
#endif
    php_stream *stream = sw_php_stream_fopen_from_fd_int_rel(fd, mode, persistent_id);

    if (stream) {
        php_stdio_stream_data *self = (php_stdio_stream_data *) stream->abstract;

        _sw_detect_is_seekable(self);
        if (!self->is_seekable) {
            stream->flags |= PHP_STREAM_FLAG_NO_SEEK;
            stream->position = -1;
        }
#if PHP_VERSION_ID >= 80200
        else if (zero_position) {
            ZEND_ASSERT(lseek(self->fd, 0, SEEK_CUR) == 0);
            stream->position = 0;
        }
#endif
        else {
            stream->position = lseek(self->fd, 0, SEEK_CUR);
#ifdef ESPIPE
            /* FIXME: Is this code still needed? */
            if (stream->position == (zend_off_t)-1 && errno == ESPIPE) {
                stream->flags |= PHP_STREAM_FLAG_NO_SEEK;
                self->is_seekable = 0;
            }
#endif
        }
    }

    return stream;
}

static php_stream_size_t sw_php_stdiop_write(php_stream *stream, const char *buf, size_t count) {
    php_stdio_stream_data *data = (php_stdio_stream_data *) stream->abstract;

    assert(data != NULL);

    if (data->fd >= 0) {
#ifdef PHP_WIN32
        ssize_t bytes_written;
        if (ZEND_SIZE_T_UINT_OVFL(count)) {
            count = UINT_MAX;
        }
        bytes_written = _write(data->fd, buf, (unsigned int)count);
#else
        php_stdio_stream_data *self = (php_stdio_stream_data *) stream->abstract;
        ssize_t bytes_written = write(data->fd, buf, count);
#endif
        if (bytes_written < 0) {
            if (PHP_IS_TRANSIENT_ERROR(errno)) {
                return 0;
            }
            if (errno == EINTR) {
                /* TODO: Should this be treated as a proper error or not? */
                return bytes_written;
            }
            if (!(stream->flags & PHP_STREAM_FLAG_SUPPRESS_ERRORS)) {
                php_error_docref(NULL, E_NOTICE, "Write of %zu bytes failed with errno=%d %s", count, errno, strerror(errno));
            }
        }
        return bytes_written;
    } else {
#ifdef HAVE_FLUSHIO
        if (data->is_seekable && data->last_op == 'r') {
            fseek(data->file, 0, SEEK_CUR);
        }
        data->last_op = 'w';
#endif

        return (ssize_t) fwrite(buf, 1, count, data->file);
    }
}

static php_stream_size_t sw_php_stdiop_read(php_stream *stream, char *buf, size_t count) {
    php_stdio_stream_data *data = (php_stdio_stream_data *) stream->abstract;
    ssize_t ret;

    assert(data != NULL);

    if (data->fd >= 0) {
#ifdef PHP_WIN32
        php_stdio_stream_data *self = (php_stdio_stream_data*)stream->abstract;

        if ((self->is_pipe || self->is_process_pipe) && !self->is_pipe_blocking) {
            HANDLE ph = (HANDLE)_get_osfhandle(data->fd);
            int retry = 0;
            DWORD avail_read = 0;

            do {
                /* Look ahead to get the available data amount to read. Do the same
                        as read() does, however not blocking forever. In case it failed,
                        no data will be read (better than block). */
                if (!PeekNamedPipe(ph, NULL, 0, NULL, &avail_read, NULL)) {
                    break;
                }
                /* If there's nothing to read, wait in 10us periods. */
                if (0 == avail_read) {
                    usleep(10);
                }
            } while (0 == avail_read && retry++ < 3200000);

            /* Reduce the required data amount to what is available, otherwise read()
                    will block.*/
            if (avail_read < count) {
                count = avail_read;
            }
        }
#endif
        php_stdio_stream_data *self = (php_stdio_stream_data *) stream->abstract;
        ret = read(data->fd, buf, PLAIN_WRAP_BUF_SIZE(count));

        if (ret == (ssize_t) -1 && errno == EINTR) {
            /* Read was interrupted, retry once,
             If read still fails, giveup with feof==0
             so script can retry if desired */
            ret = read(data->fd, buf, PLAIN_WRAP_BUF_SIZE(count));
        }

        if (ret < 0) {
            if (PHP_IS_TRANSIENT_ERROR(errno)) {
                /* Not an error. */
                ret = 0;
            } else if (errno == EINTR) {
                /* TODO: Should this be treated as a proper error or not? */
            } else {
                if (!(stream->flags & PHP_STREAM_FLAG_SUPPRESS_ERRORS)) {
                    php_error_docref(NULL, E_NOTICE, "Read of %zu bytes failed with errno=%d %s", count, errno, strerror(errno));
                }

                /* TODO: Remove this special-case? */
                if (errno != EBADF) {
                    stream->eof = 1;
                }
            }
        } else if (ret == 0) {
            stream->eof = 1;
        }

    } else {
#ifdef HAVE_FLUSHIO
        if (data->is_seekable && data->last_op == 'w')
            fseek(data->file, 0, SEEK_CUR);
        data->last_op = 'r';
#endif
        ret = fread(buf, 1, count, data->file);

        stream->eof = feof(data->file);
    }
    return ret;
}

static int sw_php_stdiop_close(php_stream *stream, int close_handle) {
    int ret;
    php_stdio_stream_data *data = (php_stdio_stream_data *) stream->abstract;

    assert(data != NULL);

#ifdef HAVE_MMAP
    if (data->last_mapped_addr) {
        munmap(data->last_mapped_addr, data->last_mapped_len);
        data->last_mapped_addr = NULL;
    }
#elif defined(PHP_WIN32)
    if (data->last_mapped_addr) {
        UnmapViewOfFile(data->last_mapped_addr);
        data->last_mapped_addr = NULL;
    }
    if (data->file_mapping) {
        CloseHandle(data->file_mapping);
        data->file_mapping = NULL;
    }
#endif

    if (close_handle) {
        if (data->file) {
            if (data->is_process_pipe) {
                errno = 0;
                ret = pclose(data->file);

#if HAVE_SYS_WAIT_H
                if (WIFEXITED(ret)) {
                    ret = WEXITSTATUS(ret);
                }
#endif
            } else {
                ret = fclose(data->file);
                data->file = NULL;
            }
        } else if (data->fd != -1) {
            if ((data->lock_flag & LOCK_EX) || (data->lock_flag & LOCK_SH)) {
                swoole_coroutine_flock(data->fd, LOCK_UN);
            }
            ret = close(data->fd);
            data->fd = -1;
        } else {
            return 0; /* everything should be closed already -> success */
        }
        if (data->temp_name) {
#ifdef PHP_WIN32
            php_win32_ioutil_unlink(ZSTR_VAL(data->temp_name));
#else
            unlink(ZSTR_VAL(data->temp_name));
#endif
            /* temporary streams are never persistent */
            zend_string_release_ex(data->temp_name, 0);
            data->temp_name = NULL;
        }
    } else {
        ret = 0;
        data->file = NULL;
        data->fd = -1;
    }

    pefree(data, stream->is_persistent);

    return ret;
}

static int sw_php_stdiop_flush(php_stream *stream) {
    php_stdio_stream_data *data = (php_stdio_stream_data *) stream->abstract;

    assert(data != NULL);

    /*
     * stdio buffers data in user land. By calling fflush(3), this
     * data is send to the kernel using write(2). fsync'ing is
     * something completely different.
     */
    if (data->file) {
        return fflush(data->file);
    }
    return 0;
}

#if PHP_VERSION_ID >= 80100
static int php_stdiop_sync(php_stream *stream, bool dataonly)
{
    php_stdio_stream_data *data = (php_stdio_stream_data*)stream->abstract;
    FILE *fp;
    int fd;

    if (php_stream_cast(stream, PHP_STREAM_AS_STDIO, (void**)&fp, REPORT_ERRORS) == FAILURE) {
        return -1;
    }

    if (sw_php_stdiop_flush(stream) == 0) {
        PHP_STDIOP_GET_FD(fd, data);
        if (dataonly) {
            return fdatasync(fd);
        } else {
            return fsync(fd);
        }
    }
    return -1;
}
#endif

static int sw_php_stdiop_seek(php_stream *stream, zend_off_t offset, int whence, zend_off_t *newoffset) {
    php_stdio_stream_data *data = (php_stdio_stream_data *) stream->abstract;
    int ret;

    assert(data != NULL);

    if (!data->is_seekable) {
        php_error_docref(NULL, E_WARNING, "Cannot seek on this stream");
        return -1;
    }

    if (data->fd >= 0) {
        zend_off_t result;

        result = lseek(data->fd, offset, whence);
        if (result == (zend_off_t) -1) return -1;

        *newoffset = result;
        return 0;

    } else {
        ret = fseek(data->file, offset, whence);
        *newoffset = ftell(data->file);
        return ret;
    }
}

static int sw_php_stdiop_cast(php_stream *stream, int castas, void **ret) {
    php_socket_t fd;
    php_stdio_stream_data *data = (php_stdio_stream_data *) stream->abstract;

    assert(data != NULL);

    /* as soon as someone touches the stdio layer, buffering may ensue,
     * so we need to stop using the fd directly in that case */

    switch (castas) {
    case PHP_STREAM_AS_STDIO:
        if (ret) {
            if (data->file == NULL) {
                /* we were opened as a plain file descriptor, so we
                 * need fdopen now */
                char fixed_mode[5];
                php_stream_mode_sanitize_fdopen_fopencookie(stream, fixed_mode);
                data->file = fdopen(data->fd, fixed_mode);
                if (data->file == NULL) {
                    return FAILURE;
                }
            }

            *(FILE **) ret = data->file;
            data->fd = SOCK_ERR;
        }
        return SUCCESS;

    case PHP_STREAM_AS_FD_FOR_SELECT:
        PHP_STDIOP_GET_FD(fd, data);
        if (SOCK_ERR == fd) {
            return FAILURE;
        }
        if (ret) {
            *(php_socket_t *) ret = fd;
        }
        return SUCCESS;

    case PHP_STREAM_AS_FD:
        PHP_STDIOP_GET_FD(fd, data);

        if (SOCK_ERR == fd) {
            return FAILURE;
        }
        if (data->file) {
            fflush(data->file);
        }
        if (ret) {
            *(php_socket_t *) ret = fd;
        }
        return SUCCESS;
    default:
        return FAILURE;
    }
}

static int sw_php_stdiop_stat(php_stream *stream, php_stream_statbuf *ssb) {
    int ret;
    php_stdio_stream_data *data = (php_stdio_stream_data *) stream->abstract;

    assert(data != NULL);
    if ((ret = do_fstat(data, 1)) == 0) {
        memcpy(&ssb->sb, &data->sb, sizeof(ssb->sb));
    }

    return ret;
}

static int sw_php_stdiop_set_option(php_stream *stream, int option, int value, void *ptrparam) {
    php_stdio_stream_data *data = (php_stdio_stream_data *) stream->abstract;
    size_t size;
    int fd;
#ifdef O_NONBLOCK
    /* FIXME: make this work for win32 */
    int flags;
    int oldval;
#endif

    PHP_STDIOP_GET_FD(fd, data);

    switch (option) {
    case PHP_STREAM_OPTION_BLOCKING:
        if (fd == -1) return -1;
#ifdef O_NONBLOCK
        flags = fcntl(fd, F_GETFL, 0);
        oldval = (flags & O_NONBLOCK) ? 0 : 1;
        if (value)
            flags &= ~O_NONBLOCK;
        else
            flags |= O_NONBLOCK;

        if (-1 == fcntl(fd, F_SETFL, flags)) return -1;
        return oldval;
#else
        return -1; /* not yet implemented */
#endif

    case PHP_STREAM_OPTION_WRITE_BUFFER:

        if (data->file == NULL) {
            return -1;
        }

        if (ptrparam)
            size = *(size_t *) ptrparam;
        else
            size = BUFSIZ;

        switch (value) {
        case PHP_STREAM_BUFFER_NONE:
            return setvbuf(data->file, NULL, _IONBF, 0);

        case PHP_STREAM_BUFFER_LINE:
            return setvbuf(data->file, NULL, _IOLBF, size);

        case PHP_STREAM_BUFFER_FULL:
            return setvbuf(data->file, NULL, _IOFBF, size);

        default:
            return -1;
        }
        break;

    case PHP_STREAM_OPTION_LOCKING:
        if (fd == -1) {
            return -1;
        }
#if PHP_VERSION_ID >= 80300
        if ((uintptr_t) ptrparam == PHP_STREAM_LOCK_SUPPORTED) {
            return 0;
        }
#else
        if ((zend_uintptr_t) ptrparam == PHP_STREAM_LOCK_SUPPORTED) {
            return 0;
        }
#endif

        if (!swoole_coroutine_flock(fd, value)) {
            data->lock_flag = value;
            return 0;
        } else {
            return -1;
        }
        break;

    case PHP_STREAM_OPTION_MMAP_API:
#if HAVE_MMAP
    {
        php_stream_mmap_range *range = (php_stream_mmap_range*)ptrparam;
        int prot, flags;

        switch (value) {
        case PHP_STREAM_MMAP_SUPPORTED:
            return fd == -1 ? PHP_STREAM_OPTION_RETURN_ERR : PHP_STREAM_OPTION_RETURN_OK;

        case PHP_STREAM_MMAP_MAP_RANGE:
            if (do_fstat(data, 1) != 0) {
                return PHP_STREAM_OPTION_RETURN_ERR;
            }
            if (range->offset > (size_t) data->sb.st_size) {
                range->offset = data->sb.st_size;
            }
            if (range->length == 0 ||
                range->length > data->sb.st_size - range->offset) {
                range->length = data->sb.st_size - range->offset;
            }
            switch (range->mode) {
            case PHP_STREAM_MAP_MODE_READONLY:
                prot = PROT_READ;
                flags = MAP_PRIVATE;
                break;
            case PHP_STREAM_MAP_MODE_READWRITE:
                prot = PROT_READ | PROT_WRITE;
                flags = MAP_PRIVATE;
                break;
            case PHP_STREAM_MAP_MODE_SHARED_READONLY:
                prot = PROT_READ;
                flags = MAP_SHARED;
                break;
            case PHP_STREAM_MAP_MODE_SHARED_READWRITE:
                prot = PROT_READ | PROT_WRITE;
                flags = MAP_SHARED;
                break;
            default:
                return PHP_STREAM_OPTION_RETURN_ERR;
            }
            range->mapped = (char*)mmap(NULL, range->length, prot, flags, fd, range->offset);
            if (range->mapped == (char*)MAP_FAILED) {
                range->mapped = NULL;
                return PHP_STREAM_OPTION_RETURN_ERR;
            }
            /* remember the mapping */
            data->last_mapped_addr = range->mapped;
            data->last_mapped_len = range->length;
            return PHP_STREAM_OPTION_RETURN_OK;

        case PHP_STREAM_MMAP_UNMAP:
            if (data->last_mapped_addr) {
                munmap(data->last_mapped_addr, data->last_mapped_len);
                data->last_mapped_addr = NULL;

                return PHP_STREAM_OPTION_RETURN_OK;
            }
            return PHP_STREAM_OPTION_RETURN_ERR;
        }
    }
#elif defined(PHP_WIN32)
    {
        php_stream_mmap_range *range = (php_stream_mmap_range*)ptrparam;
        HANDLE hfile = (HANDLE)_get_osfhandle(fd);
        DWORD prot, acc, loffs = 0, hoffs = 0, delta = 0;
        LARGE_INTEGER file_size;

        switch (value) {
        case PHP_STREAM_MMAP_SUPPORTED:
            return hfile == INVALID_HANDLE_VALUE ? PHP_STREAM_OPTION_RETURN_ERR : PHP_STREAM_OPTION_RETURN_OK;

        case PHP_STREAM_MMAP_MAP_RANGE:
            switch (range->mode) {
            case PHP_STREAM_MAP_MODE_READONLY:
                prot = PAGE_READONLY;
                acc = FILE_MAP_READ;
                break;
            case PHP_STREAM_MAP_MODE_READWRITE:
                prot = PAGE_READWRITE;
                acc = FILE_MAP_READ | FILE_MAP_WRITE;
                break;
            case PHP_STREAM_MAP_MODE_SHARED_READONLY:
                prot = PAGE_READONLY;
                acc = FILE_MAP_READ;
                /* TODO: we should assign a name for the mapping */
                break;
            case PHP_STREAM_MAP_MODE_SHARED_READWRITE:
                prot = PAGE_READWRITE;
                acc = FILE_MAP_READ | FILE_MAP_WRITE;
                /* TODO: we should assign a name for the mapping */
                break;
            default:
                return PHP_STREAM_OPTION_RETURN_ERR;
            }

            /* create a mapping capable of viewing the whole file (this costs no real resources) */
            data->file_mapping = CreateFileMapping(hfile, NULL, prot, 0, 0, NULL);

            if (data->file_mapping == NULL) {
                return PHP_STREAM_OPTION_RETURN_ERR;
            }

            if (!GetFileSizeEx(hfile, &file_size)) {
                CloseHandle(data->file_mapping);
                data->file_mapping = NULL;
                return PHP_STREAM_OPTION_RETURN_ERR;
            }
# if defined(_WIN64)
            size = file_size.QuadPart;
# else
            if (file_size.HighPart) {
                CloseHandle(data->file_mapping);
                data->file_mapping = NULL;
                return PHP_STREAM_OPTION_RETURN_ERR;
            } else {
                size = file_size.LowPart;
            }
# endif
            if (range->offset > size) {
                range->offset = size;
            }
            if (range->length == 0 || range->length > size - range->offset) {
                range->length = size - range->offset;
            }

            /* figure out how big a chunk to map to be able to view the part that we need */
            if (range->offset != 0) {
                SYSTEM_INFO info;
                DWORD gran;

                GetSystemInfo(&info);
                gran = info.dwAllocationGranularity;
                ZEND_ASSERT(gran != 0 && (gran & (gran - 1)) == 0);
                size_t rounded_offset = (range->offset / gran) * gran;
                delta = range->offset - rounded_offset;
                loffs = (DWORD)rounded_offset;
                hoffs = (DWORD)(rounded_offset >> 32);
            }

            /* MapViewOfFile()ing zero bytes would map to the end of the file; match *nix behavior instead */
            if (range->length + delta == 0) {
                return PHP_STREAM_OPTION_RETURN_ERR;
            }

            data->last_mapped_addr = MapViewOfFile(data->file_mapping, acc, hoffs, loffs, range->length + delta);

            if (data->last_mapped_addr) {
                /* give them back the address of the start offset they requested */
                range->mapped = data->last_mapped_addr + delta;
                return PHP_STREAM_OPTION_RETURN_OK;
            }

            CloseHandle(data->file_mapping);
            data->file_mapping = NULL;

            return PHP_STREAM_OPTION_RETURN_ERR;

        case PHP_STREAM_MMAP_UNMAP:
            if (data->last_mapped_addr) {
                UnmapViewOfFile(data->last_mapped_addr);
                data->last_mapped_addr = NULL;
                CloseHandle(data->file_mapping);
                data->file_mapping = NULL;
                return PHP_STREAM_OPTION_RETURN_OK;
            }
            return PHP_STREAM_OPTION_RETURN_ERR;

        default:
            return PHP_STREAM_OPTION_RETURN_ERR;
        }
    }
#endif
        return PHP_STREAM_OPTION_RETURN_NOTIMPL;

#if PHP_VERSION_ID >= 80100
    case PHP_STREAM_OPTION_SYNC_API:
        switch (value) {
        case PHP_STREAM_SYNC_SUPPORTED:
            return fd == -1 ? PHP_STREAM_OPTION_RETURN_ERR : PHP_STREAM_OPTION_RETURN_OK;
        case PHP_STREAM_SYNC_FSYNC:
            return php_stdiop_sync(stream, 0) == 0 ? PHP_STREAM_OPTION_RETURN_OK : PHP_STREAM_OPTION_RETURN_ERR;
        case PHP_STREAM_SYNC_FDSYNC:
            return php_stdiop_sync(stream, 1) == 0 ? PHP_STREAM_OPTION_RETURN_OK : PHP_STREAM_OPTION_RETURN_ERR;
        }
        /* Invalid option passed */
        return PHP_STREAM_OPTION_RETURN_ERR;
#endif

    case PHP_STREAM_OPTION_TRUNCATE_API:
        switch (value) {
        case PHP_STREAM_TRUNCATE_SUPPORTED:
            return fd == -1 ? PHP_STREAM_OPTION_RETURN_ERR : PHP_STREAM_OPTION_RETURN_OK;

        case PHP_STREAM_TRUNCATE_SET_SIZE: {
            ptrdiff_t new_size = *(ptrdiff_t*)ptrparam;
            if (new_size < 0) {
                return PHP_STREAM_OPTION_RETURN_ERR;
            }
#ifdef PHP_WIN32
            HANDLE h = (HANDLE) _get_osfhandle(fd);
            if (INVALID_HANDLE_VALUE == h) {
                return PHP_STREAM_OPTION_RETURN_ERR;
            }

            LARGE_INTEGER sz, old_sz;
            sz.QuadPart = 0;

            if (!SetFilePointerEx(h, sz, &old_sz, FILE_CURRENT)) {
                return PHP_STREAM_OPTION_RETURN_ERR;
            }

#ifdef _WIN64
            sz.QuadPart = new_size;
#else
            sz.HighPart = 0;
            sz.LowPart = new_size;
#endif
            if (!SetFilePointerEx(h, sz, NULL, FILE_BEGIN)) {
                return PHP_STREAM_OPTION_RETURN_ERR;
            }
            if (0 == SetEndOfFile(h)) {
                return PHP_STREAM_OPTION_RETURN_ERR;
            }
            if (!SetFilePointerEx(h, old_sz, NULL, FILE_BEGIN)) {
                return PHP_STREAM_OPTION_RETURN_ERR;
            }
            return PHP_STREAM_OPTION_RETURN_OK;
#else
            return ftruncate(fd, new_size) == 0 ? PHP_STREAM_OPTION_RETURN_OK : PHP_STREAM_OPTION_RETURN_ERR;
#endif
        }
        }
        return PHP_STREAM_OPTION_RETURN_NOTIMPL;

#ifdef PHP_WIN32
    case PHP_STREAM_OPTION_PIPE_BLOCKING:
        data->is_pipe_blocking = value;
        return PHP_STREAM_OPTION_RETURN_OK;
#endif
    case PHP_STREAM_OPTION_META_DATA_API:
        if (fd == -1) return -1;
#ifdef O_NONBLOCK
        flags = fcntl(fd, F_GETFL, 0);

        add_assoc_bool((zval *) ptrparam, "timed_out", 0);
        add_assoc_bool((zval *) ptrparam, "blocked", (flags & O_NONBLOCK) ? 0 : 1);
        add_assoc_bool((zval *) ptrparam, "eof", stream->eof);

        return PHP_STREAM_OPTION_RETURN_OK;
#endif
        return -1;
    default:
        return PHP_STREAM_OPTION_RETURN_NOTIMPL;
    }
}
/* }}} */

/* {{{ plain files opendir/readdir implementation */
static php_stream_size_t php_plain_files_dirstream_read(php_stream *stream, char *buf, size_t count) {
    DIR *dir = (DIR *) stream->abstract;
    struct dirent *result;
    php_stream_dirent *ent = (php_stream_dirent *) buf;

    /* avoid problems if someone mis-uses the stream */
    if (count != sizeof(php_stream_dirent)) {
        return -1;
    }

    result = readdir(dir);
    if (result) {
        PHP_STRLCPY(ent->d_name, result->d_name, sizeof(ent->d_name), strlen(result->d_name));
#if PHP_VERSION_ID >= 80300
#ifdef _DIRENT_HAVE_D_TYPE
        ent->d_type = result->d_type;
#else
        ent->d_type = DT_UNKNOWN;
#endif
#endif
        return sizeof(php_stream_dirent);
    }
    return 0;
}

static int php_plain_files_dirstream_close(php_stream *stream, int close_handle) {
    return closedir((DIR *) stream->abstract);
}

static int php_plain_files_dirstream_rewind(php_stream *stream, zend_off_t offset, int whence, zend_off_t *newoffs) {
    rewinddir((DIR *) stream->abstract);
    return 0;
}

static php_stream_ops php_plain_files_dirstream_ops = {
    NULL,
    php_plain_files_dirstream_read,
    php_plain_files_dirstream_close,
    NULL,
    "dir",
    php_plain_files_dirstream_rewind,
    NULL, /* cast */
    NULL, /* stat */
    NULL  /* set_option */
};

static php_stream *php_plain_files_dir_opener(php_stream_wrapper *wrapper,
                                              const char *path,
                                              const char *mode,
                                              int options,
                                              zend_string **opened_path,
                                              php_stream_context *context STREAMS_DC) {
    DIR *dir = NULL;
    php_stream *stream = NULL;

#ifdef HAVE_GLOB
    if (options & STREAM_USE_GLOB_DIR_OPEN) {
        return php_glob_stream_wrapper.wops->dir_opener(
            (php_stream_wrapper *) &php_glob_stream_wrapper, path, mode, options, opened_path, context STREAMS_REL_CC);
    }
#endif

    if (((options & STREAM_DISABLE_OPEN_BASEDIR) == 0) && php_check_open_basedir(path)) {
        return NULL;
    }

    dir = opendir(path);

#ifdef PHP_WIN32
    if (!dir) {
        php_win32_docref2_from_error(GetLastError(), path, path);
    }

    if (dir && dir->finished) {
        closedir(dir);
        dir = NULL;
    }
#endif
    if (dir) {
        stream = php_stream_alloc(&php_plain_files_dirstream_ops, dir, 0, mode);
        if (stream == NULL) closedir(dir);
    }

    return stream;
}
/* }}} */

static php_stream *_sw_php_stream_fopen(const char *filename,
                                    const char *mode,
                                    zend_string **opened_path,
                                    int options STREAMS_DC) {
    char _realpath[MAXPATHLEN];
    int open_flags;
    int fd;
    php_stream *ret;
    int persistent = options & STREAM_OPEN_PERSISTENT;
    char *persistent_id = NULL;

    if (FAILURE == sw_php_stream_parse_fopen_modes(mode, &open_flags)) {
        php_stream_wrapper_log_error(&php_plain_files_wrapper, options, "`%s' is not a valid mode for fopen", mode);
        return NULL;
    }

    if (options & STREAM_ASSUME_REALPATH) {
        strlcpy(_realpath, filename, sizeof(_realpath));
    } else {
        if (expand_filepath(filename, _realpath) == NULL) {
            return NULL;
        }
    }

    if (persistent) {
        spprintf(&persistent_id, 0, "streams_stdio_%d_%s", open_flags, _realpath);
        switch (php_stream_from_persistent_id(persistent_id, &ret)) {
        case PHP_STREAM_PERSISTENT_SUCCESS:
            if (opened_path) {
                // TODO: avoid reallocation???
                *opened_path = zend_string_init(_realpath, strlen(_realpath), 0);
            }
            /* fall through */
#if PHP_VERSION_ID >= 80100
            ZEND_FALLTHROUGH;
#endif

        case PHP_STREAM_PERSISTENT_FAILURE:
            efree(persistent_id);
            return ret;
        }
    }
#ifdef PHP_WIN32
    fd = php_win32_ioutil_open(_realpath, open_flags, 0666);
#else
    fd = open(_realpath, open_flags, 0666);
#endif
    if (fd != -1) {
#if PHP_VERSION_ID >= 80200
        ret = sw_php_stream_fopen_from_fd_rel(fd, mode, persistent_id, (open_flags & O_APPEND) == 0);
#else
        ret = sw_php_stream_fopen_from_fd_rel(fd, mode, persistent_id);
#endif
        if (ret) {
            if (opened_path) {
                *opened_path = zend_string_init(_realpath, strlen(_realpath), 0);
            }
            if (persistent_id) {
                efree(persistent_id);
            }

            /* WIN32 always set ISREG flag */
#ifndef PHP_WIN32
            /* sanity checks for include/require.
             * We check these after opening the stream, so that we save
             * on fstat() syscalls */
            if (options & STREAM_OPEN_FOR_INCLUDE) {
                php_stdio_stream_data *self = (php_stdio_stream_data *) ret->abstract;
                int r;

                r = do_fstat(self, 0);
                if ((r == 0 && !S_ISREG(self->sb.st_mode))) {
                    if (opened_path) {
                        zend_string_release_ex(*opened_path, 0);
                        *opened_path = NULL;
                    }
                    php_stream_close(ret);
                    return NULL;
                }

                /* Make sure the fstat result is reused when we later try to get the
                 * file size. */
                self->no_forced_fstat = 1;
            }

            if (options & STREAM_USE_BLOCKING_PIPE) {
                php_stdio_stream_data *self = (php_stdio_stream_data *) ret->abstract;
                self->is_pipe_blocking = 1;
            }
#endif

            return ret;
        }
        close(fd);
    }
    if (persistent_id) {
        efree(persistent_id);
    }
    return NULL;
}

static php_stream *php_plain_files_stream_opener(php_stream_wrapper *wrapper,
                                 const char *path,
                                 const char *mode,
                                 int options,
                                 zend_string **opened_path,
                                 php_stream_context *context STREAMS_DC) {
    if (((options & STREAM_DISABLE_OPEN_BASEDIR) == 0) && php_check_open_basedir(path)) {
        return NULL;
    }

    php_stream *stream;
    /** phar_open_archive_fp, cannot use async-io */
    if (EG(current_execute_data) && EG(current_execute_data)->func &&
        ZEND_USER_CODE(EG(current_execute_data)->func->type)) {
        const zend_op *opline = EG(current_execute_data)->opline;
        if (opline && opline->opcode == ZEND_INCLUDE_OR_EVAL &&
            (opline->extended_value & (ZEND_INCLUDE | ZEND_INCLUDE_ONCE | ZEND_REQUIRE | ZEND_REQUIRE_ONCE))) {
            goto _open_for_include;
        }
    }
    /** include file, cannot use async-io */
    if (options & STREAM_OPEN_FOR_INCLUDE) {
    _open_for_include:
        stream = php_stream_fopen_rel(path, mode, opened_path, options);
        if (stream == NULL) {
            return NULL;
        }
        stream->ops = php_swoole_get_ori_php_stream_stdio_ops();
        return stream;
    }

    return sw_php_stream_fopen_rel(path, mode, opened_path, options);
}

static int php_plain_files_url_stater(
    php_stream_wrapper *wrapper, const char *url, int flags, php_stream_statbuf *ssb, php_stream_context *context) {
#if PHP_VERSION_ID >= 80100
    if (!(flags & PHP_STREAM_URL_STAT_IGNORE_OPEN_BASEDIR)) {
#endif
        if (strncasecmp(url, "file://", sizeof("file://") - 1) == 0) {
            url += sizeof("file://") - 1;
        }

        if (php_check_open_basedir_ex(url, (flags & PHP_STREAM_URL_STAT_QUIET) ? 0 : 1)) {
            return -1;
        }
#if PHP_VERSION_ID >= 80100
    }
#endif

#ifdef PHP_WIN32
    if (flags & PHP_STREAM_URL_STAT_LINK) {
        return lstat(url, &ssb->sb);
    }
#else
#ifdef HAVE_SYMLINK
    if (flags & PHP_STREAM_URL_STAT_LINK) {
        return lstat(url, &ssb->sb);
    } else
#endif
#endif
    return stat(url, &ssb->sb);
}

static int php_plain_files_unlink(php_stream_wrapper *wrapper,
                                  const char *url,
                                  int options,
                                  php_stream_context *context) {
    int ret;

    if (strncasecmp(url, "file://", sizeof("file://") - 1) == 0) {
        url += sizeof("file://") - 1;
    }

    if (php_check_open_basedir(url)) {
        return 0;
    }

    ret = unlink(url);
    if (ret == -1) {
        if (options & REPORT_ERRORS) {
            php_error_docref1(NULL, url, E_WARNING, "%s", strerror(errno));
        }
        return 0;
    }

    /* Clear stat cache (and realpath cache) */
    php_clear_stat_cache(1, NULL, 0);

    return 1;
}

static int php_plain_files_rename(
    php_stream_wrapper *wrapper, const char *url_from, const char *url_to, int options, php_stream_context *context) {
    int ret;

    if (!url_from || !url_to) {
        return 0;
    }

#ifdef PHP_WIN32
    if (!php_win32_check_trailing_space(url_from, (int) strlen(url_from))) {
        php_win32_docref2_from_error(ERROR_INVALID_NAME, url_from, url_to);
        return 0;
    }
    if (!php_win32_check_trailing_space(url_to, (int) strlen(url_to))) {
        php_win32_docref2_from_error(ERROR_INVALID_NAME, url_from, url_to);
        return 0;
    }
#endif

    if (strncasecmp(url_from, "file://", sizeof("file://") - 1) == 0) {
        url_from += sizeof("file://") - 1;
    }

    if (strncasecmp(url_to, "file://", sizeof("file://") - 1) == 0) {
        url_to += sizeof("file://") - 1;
    }

    if (php_check_open_basedir(url_from) || php_check_open_basedir(url_to)) {
        return 0;
    }

    ret = rename(url_from, url_to);

    if (ret == -1) {
#ifndef PHP_WIN32
#ifdef EXDEV
        if (errno == EXDEV) {
            zend_stat_t sb;
#if !defined(ZTS) && !defined(TSRM_WIN32)
            /* not sure what to do in ZTS case, umask is not thread-safe */
            int oldmask = umask(077);
#endif
            int success = 0;
            if (php_copy_file(url_from, url_to) == SUCCESS) {
                if (stat(url_from, &sb) == 0) {
                    success = 1;
#ifndef TSRM_WIN32
                    /*
                     * Try to set user and permission info on the target.
                     * If we're not root, then some of these may fail.
                     * We try chown first, to set proper group info, relying
                     * on the system environment to have proper umask to not allow
                     * access to the file in the meantime.
                     */
                    if (chown(url_to, sb.st_uid, sb.st_gid)) {
                        php_error_docref2(NULL, url_from, url_to, E_WARNING, "%s", strerror(errno));
                        if (errno != EPERM) {
                            success = 0;
                        }
                    }

                    if (success) {
                        if (chmod(url_to, sb.st_mode)) {
                            php_error_docref2(NULL, url_from, url_to, E_WARNING, "%s", strerror(errno));
                            if (errno != EPERM) {
                                success = 0;
                            }
                        }
                    }
#endif
                    if (success) {
                        unlink(url_from);
                    }
                } else {
                    php_error_docref2(NULL, url_from, url_to, E_WARNING, "%s", strerror(errno));
                }
            } else {
                php_error_docref2(NULL, url_from, url_to, E_WARNING, "%s", strerror(errno));
            }
#if !defined(ZTS) && !defined(TSRM_WIN32)
            umask(oldmask);
#endif
            return success;
        }
#endif
#endif

#ifdef PHP_WIN32
        php_win32_docref2_from_error(GetLastError(), url_from, url_to);
#else
        php_error_docref2(NULL, url_from, url_to, E_WARNING, "%s", strerror(errno));
#endif
        return 0;
    }

    /* Clear stat cache (and realpath cache) */
    php_clear_stat_cache(1, NULL, 0);

    return 1;
}

static int php_plain_files_mkdir(
    php_stream_wrapper *wrapper, const char *dir, int mode, int options, php_stream_context *context) {
    if (strncasecmp(dir, "file://", sizeof("file://") - 1) == 0) {
        dir += sizeof("file://") - 1;
    }

    if (!(options & PHP_STREAM_MKDIR_RECURSIVE)) {
        return sw_php_mkdir(dir, mode) == 0;
    }

    char buf[MAXPATHLEN];
    if (!expand_filepath_with_mode(dir, buf, NULL, 0, CWD_EXPAND)) {
        php_error_docref(NULL, E_WARNING, "Invalid path");
        return 0;
    }

    if (php_check_open_basedir(buf)) {
        return 0;
    }

    /* we look for directory separator from the end of string, thus hopefully reducing our work load */
    char *p;
    zend_stat_t sb;
    size_t dir_len = strlen(dir), offset = 0;
    char *e = buf + strlen(buf);

    if ((p = (char *) memchr(buf, DEFAULT_SLASH, dir_len))) {
        offset = p - buf + 1;
    }

    if (p && dir_len == 1) {
        /* buf == "DEFAULT_SLASH" */
    } else {
        /* find a top level directory we need to create */
        while ((p = strrchr(buf + offset, DEFAULT_SLASH)) || (offset != 1 && (p = strrchr(buf, DEFAULT_SLASH)))) {
            int n = 0;

            *p = '\0';
            while (p > buf && *(p - 1) == DEFAULT_SLASH) {
                ++n;
                --p;
                *p = '\0';
            }
            if (stat(buf, &sb) == 0) {
                while (1) {
                    *p = DEFAULT_SLASH;
                    if (!n) break;
                    --n;
                    ++p;
                }
                break;
            }
        }
    }

    if (!p) {
        p = buf;
    }
    while (true) {
        int ret = mkdir(buf, (mode_t) mode);
        if (ret < 0 && errno != EEXIST) {
            if (options & REPORT_ERRORS) {
                php_error_docref(NULL, E_WARNING, "%s", strerror(errno));
            }
            return 0;
        }

        bool replaced_slash = false;
        while (++p != e) {
            if (*p == '\0') {
                replaced_slash = true;
                *p = DEFAULT_SLASH;
                if (*(p + 1) != '\0') {
                    break;
                }
            }
        }
        if (p == e || !replaced_slash) {
            /* No more directories to create */
            /* issue a warning to client when the last directory was created failed */
            if (ret < 0) {
                if (options & REPORT_ERRORS) {
                    php_error_docref(NULL, E_WARNING, "%s", strerror(errno));
                }
                return 0;
            }
            return 1;
        }
    }
}

static int php_plain_files_rmdir(php_stream_wrapper *wrapper,
                                 const char *url,
                                 int options,
                                 php_stream_context *context) {
    if (strncasecmp(url, "file://", sizeof("file://") - 1) == 0) {
        url += sizeof("file://") - 1;
    }

    if (php_check_open_basedir(url)) {
        return 0;
    }

#ifdef PHP_WIN32
    if (!php_win32_check_trailing_space(url, (int) strlen(url))) {
        php_error_docref1(NULL, url, E_WARNING, "%s", strerror(ENOENT));
        return 0;
    }
#endif

    if (rmdir(url) < 0) {
        php_error_docref1(NULL, url, E_WARNING, "%s", strerror(errno));
        return 0;
    }

    /* Clear stat cache (and realpath cache) */
    php_clear_stat_cache(1, NULL, 0);

    return 1;
}

static int php_plain_files_metadata(
    php_stream_wrapper *wrapper, const char *url, int option, void *value, php_stream_context *context) {
    struct utimbuf *newtime;
#ifndef PHP_WIN32
    uid_t uid;
    gid_t gid;
#endif
    mode_t mode;
    int ret = 0;

#ifdef PHP_WIN32
    if (!php_win32_check_trailing_space(url, strlen(url))) {
        php_error_docref1(NULL, url, E_WARNING, "%s", strerror(ENOENT));
        return 0;
    }
#endif

    if (strncasecmp(url, "file://", sizeof("file://") - 1) == 0) {
        url += sizeof("file://") - 1;
    }

    if (php_check_open_basedir(url)) {
        return 0;
    }

    switch (option) {
    case PHP_STREAM_META_TOUCH:
        newtime = (struct utimbuf *) value;
        if (access(url, F_OK) != 0) {
            int file = open(url, O_CREAT | O_WRONLY | O_TRUNC, 0666);
            if (file == -1) {
                php_error_docref1(NULL, url, E_WARNING, "Unable to create file %s because %s", url, strerror(errno));
                return 0;
            }
            close(file);
        }

        ret = utime(url, newtime);
        break;
#ifndef PHP_WIN32
    case PHP_STREAM_META_OWNER_NAME:
    case PHP_STREAM_META_OWNER:
        if (option == PHP_STREAM_META_OWNER_NAME) {
            if (php_get_uid_by_name((char *) value, &uid) != SUCCESS) {
                php_error_docref1(NULL, url, E_WARNING, "Unable to find uid for %s", (char *) value);
                return 0;
            }
        } else {
            uid = (uid_t) * (long *) value;
        }
        ret = chown(url, uid, -1);
        break;
    case PHP_STREAM_META_GROUP:
    case PHP_STREAM_META_GROUP_NAME:
        if (option == PHP_STREAM_META_GROUP_NAME) {
            if (php_get_gid_by_name((char *) value, &gid) != SUCCESS) {
                php_error_docref1(NULL, url, E_WARNING, "Unable to find gid for %s", (char *) value);
                return 0;
            }
        } else {
            gid = (gid_t) * (long *) value;
        }
        ret = chown(url, -1, gid);
        break;
#endif
    case PHP_STREAM_META_ACCESS:
        mode = (mode_t) * (zend_long *) value;
        ret = chmod(url, mode);
        break;
    default:
        zend_value_error("Unknown option %d for stream_metadata", option);
        return 0;
    }
    if (ret == -1) {
        php_error_docref1(NULL, url, E_WARNING, "Operation failed: %s", strerror(errno));
        return 0;
    }
    php_clear_stat_cache(0, NULL, 0);
    return 1;
}

static php_stream_wrapper_ops sw_php_plain_files_wrapper_ops = {
    php_plain_files_stream_opener,
    NULL,
    NULL,
    php_plain_files_url_stater,
    php_plain_files_dir_opener,
    "plainfile/coroutine",
    php_plain_files_unlink,
    php_plain_files_rename,
    php_plain_files_mkdir,
    php_plain_files_rmdir,
    php_plain_files_metadata
};

PHPAPI php_stream_wrapper sw_php_plain_files_wrapper = {
    &sw_php_plain_files_wrapper_ops,
    NULL,
    0
};

static void php_stream_mode_sanitize_fdopen_fopencookie(php_stream *stream, char *result) {
    /* replace modes not supported by fdopen and fopencookie, but supported
     * by PHP's fread(), so that their calls won't fail */
    const char *cur_mode = stream->mode;
    int has_plus = 0, has_bin = 0, i, res_curs = 0;

    if (cur_mode[0] == 'r' || cur_mode[0] == 'w' || cur_mode[0] == 'a') {
        result[res_curs++] = cur_mode[0];
    } else {
        /* assume cur_mode[0] is 'c' or 'x'; substitute by 'w', which should not
         * truncate anything in fdopen/fopencookie */
        result[res_curs++] = 'w';

        /* x is allowed (at least by glibc & compat), but not as the 1st mode
         * as in PHP and in any case is (at best) ignored by fdopen and fopencookie */
    }

    /* assume current mode has at most length 4 (e.g. wbn+) */
    for (i = 1; i < 4 && cur_mode[i] != '\0'; i++) {
        if (cur_mode[i] == 'b') {
            has_bin = 1;
        } else if (cur_mode[i] == '+') {
            has_plus = 1;
        }
        /* ignore 'n', 't' or other stuff */
    }

    if (has_bin) {
        result[res_curs++] = 'b';
    }
    if (has_plus) {
        result[res_curs++] = '+';
    }
    result[res_curs] = '\0';
}

static int sw_php_mkdir(const char *dir, zend_long mode) {
    int ret;

    if (php_check_open_basedir(dir)) {
        return -1;
    }

    if ((ret = mkdir(dir, (mode_t) mode)) < 0) {
        php_error_docref(NULL, E_WARNING, "%s", strerror(errno));
    }

    return ret;
}
