#pragma once

#include <algorithm>
#include <cstring>
#include <sstream>
#include <cstdint>
#include <string>

#include <iostream>

using std::cerr;
using std::cout;
using std::endl;
using std::string;

class HTTPURL {
  private:
    string _protocol; // http vs https
    string _domain;   // mail.google.com
    uint16_t _port;   // 80,443
    string _path;     // /mail/
    string _query;    // [after ?] a=b&c=b

  public:
    string &protocol;
    string &domain;
    uint16_t &port;
    string &path;
    string &query;

    HTTPURL(const string &url) : protocol(_protocol), domain(_domain), port(_port), path(_path), query(_query) {
        string u = _trim(url);
        size_t offset = 0, slash_pos, hash_pos, colon_pos, qmark_pos;
        string urlpath, urldomain, urlport;
        uint16_t default_port;

        static const char *allowed[] = {"https://", "http://", "ftp://", NULL};
        for (int i = 0; allowed[i] != NULL && this->_protocol.length() == 0; i++) {
            const char *c = allowed[i];
            if (u.compare(0, strlen(c), c) == 0) {
                offset = strlen(c);
                this->_protocol = string(c, 0, offset - 3);
            }
        }
        default_port = this->_protocol == "https" ? 443 : 80;
        slash_pos = u.find_first_of('/', offset + 1);
        urlpath = slash_pos == string::npos ? "/" : u.substr(slash_pos);
        urldomain = string(u.begin() + offset, slash_pos != string::npos ? u.begin() + slash_pos : u.end());
        urlpath = (hash_pos = urlpath.find("#")) != string::npos ? urlpath.substr(0, hash_pos) : urlpath;
        urlport = (colon_pos = urldomain.find(":")) != string::npos ? urldomain.substr(colon_pos + 1) : "";
        urldomain = urldomain.substr(0, colon_pos != string::npos ? colon_pos : urldomain.length());
        this->_domain = _tolowerstr(urldomain);
        this->_query = (qmark_pos = urlpath.find("?")) != string::npos ? urlpath.substr(qmark_pos + 1) : "";
        this->_path = qmark_pos != string::npos ? urlpath.substr(0, qmark_pos) : urlpath;
        this->_port = urlport.length() == 0 ? default_port : _atoi(urlport);
    };

  private:
    static inline string _trim(const string &input) {
        string str = input;
        size_t endpos = str.find_last_not_of(" \t\n\r");
        if (string::npos != endpos) {
            str = str.substr(0, endpos + 1);
        }
        size_t startpos = str.find_first_not_of(" \t\n\r");
        if (string::npos != startpos) {
            str = str.substr(startpos);
        }
        return str;
    };
    static inline string _tolowerstr(const string &input) {
        string str = input;
        std::transform(str.begin(), str.end(), str.begin(), ::tolower);
        return str;
    };
    static inline int _atoi(const string &input) {
        int r;
        std::stringstream(input) >> r;
        return r;
    };
};