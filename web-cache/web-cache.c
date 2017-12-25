#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <WS2tcpip.h>
#include <WinSock2.h>
#include <minwinbase.h>
#include <sys/stat.h> /* stat() */
#include <time.h> /*  */
#include <direct.h> /* _mkdir() */
#include <Shlwapi.h> /* PathFileExistsA() */
#include <Windows.h>
#include "web-cache.h"
// #pragma comment (lib, "Ws2_32.lib") /* add when main add */
#pragma comment (lib, "Shlwapi.lib")

char *workspace = DEFAULT_CACHE_DIC;
int port = DEFAULT_WEB_CACHE_PORT;

static int web_cache_exit_flag = 0;
static int timeout = 1000;

/**
 * djb2 string hash algorithm
 * copied from: http://www.cse.yorku.ca/~oz/hash.html
 */
unsigned long hash(unsigned char *str)
{
    unsigned long hash = 5381;
    int c;

    while (c = *str++)
        hash = ((hash << 5) + hash) + c; /* hash * 33 + c */

    return hash;
}

#define FILE_BUF_LEN 1024
int parse_config_file(const char* file_name) {
    char buf[FILE_BUF_LEN];
    FILE *file = NULL;
    int line_len = 0;
    fopen_s(&file, file_name, "r");
    if (file == NULL) return FILE_NOT_EXIST;
    
    /* ignore comments and blank lines */
    while (fgets(buf, FILE_BUF_LEN, file) != NULL)
        if (buf[0] != '\n' && buf[0] != '#') break;
    /* first config line: workspace */
    line_len = strlen(buf); // ignore '\n'
    workspace = (char*)malloc(line_len);
    memcpy(workspace, buf, line_len);
    workspace[line_len - 1] = 0;

    /* ignore comments and blank lines */
    while (fgets(buf, FILE_BUF_LEN, file) != NULL)
        if (buf[0] != '\n' && buf[0] != '#') break;
    /* second config line: port */
    sscanf_s(buf, "%d", &port);

    fclose(file);
    return SUCCESS;
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

        /* modify status code from 304 to 200 */
        if (context->status_code == 304) {
            memcpy((char*)msg, "HTTP/1.1 200 OK          \r\n", 27);
        }

        free(str);
    }
    else {
        /* check method_type (currently support GET method only )*/
        if (msg[0] == 'G' && msg[1] == 'E' && msg[2] == 'T')
            context->method_type = GET;
        else
            context->method_type = NOT_GET;

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
 *   NOTICE: When header not found, value will be NULL.
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
int parse_header(const char* msg, const char* name, char** value, char** header_loc, char** header_end) {
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
            *header_loc = msg;
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

    /* set header end */
    *header_end = msg;
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
    return SUCCESS;
}

/**
 * This function will parse the host and port from the "host" string.
 * If the "host" string is correctly parsed, this function will 
 * allocate memory for "context"'s host string.
 */
int parse_host(const char* host, http_context* context) {
    if (host == NULL) {
        printf("Fatal error: Host is null when parsing %s\n", context->url);
        exit(ILLEGAL_HEADERS);
    }
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
    /* local_path = host + '/' + hash(url) */
    int str_len = strlen(context->host) + strlen(context->url) + 2;
    char *str = (char*)malloc(str_len);
    sprintf_s(str, str_len, "%s\\%lu", context->host, hash(context->url));
    context->local_path = str;
    return SUCCESS;
}

/**
 * This function will change the request message. If there is a 
 * "If-Modified-Since" header in the message, It will modify the value
 * of that header. If not, it will add a header at the end.
 * 
 * NOTICE: The caller MUST insure that the request message buffer has
 * more than 54 bytes empty space, 19bytes for "If-Modified-Since: ",
 * 33 bytes for the longest http time string, 2 bytes for CRLF.
 * 
 * Conver to GMT time format modified from :
 * "https://stackoverflow.com/questions/2726975/
 * how-can-i-generate-an-rfc1123-date-string-from-c-code-win32"
 * ==================================================================
 * Parameters:
 * ret: 
 *   The return code of parse_header(), it can be SUCCESS or 
 *   HEADER_NOT_FOUND.
 * header_loc:
 *   The location of the "If-Modified-Since" header, it can be NULL.
 * header_end:
 *   The location of the header end, it MUST NOT be NULL.
 */
int parse_if_modified_since(int ret, char** header_loc, char** header_end, http_context *context) {
    char *msg = NULL;
    if (ret == HEADER_NOT_FOUND) 
        msg = *header_end - 2; /* return before CRLF */
    else 
        msg = *header_loc;
    if (msg == NULL) {
        printf("Fatal error: If-Modified-Since is null when parsing %s\n", context->url);
        exit(ILLEGAL_HEADERS);
    }

    if (PathFileExistsA(context->local_path)) {

        /* get file last modified time*/
        struct stat file_attr;
        time_t last_mtime;
        struct tm last_mtm;
        
        char new_header_str[64];
        int count = 0;

        stat(context->local_path, &file_attr);
        last_mtime = file_attr.st_mtime;
        gmtime_s(&last_mtm, &last_mtime);

#ifdef ASCTIME
        char asctime_str[26];
        asctime_s(asctime_str, 26, &last_mtm); // this function will add '\n'
        sprintf_s(new_header_str, 64, "If-Modified-Since: %s", asctime_str);
        for (count = 0; new_header_str[count] != '\n'; count++);

#endif
#ifdef GMTTIME
        const int RFC1123_TIME_LEN = 29;
        const char *DAY_NAMES[] = 
        { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };
        const char *MONTH_NAMES[] =
        { "Jan", "Feb", "Mar", "Apr", "May", "Jun",
            "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };
        char gmttime_str[30];
        strftime(gmttime_str, RFC1123_TIME_LEN + 1, "---, %d --- %Y %H:%M:%S GMT", &last_mtm);
        memcpy(gmttime_str, DAY_NAMES[last_mtm.tm_wday], 3);
        memcpy(gmttime_str + 8, MONTH_NAMES[last_mtm.tm_mon], 3);
        sprintf_s(new_header_str, 64, "If-Modified-Since: %s", gmttime_str);
        for (count = 0; new_header_str[count] != 0; count++);
#endif
        /* copy new header */
        memcpy(msg, new_header_str, count);
        msg += count;
        if (ret == HEADER_NOT_FOUND) {
            msg[0] = '\r'; msg[1] = '\n'; msg[2] = '\r'; msg[3] = '\n';
            context->msg = *header_end = msg + 4; // update header_end
        }
        else {
            for (msg; *msg != '\r'; *msg++ = ' ');
        }
    }

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
    char *header_value = NULL, *header_end = msg, *header_loc = NULL;
    
    /* parse header "Host" */
    ret = parse_header(msg, "Host", &header_value, &header_loc, &header_end);
    THROW_PARSE_REQ_EXCEPTION(ret, header_value);
    ret = parse_host(header_value, context);
    THROW_PARSE_REQ_EXCEPTION(ret, header_value);
    free(header_value);
    get_ip_from_host(context); /* get ip */    
    get_local_path(context); /* update local path*/

    /* parse header "If-Modified-Since" */
    ret = parse_header(msg, "If-Modified-Since", &header_value, &header_loc, &header_end);
    if (ret != HEADER_NOT_FOUND) /* ignore HEADER_NOT_FOUND */
        THROW_PARSE_REQ_EXCEPTION(ret, header_value);
    ret = parse_if_modified_since(ret, &header_loc, &header_end, context);
    THROW_PARSE_REQ_EXCEPTION(ret, header_value);
    free(header_value);

    /* update context.msg */
    context->msg = header_end;

    return ret;
}

int parse_response(const char* msg, http_context* context) {
    int ret;
    char *header_value = NULL, *header_end = msg, *header_loc = NULL;

    /* parse an never exist header to get header_end */
    ret = parse_header(msg, "Host", &header_value, &header_loc, &header_end);
    if (ret != HEADER_NOT_FOUND) THROW_PARSE_REQ_EXCEPTION(ret, header_value);

    /* update context.msg */
    context->msg = header_end;
    return SUCCESS;
}

int parse(const char* msg, http_context* context) {
    if (msg == NULL) {
        printf("Fatal error: message is null\n");
        exit(-1);
    }
    if (context == NULL) {
        printf("Fatal error: context is null\n");
        exit(-1);
    }
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

#define BROWSER_BUF_LEN 4097
#define SERVER_BUF_LEN 16349
DWORD WINAPI simple_cache_thread(LPVOID lpParam) {
    int ret;

    /* get server socket */
    SOCKET cache_socket = *(SOCKET*)lpParam;

    /* initialize browser socket */
    SOCKET browser_socket;
    struct sockaddr_in browser_addr;
    int browser_addr_len;
    char browser_buf[BROWSER_BUF_LEN];

    /* initialize web server socket */
    SOCKET server_socket;
    struct sockaddr_in server_addr;
    char server_buf[SERVER_BUF_LEN];

    http_context context;
    int req_len, rep_len, write_len;
    int contain_rep_header;
    FILE* web_page_file = NULL;
    web_cache_exit_flag = 0;
    while (web_cache_exit_flag != 1) {
        INIT_CONTEXT(context);

        /* accept connection */
        browser_addr_len = sizeof(browser_addr);
        browser_socket = accept(cache_socket, (struct sockaddr*)&browser_addr, &browser_addr_len);
#ifdef DEBUG
        if (browser_socket == INVALID_SOCKET)  printf("accept error: %d\n", WSAGetLastError());
#endif

        /* receive request */
        memset(browser_buf, 0, BROWSER_BUF_LEN);
        req_len = recv(browser_socket, browser_buf, BROWSER_BUF_LEN - 1, 0);
#ifdef DEBUG
        if (req_len < 0) printf("receive from browser error: %d\n", WSAGetLastError());
#endif
        if (req_len == 0) continue;

        /* parse request message */
        parse(browser_buf, &context);
        req_len = strlen(browser_buf);

        /* initialize web server socket */
        server_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(context.port);
        ret = inet_pton(AF_INET, context.ip_addr, &server_addr.sin_addr);
#ifdef DEBUG
        if (ret != 1) printf("IP convert to string error: %d\n", WSAGetLastError());
#endif
        setsockopt(server_socket, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(int));

        /* conditional create dirctory for host */
        ret = _mkdir(context.host);

        /* send request */
        connect(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr));
        ret = send(server_socket, browser_buf, req_len, 0);

        contain_rep_header = 1; // first message always contains response header
        while(1) {
            rep_len = recv(server_socket, server_buf, SERVER_BUF_LEN-1, 0);
#ifdef DEBUG
            if (rep_len < 0) printf("URL: %s Receive from server error: %d\n", context.url, WSAGetLastError());
#endif
            if (rep_len <= 0) break;
            server_buf[rep_len] = 0;
#ifdef DEBUG
            printf("Status: 200, URL: %s, Bytes: %d\n", context.url, rep_len);
#endif
            if (context.method_type == GET) {
                /* GET method messages */
                if (contain_rep_header) {
                    parse(server_buf, &context); /* parse response header */
                    contain_rep_header = 0; /* following messages will not contain */
                    if (context.status_code == 200) {
                        fopen_s(&web_page_file, context.local_path, "wb");
                        while (web_page_file == NULL) {
                            Sleep(500);
                            fopen_s(&web_page_file, context.local_path, "wb");
                        }
                        write_len = rep_len - (context.msg - server_buf);
                        fwrite(context.msg, write_len, 1, web_page_file);
                        fclose(web_page_file);
                    }
                    if (context.status_code == 304) break;
                }
                else {
                    /* check status code */
                    if (context.status_code == 200) {
                        fopen_s(&web_page_file, context.local_path, "ab");
                        while (web_page_file == NULL) {
                            Sleep(500);
                            fopen_s(&web_page_file, context.local_path, "ab");
                        }
                        write_len = rep_len;
                        fwrite(server_buf, write_len, 1, web_page_file);
                        fclose(web_page_file);
                    }
                }
                send(browser_socket, server_buf, rep_len, 0);
            }
            else {
                /* NOT GET method messages */
                while (1) {
                    rep_len = recv(server_socket, server_buf, 16348, 0);
#ifdef DEBUG
                    if (rep_len < 0) printf("URL: %s Receive from server error: %d\n", context.url, WSAGetLastError());
#endif
                    if (rep_len <= 0) break;
                    server_buf[rep_len] = 0;
                    send(browser_socket, server_buf, rep_len, 0);
#ifdef DEBUG
                    printf("Status: 200, URL: %s, Bytes: %d\n", context.url, rep_len);
#endif
                }
            }
        }

        if (context.status_code == 304) {
#ifdef DEBUG
            printf("Status: 304, URL: %s\n", context.url);
#endif
            fopen_s(&web_page_file, context.local_path, "rb");
            while (web_page_file == NULL) {
                Sleep(500);
                fopen_s(&web_page_file, context.local_path, "rb");
            }
            while (1) {
                rep_len = fread(server_buf, 1, 16348, web_page_file);
                if (rep_len <= 0) break;
                send(browser_socket, server_buf, rep_len, 0);
#ifdef DEBUG
                printf("Status: 304, URL: %s, Bytes: %d\n", context.url, rep_len);
#endif
            }
            fclose(web_page_file);
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

    /* infos */
    printf("Web-Cache 1.0\n");
    printf("=============\n");
    printf("Input \"INFO\" to show web-cache configuration info\n");
    printf("Input \"EXIT\" to exit\n");
    printf("=============\n");
    printf("Web-Cache Initializing\n");

    /* conditional create directory - Cache */
    ret = _mkdir(workspace);
    ret = _chdir(workspace); // switch to cache directory
    if (ret == -1) {
        printf("Fatal error: Cannot create or switch to workspace \n");
        exit(FILE_NOT_EXIST);
    }

    /* initialize web cache socket */
    SOCKET cache_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    struct sockaddr_in cache_addr;
    cache_addr.sin_family = AF_INET;
    cache_addr.sin_addr.S_un.S_addr = INADDR_ANY;
    cache_addr.sin_port = htons(port);

    /* bind and listen */
    ret = bind(cache_socket, (struct sockaddr*)&cache_addr, sizeof(cache_addr));
    if (ret != 0) printf("bind error: %d\n", WSAGetLastError());
    ret = listen(cache_socket, DEFAULT_BACKLOG);
    if (ret != 0) printf("listen error: %d\n", WSAGetLastError());

    /* infos */
    printf("Web-Cache Running\n");

    /* muti-thread */
    HANDLE threads[2];
    for (int i = 0; i < 2; i++)
        threads[i] = CreateThread(NULL, 0, simple_cache_thread, &cache_socket, 0, NULL);

    /* wait for command */
    char command[64];
    while (gets_s(command, 64) != NULL) {
        if (strncmp(command, "EXIT", 4) == 0) {
            web_cache_exit_flag = 1;
            printf("Web-Cache existing, please wait...\n");
            break;
        }
        else if (strncmp(command, "INFO", 4) == 0) {
            printf("workspace: %s\n", workspace);
            printf("port: %d\n", port);
        }
        else {
            printf("Unknown command\n");
        }
    }

    WaitForMultipleObjects(1, threads, TRUE, timeout);
    closesocket(cache_socket);
    printf("Web-Cache exist, goodbye\n");
}
