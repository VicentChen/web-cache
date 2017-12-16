#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <WS2tcpip.h>
#include <WinSock2.h>
#include <minwinbase.h>
#include "web-cache.h"
// #pragma comment (lib, "Ws2_32.lib") /* add when main add */

static http_context** req_queue = NULL;
static int queue_len = DEFAULT_QUEUE_LEN + 1;
static int head = 0, tail = 0;

void init_queue() {
    req_queue = (http_context**)malloc(sizeof(http_context*)*queue_len);
}

int en_queue(http_context *context) {
    if ((tail + 1) % queue_len == head)
        return QUEUE_FULL;
    req_queue[tail] = context;
    tail = (tail + 1) % queue_len;
    return SUCCESS;
}

http_context* de_queue() {
    if (head == tail) return NULL;
    int ret = head;
    head = (head + 1) % queue_len;
    return req_queue[ret];
}

void release_queue() {
    if (req_queue) free(req_queue);
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
