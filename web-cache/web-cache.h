#ifndef WEB_CACHE_H__
#define WEB_CACHE_H__

#define DEBUG

/* status */
enum {
    SUCCESS,
    ILLEGAL_START_LINE,
    GET_IP_FAIL
};

#define IP_STR_MAXSIZE 16 /* xxx.xxx.xxx.xxx */
typedef struct {
    char* domain;
    union {
        char ip_addr[IP_STR_MAXSIZE];
        int status_code;
    };
    char* local_path;
}cache_info;

int parse_start_line(char*, cache_info*);
int get_ip_from_domain(cache_info*);

#endif
