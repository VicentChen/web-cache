#ifndef WEB_CACHE_H__
#define WEB_CACHE_H__

/* status */
enum {
    SUCCESS,
    START_LINE_ILLEGAL
};

typedef struct {
    char* domain;
    void* ip_addr;
    char* local_path;
}cache_info;

int check_start_line(char*, cache_info*);
int get_ip_from_domain(cache_info*);

#endif
