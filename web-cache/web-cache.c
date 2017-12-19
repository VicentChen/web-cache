#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <WS2tcpip.h>
#include <WinSock2.h>
#include <minwinbase.h>
#include <Windows.h>
#include "web-cache.h"
// #pragma comment (lib, "Ws2_32.lib") /* add when main add */

int web_cache_exit_flag = 0;

// not thread safe
int get_queue_size(queue *q) {
    assert(q != NULL);
    return q->size - 1;
}

// not thread safe
int set_queue_size(queue* q, int size) {
    q->size = size + 1;
    q->items = (void**)realloc(q->items, sizeof(void*) * q->size);
    if (q->items) return SUCCESS;
    else return OUT_OF_MEMORY;
}

int en_queue(queue* q, void* item_ptr) {
    assert(q != NULL);
    WaitForSingleObject(q->mutex, INFINITE);
    if ((q->tail + 1) % q->size == q->head) {
        ReleaseMutex(q->mutex);
        return QUEUE_FULL;
    }
    q->items[q->tail] = item_ptr;
    q->tail = (q->tail + 1) % q->size;
    ReleaseMutex(q->mutex);
    return SUCCESS;
}

void* de_queue(queue *q) {
    assert(q != NULL);
    void *result;
    if (q->head == q->tail) return NULL;
    int ret = q->head;
    q->head = (q->head + 1) % q->size;
    result = q->items[ret];
    return q->items[ret];
}

#define SUPPORT_SPACE 2
int parse_start_line(const char* msg, http_context* context) {
    
    int i, space_count = 0;
    int space[SUPPORT_SPACE] = {0, 0};
    int alloc_size = 0;

    /* walk through start line */
    context->msg_type = UNKNOWN;
    for(i = 0; msg[i] != '\r'; i++) {
        if (msg[i] == 0) return ILLEGAL_START_LINE;
        if (msg[i] == ' ' && space_count < SUPPORT_SPACE) space[space_count++] = i;
    }
    if (space_count < 2) return ILLEGAL_START_LINE;
    if (msg[i + 1] != '\n') return ILLEGAL_START_LINE;

    /* only legal message will execute codes below */
    /* update "msg" in context */
    context->msg = msg + i + 2;

    /* check request or response */
    if (msg[0] == 'H' && msg[1] == 'T' && msg[2] == 'T' && msg[3] == 'P') {
        /* response: set status code */
        context->msg_type = RESPONSE;

        context->status_code = 0;
        alloc_size = space[1] - space[0];
        char* str = (char*)malloc(alloc_size);
        memcpy(str, msg + space[0] + 1, alloc_size);
        str[alloc_size - 1] = 0;
        context->status_code = atoi(str);
        free(str);
    }
    else {
        /* request: set url */
        context->msg_type = REQUEST;

        if (context->url) {
#ifdef DEBUG
            printf("url NOT NULL and it'll be freed\n");
#endif
            free(context->url);
            context->url = NULL;
        }
        alloc_size = space[1] - space[0];
        context->url = (char*)malloc(alloc_size);
        memcpy(context->url, msg + space[0] + 1, alloc_size);
        context->url[alloc_size - 1] = 0;
    }

    return SUCCESS;
}

/**
 *==================================================================
 * Parameters:
 * 
 * value:
 *   It's caller's responsibility to make sure "value" is null, 
 *   because "value" will always be updated.  It's also caller's 
 *   responsibility to free memory allocated for "value".
 *   NOTICE: When headers are empty(or contains only blank - space
 *   and horizontal tab '\t'), "value" will be an empty string.
 * header_end:
 *   Updates when headers are syntactically correct.
 *==================================================================
 * Return Values:
 * 
 * ILLEGAL_HEADERS:
 *   Headers are syntatically incorrect, including "\r\n" miss "\n"
 *   or "\r".
 * HEADER_NOT_FOUND: 
 *   Headers are syntatically correct but no header's name matches.
 * SUCCESS.
 */
