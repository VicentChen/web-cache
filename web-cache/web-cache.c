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
int parse_start_line(char* msg, cache_info* info) {
    assert(info != NULL);
    
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

    /* check request or response */
    if (msg[0] == 'H' && msg[1] == 'T' && msg[2] == 'T' && msg[3] == 'P') {
        /* response: set status code */
        info->status_code = 0;
        if (ret == SUCCESS) {
            int status_code_size = space[1] - space[0];
            char* str = (char*)malloc(status_code_size);
            memcpy(str, msg + space[0] + 1, status_code_size);
            str[status_code_size - 1] = 0;
            info->status_code = atoi(str);
            free(str);
        }
    }
    else {
        /* request: set domain info */
        if (info->domain) {
#ifdef DEBUG
            printf("domain NOT NULL and it'll be freed\n");
#endif
            free(info->domain);
            info->domain = NULL;
        }

        if (ret == SUCCESS) {
            int domain_size = space[1] - space[0];
            info->domain = (char*)malloc(domain_size);
            memcpy(info->domain, msg + space[0] + 1, domain_size);
            info->domain[domain_size - 1] = 0;
        }
    }

    return ret;
}

int get_ip_from_domain(cache_info* info) {
    struct addrinfo hints;
    struct addrinfo* result = NULL;
    struct sockaddr_in* sockaddr;

    /* hints: IPv4, TCP */
    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    getaddrinfo(info->domain, NULL, &hints, &result);
    memset(info->ip_addr, 0, IP_STR_MAXSIZE);

    if (result == NULL) {
        strcpy_s(info->ip_addr, 8, "0.0.0.0");
        info->ip_addr[8] = 0;
        return GET_IP_FAIL;
    }

    sockaddr = (struct sockaddr_in*)result->ai_addr;    
    inet_ntop(sockaddr->sin_family, &(sockaddr->sin_addr), info->ip_addr, IP_STR_MAXSIZE);
    freeaddrinfo(result);
    return SUCCESS;
}
