#ifndef WEB_CACHE_H__
#define WEB_CACHE_H__

#define DEBUG
#define DEFAULT_HTTP_PORT 80
#define DEFAULT_WEB_CACHE_PORT 63014
#define DEFAULT_BACKLOG 64
#define DEFAULT_QUEUE_LEN 16
#define CACHE_DIC "Cache"
#define CACHE_POSTFIX ".cache"

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

#define IP_STR_MAXSIZE 16 /* xxx.xxx.xxx.xxx */
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

    SOCKET browser_socket;
    struct sockaddr_in browser_addr;
    int browser_addr_len;
    char browser_buf[1025]; // TODO: optimize, not a fixed size
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

typedef struct {
    void **items;
    int size;
    int head;
    int tail;
    HANDLE mutex;
}queue;

#define init_queue(q)\
    do{\
        q.items = NULL;\
        q.head = q.tail = q.size = 0;\
        q.mutex = CreateMutex(NULL, FALSE, NULL);\
    }while(0)

#define free_queue(q)\
    do{\
        if (q.items) free(q.items);\
    } while(0)

unsigned long hash(unsigned char*);
int get_queue_size(queue*);
int set_queue_size(queue*, int);
int en_queue(queue*, void*);
void* de_queue(queue*);

int parse(const char*, http_context*);
int parse_start_line(const char*, http_context*);
int parse_header(const char*, const char*, char**, char**, char**);
int parse_host(const char*, http_context*);
int get_local_path(http_context*);
int get_ip_from_host(http_context*);
int parse_if_modified_since(int, char**, char**, http_context*);

void simple_cache();

#endif