int parse_header(const char* msg, const char* name, char** value, char** header_end) {
    int name_len = strlen(name);
    const char *sub_start = NULL;
    const char *sub_end = NULL;
    int value_size;
    int header_not_found = 1;

    *value = NULL;

    /* search field "name" */
    while(*msg != '\r') {
        if (*msg == 0) return ILLEGAL_HEADERS;
        if (strncmp(msg, name, name_len) == 0) {
            /* locate specific header */
            for (msg; *msg != ':'; msg++); // skip header name
            msg++; // skip ':'
            for (msg; *msg == ' ' || *msg == '\t'; msg++); // skip optional space
            sub_start = msg;
            for (msg; *msg != '\r'; msg++); // find header value
            for (msg--; *msg == ' ' || *msg == '\t'; msg--); // skip optional space
            sub_end = msg + 1;
            header_not_found = 0;
        }
        /* skip line */
        for (msg; *msg != '\r'; msg++); 
        if (msg[1] == '\n') msg += 2;
        else return ILLEGAL_HEADERS;
    }
    if (msg[1] == '\n') msg += 2;
    else return ILLEGAL_HEADERS;
    if (header_not_found) return HEADER_NOT_FOUND;

    /* copy header value */
    if (sub_end <= sub_start) {
        *value = (char*)malloc(1);
        **value = 0;
    }
    else {
        value_size = sub_end - sub_start + 1;
        *value = (char*)malloc(value_size);
        memcpy(*value, sub_start, value_size);
        (*value)[value_size - 1] = 0;
    }

    /* set header end */
    *header_end = msg;

    return SUCCESS;
}

/**
 * This function will parse the host and port from
 * the "host" string. If the "host" string is correctly
 * parsed, this function will allocate memory for
 * "context"'s host string.
 */
int parse_host(const char* host, http_context* context) {
    assert(host != NULL);
    int host_len;
    const char* host_end = host;

    /* find host */
    for (host_end; *host_end != ':' && *host_end != 0; host_end++);
    host_len = host_end - host;
    if (host_len <= 0) return ILLEGAL_HEADERS;

    /* copy host */
    context->host = (char*)malloc(host_len + 1);
    memcpy(context->host, host, host_len);
    context->host[host_len] = 0;
    
    /* parse port */
    host = host_end;
    if (*host != 0)
        sscanf_s(++host, "%d", &context->port);
    else
        context->port = DEFAULT_HTTP_PORT;

    return SUCCESS;
}

int get_local_path(http_context* context) {
    /* local_path = host + '/' + url */
    int str_len = strlen(context->host) + strlen(context->url) + 2;
    char *str = (char*)malloc(str_len);
    sprintf_s(str, str_len, "%s %s", context->host, context->url);
    
    for (char* c = str; *c != 0; c++) {
        switch (*c) {
            case '\\': /* '\\' => ' ' */
            case '/': /* '/' => ' '*/
                *c = ' '; break;
            case ' ': /*' ' => '/' */
                *c = '/'; break;
            default: break;
        }
    }
    context->local_path = str;
    return SUCCESS;
}

#define THROW_PARSE_REQ_EXCEPTION(ret, header_value)\
    do{\
        if (ret != SUCCESS) {\
            if (header_value != NULL) free(header_value);\
            return ret;\
        }\
    }while(0)

int parse_request(const char* msg, http_context* context) {
    int ret;
    char *header_value = NULL, *header_end = msg;
    
    /* parse header "Host" */
    ret = parse_header(msg, "Host", &header_value, &header_end);
    THROW_PARSE_REQ_EXCEPTION(ret, header_value);
    ret = parse_host(header_value, context);
    THROW_PARSE_REQ_EXCEPTION(ret, header_value);
    free(header_value);

    /* get ip */
    get_ip_from_host(context);

    /* update local path*/
    get_local_path(context);

    /* update context.msg */
    context->msg = header_end;

    return ret;
}

int parse_response(const char* msg, http_context* context) {
    /* TODO: parse headers */
    return SUCCESS;
}

int parse(const char* msg, http_context* context) {
    assert(msg != NULL);
    assert(context != NULL);
    int ret = SUCCESS;
    context->msg = msg;
    ret = parse_start_line(context->msg, context);
    if (ret != SUCCESS) return ret;

    if (context->msg_type == REQUEST)
        ret = parse_request(context->msg, context);
    else if (context->msg_type == RESPONSE)
        ret = parse_response(context->msg, context);
    else
        ret = UNKNOWN_MESSAGE_TYPE;
    if (ret != SUCCESS) return ret;

    return ret;
}

