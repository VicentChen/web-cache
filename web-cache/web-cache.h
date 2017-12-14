#ifndef WEB_CACHE_H__
#define WEB_CACHE_H__

#define DEBUG

typedef enum {
    REQUEST,
    RESPONSE
} msg_type;

/* status */
enum {
    SUCCESS,
    ILLEGAL_START_LINE,
    ILLEGAL_HEADERS,
    HEADER_NOT_FOUND,
    GET_IP_FAIL
};

#define IP_STR_MAXSIZE 16 /* xxx.xxx.xxx.xxx */
typedef struct {
    msg_type msg_type;
    char ip_addr[IP_STR_MAXSIZE];
    int status_code;
    const char* msg;

    char* url;
    char* domain;
    char* local_path;
}http_context;

int parse(const char*, http_context*);
int parse_start_line(const char*, http_context*);
int parse_header(const char*, const char*, char**, char**);
int get_ip_from_domain(http_context*);

#endif
