#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "web-cache.h"

/**
 * check request-line: info not null
 * check response-line: info is null
 */
int check_start_line(char* msg, cache_info* info) {
    int i, space_count = 0;
    int space[3] = {0, 0, 0};
    int ret = SUCCESS;

    /* walk through start line */
    for(i = 0; msg[i] != '\r'; i++) {
        if (msg[i] == 0) ret = START_LINE_ILLEGAL;
        if (msg[i] == ' ') space[space_count++] = i;
        if (space_count == 3) ret = START_LINE_ILLEGAL;
    }
    if (space_count != 2) ret = START_LINE_ILLEGAL;
    if (msg[i + 1] != '\n') ret = START_LINE_ILLEGAL;

    /* set domain info */
    if (info != NULL) {
        if (ret == SUCCESS) {
            int domain_size = space[1] - space[0];
            info->domain = (char*)malloc(domain_size);
            memcpy(info->domain, msg + space[0] + 1, domain_size);
            info->domain[domain_size - 1] = 0;
        }
        else {
            info->domain = NULL;
        }
    }

    return ret;
}

int get_ip_from_domain(cache_info* info) {
    
    return SUCCESS;
}