int get_ip_from_host(http_context* context) {
    assert(context != NULL);

    struct addrinfo hints;
    struct addrinfo* result = NULL;
    struct sockaddr_in* sockaddr;

    /* hints: IPv4, TCP */
    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    getaddrinfo(context->host, NULL, &hints, &result);
    memset(context->ip_addr, 0, IP_STR_MAXSIZE);

    if (result == NULL) {
        strcpy_s(context->ip_addr, 8, "0.0.0.0");
        context->ip_addr[8] = 0;
        return GET_IP_FAIL;
    }

    sockaddr = (struct sockaddr_in*)result->ai_addr;    
    inet_ntop(sockaddr->sin_family, &(sockaddr->sin_addr), context->ip_addr, IP_STR_MAXSIZE);
    freeaddrinfo(result);
    return SUCCESS;
}

DWORD WINAPI simple_cache_thread(LPVOID lpParam) {
    int ret;
    int timeout = 1000;

    /* get server socket */
    SOCKET cache_socket = *(SOCKET*)lpParam;

    /* initialize browser socket */
    SOCKET browser_socket;
    struct sockaddr_in browser_addr;
    int browser_addr_len;
    char browser_buf[4097];

    /* initialize web server socket */
    SOCKET server_socket;
    struct sockaddr_in server_addr;
    char server_buf[16349];

    http_context context;
    int req_len, rep_len;
    web_cache_exit_flag = 0;
    while (web_cache_exit_flag != 1) {
        INIT_CONTEXT(context);

        /* accept connection */
        browser_addr_len = sizeof(browser_addr);
        browser_socket = accept(cache_socket, (struct sockaddr*)&browser_addr, &browser_addr_len);
        if (browser_socket == INVALID_SOCKET) printf("accept error: %d\n", WSAGetLastError());
        /* receive request */
        req_len = recv(browser_socket, browser_buf, 4096, 0);
        if (req_len < 0) printf("receive from browser error: %d\n", WSAGetLastError());
        if (req_len == 0) continue;
        browser_buf[req_len] = 0;

        /* parse request */
        parse(browser_buf, &context);

        /* initialize web server socket */
        server_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(context.port);
        ret = inet_pton(AF_INET, context.ip_addr, &server_addr.sin_addr);
        if (ret != 1) printf("IP convert to string error: %d\n", WSAGetLastError());
        setsockopt(server_socket, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(int));

        /* send request */
        connect(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr));
        ret = send(server_socket, browser_buf, req_len, 0);
        while(1) {
            rep_len = recv(server_socket, server_buf, 16348, 0);
            if (rep_len < 0) printf("receive from server error: %d\n", WSAGetLastError());
            if (rep_len <= 0) break;
            server_buf[rep_len] = 0;
            send(browser_socket, server_buf, rep_len, 0);
        }

        /* close socket */
        closesocket(server_socket);
        closesocket(browser_socket);

        FREE_CONTEXT(context);
    }
    return 0;
}

void simple_cache() {
    int ret;

    /* initialize web cache socket */
    SOCKET cache_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    struct sockaddr_in cache_addr;
    cache_addr.sin_family = AF_INET;
    cache_addr.sin_addr.S_un.S_addr = INADDR_ANY;
    cache_addr.sin_port = htons(DEFAULT_WEB_CACHE_PORT);

    /* bind and listen */
    ret = bind(cache_socket, (struct sockaddr*)&cache_addr, sizeof(cache_addr));
    if (ret != 0) printf("bind error: %d\n", WSAGetLastError());
    ret = listen(cache_socket, DEFAULT_BACKLOG);
    if (ret != 0) printf("listen error: %d\n", WSAGetLastError());

    /* muti-thread */
    HANDLE threads[32];
    for (int i = 0; i < 32; i++)
        threads[i] = CreateThread(NULL, 0, simple_cache_thread, &cache_socket, 0, NULL);

    WaitForMultipleObjects(4, threads, TRUE, INFINITE);
    closesocket(cache_socket);
}
