#ifndef WEB_CACHE_H__
#define WEB_CACHE_H__

/* status */
enum {
    MSG_CHECK_OK,
    MSG_NOT_HTTP,
    MSG_HTTP_ILLEGAL
};

int check_start_line(char* msg);

#endif
