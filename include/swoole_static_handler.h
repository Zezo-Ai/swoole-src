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

#pragma once

#include "swoole_server.h"
#include "swoole_http.h"
#include "swoole_mime_type.h"

#include <string>
#include <set>

namespace swoole {
namespace http_server {
class StaticHandler {
  private:
    Server *serv;
    std::string request_url;
    std::string dir_path;
    std::set<std::string> dir_files;
    std::string index_file;
    typedef struct {
        off_t offset;
        size_t length;
        char part_header[SW_HTTP_SERVER_PART_HEADER];
    } task_t;
    std::vector<task_t> tasks;

    size_t l_filename = 0;
    char filename[PATH_MAX];
    struct stat file_stat;
    bool last = false;
    std::string content_type;
    std::string boundary;
    std::string end_part;
    size_t content_length = 0;

  public:
    int status_code = SW_HTTP_OK;
    StaticHandler(Server *_server, const char *url, size_t url_length) : request_url(url, url_length) {
        serv = _server;
    }

    /**
     * @return true: continue to execute backwards
     * @return false: break static handler
     */
    bool hit();
    bool hit_index_file();

    bool is_modified(const std::string &date_if_modified_since);
    bool is_modified_range(const std::string &date_range);
    size_t make_index_page(String *buffer);
    bool get_dir_files();
    bool set_filename(const std::string &filename);

    bool catch_error() {
        if (last) {
            status_code = SW_HTTP_NOT_FOUND;
            return true;
        } else {
            return false;
        }
    }

    bool has_index_file() {
        return !index_file.empty();
    }

    bool is_enabled_auto_index() {
        return serv->http_autoindex;
    }

    static std::string get_date();

    time_t get_file_mtime() {
#ifdef __MACH__
        return file_stat.st_mtimespec.tv_sec;
#else
        return file_stat.st_mtim.tv_sec;
#endif
    }

    std::string get_date_last_modified();

    const char *get_filename() {
        return filename;
    }

    const std::string &get_boundary() {
        if (boundary.empty()) {
            boundary = std::string(SW_HTTP_SERVER_BOUNDARY_PREKEY);
            swoole_random_string(boundary, SW_HTTP_SERVER_BOUNDARY_TOTAL_SIZE - sizeof(SW_HTTP_SERVER_BOUNDARY_PREKEY));
        }
        return boundary;
    }

    const std::string &get_content_type() {
        if (tasks.size() > 1) {
            content_type = std::string("multipart/byteranges; boundary=") + get_boundary();
            return content_type;
        } else {
            return get_mimetype();
        }
    }

    const std::string &get_mimetype() {
        return swoole::mime_type::get(get_filename());
    }

    std::string get_filename_std_string() {
        return std::string(filename, l_filename);
    }

    bool get_absolute_path();

    size_t get_filesize() {
        return file_stat.st_size;
    }

    const std::vector<task_t> &get_tasks() {
        return tasks;
    }

    bool is_dir() {
        return S_ISDIR(file_stat.st_mode);
    }

    bool is_link() {
        return S_ISLNK(file_stat.st_mode);
    }

    bool is_file() {
        return S_ISREG(file_stat.st_mode);
    }

    bool is_absolute_path() {
        return swoole_strnpos(filename, l_filename, SW_STRL("..")) == -1;
    }

    bool is_located_in_document_root() {
        const std::string &document_root = serv->get_document_root();
        const size_t l_document_root = document_root.length();

        return l_filename > l_document_root && filename[l_document_root] == '/' &&
               swoole_str_starts_with(filename, l_filename, document_root.c_str(), l_document_root);
    }

    size_t get_content_length() {
        return content_length;
    }

    const std::string &get_end_part() {
        return end_part;
    }

    void parse_range(const char *range, const char *if_range);
};

};  // namespace http_server
};  // namespace swoole
