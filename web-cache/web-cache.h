#ifndef WEB_CACHE_H__
#define WEB_CACHE_H__

#define DEBUG
#define GMTTIME
#define DEFAULT_HTTP_PORT 80
#define DEFAULT_WEB_CACHE_PORT 63014
#define DEFAULT_BACKLOG 64
#define DEFAULT_QUEUE_LEN 16
#define CACHE_DIC "Cache"
#define IP_STR_MAXSIZE 16 /* xxx.xxx.xxx.xxx */

typedef enum {
    UNKNOWN,
    REQUEST,
    RESPONSE,
    GET,
    NOT_GET
} msg_type;

/* status */
enum {
    SUCCESS,
    FILE_NOT_EXIST,
    ILLEGAL_START_LINE,
    ILLEGAL_HEADERS,
    HEADER_NOT_FOUND,
    UNKNOWN_MESSAGE_TYPE,
    GET_IP_FAIL,
    OUT_OF_MEMORY,
    QUEUE_FULL,
    QUEUE_EMPTY,
    LISTEN_ERROR,
    RECV_EMPTY
};

typedef struct {
    msg_type method_type;
    msg_type msg_type;
    char ip_addr[IP_STR_MAXSIZE];
    int port;
    int status_code;
    const char* msg;

    char* url;
    char* host;
    char* local_path;

}http_context;

#define INIT_CONTEXT(context)\
    do{\
        context.method_type = UNKNOWN;\
        context.msg_type = UNKNOWN;\
        context.msg = NULL;\
        context.url = NULL;\
        context.host = NULL;\
        context.port = -1;\
        context.local_path = NULL;\
        memset(context.ip_addr, 0, IP_STR_MAXSIZE);\
    } while(0)
#define FREE_CONTEXT(context)\
    do{\
        context.msg = NULL;\
        if(context.url) { free(context.url); context.url = NULL; }\
        if(context.host) { free(context.host); context.host = NULL; }\
        if(context.local_path) { free(context.local_path); context.local_path = NULL; }\
    }while(0)

/* variables for configuration */
extern char *workspace;
extern int port;

unsigned long hash(unsigned char*);

int parse_config_file(const char*);

int parse(const char*, http_context*);
int parse_start_line(const char*, http_context*);
int parse_header(const char*, const char*, char**, char**, char**);
int parse_host(const char*, http_context*);
int get_local_path(http_context*);
int get_ip_from_host(http_context*);
int parse_if_modified_since(int, char**, char**, http_context*);

void simple_cache();

#endif
