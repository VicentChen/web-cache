#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <WS2tcpip.h>
#include <WinSock2.h>
#include <minwinbase.h>
#include "web-cache.h"
// #pragma comment (lib, "Ws2_32.lib") /* add when main add */


#define SUPPORT_SPACE 2
int parse_start_line(const char* msg, http_context* context) {
    assert(context != NULL);
    
    int i, space_count = 0;
    int space[SUPPORT_SPACE] = {0, 0};
    int ret = SUCCESS;
    int flag = 0; const int HTTP = 1;

    /* walk through start line */
    for(i = 0; msg[i] != '\r'; i++) {
        if (msg[i] == 0) ret = ILLEGAL_START_LINE;
        if (msg[i] == ' ' && space_count < SUPPORT_SPACE) space[space_count++] = i;
    }
    if (space_count < 2) ret = ILLEGAL_START_LINE;
    if (msg[i + 1] != '\n') ret = ILLEGAL_START_LINE;

    /* update "msg" in context */
    context->msg = msg + i + 2;

    /* check request or response */
    if (msg[0] == 'H' && msg[1] == 'T' && msg[2] == 'T' && msg[3] == 'P') {
        /* response: set status code */
        context->status_code = 0;
        if (ret == SUCCESS) {
            int status_code_size = space[1] - space[0];
            char* str = (char*)malloc(status_code_size);
            memcpy(str, msg + space[0] + 1, status_code_size);
            str[status_code_size - 1] = 0;
            context->status_code = atoi(str);
            free(str);
        }
    }
    else {
        /* request: set url */
        if (context->url) {
#ifdef DEBUG
            printf("url NOT NULL and it'll be freed\n");
#endif
            free(context->url);
            context->url = NULL;
        }

        if (ret == SUCCESS) {
            int domain_size = space[1] - space[0];
            context->url = (char*)malloc(domain_size);
            memcpy(context->url, msg + space[0] + 1, domain_size);
            context->url[domain_size - 1] = 0;
        }
    }

    return ret;
}

int parse_headers(const char* msg, http_context* context) {
    return SUCCESS;
}

int parse(const char* msg, http_context* context) {
    int ret = SUCCESS;
    context->msg = msg;
    ret = parse_start_line(context->msg, context);
    if (ret != SUCCESS) return ret;
    return ret;
}

int get_ip_from_domain(http_context* context) {
    struct addrinfo hints;
    struct addrinfo* result = NULL;
    struct sockaddr_in* sockaddr;

    /* hints: IPv4, TCP */
    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    getaddrinfo(context->domain, NULL, &hints, &result);
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
