#ifndef WEB_CACHE_H__
#define WEB_CACHE_H__

#define DEBUG
#define DEFAULT_HTTP_PORT 80
#define WEB_CACHE_PORT 63014
#define DEFAULT_BACKLOG 64
#define DEFAULT_QUEUE_LEN 16

typedef enum {
    UNKNOWN,
    REQUEST,
    RESPONSE
} msg_type;

/* status */
enum {
    SUCCESS,
    ILLEGAL_START_LINE,
    ILLEGAL_HEADERS,
    HEADER_NOT_FOUND,
    UNKNOWN_MESSAGE_TYPE,
    GET_IP_FAIL,
    QUEUE_FULL
};

#define IP_STR_MAXSIZE 16 /* xxx.xxx.xxx.xxx */
typedef struct {
    msg_type msg_type;
    char ip_addr[IP_STR_MAXSIZE];
    int port;
    int status_code;
    const char* msg;

    char* url;
    char* host;
    char* local_path;
}http_context;

int parse(const char*, http_context*);
int parse_start_line(const char*, http_context*);
int parse_header(const char*, const char*, char**, char**);
int parse_host(const char*, http_context*);
int get_ip_from_host(http_context*);

#endif
