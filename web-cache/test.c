#ifdef _WINDOWS
#define _CRTDBG_MAP_ALLOC
#include <crtdbg.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <WS2tcpip.h>
#include <WinSock2.h>
#include "web-cache.h"
#pragma comment (lib, "Ws2_32.lib")

static int main_ret = 0;
static int test_count = 0;
static int test_pass = 0;

#define EXPECT_EQ_BASE(equality, expect, actual, format) \
    do {\
        test_count++;\
        if (equality)\
            test_pass++;\
        else {\
            fprintf(stderr, "Line %d: expect: " format " actual: " format "\n", __LINE__, expect, actual);\
            main_ret = 1;\
        }\
    } while(0)
#define EXPECT_EQ_INT(expect, actual) EXPECT_EQ_BASE((expect) == (actual), expect, actual, "%d")
/*alength: exclude '\0' */
#define EXPECT_EQ_STRING(expect, actual, alength) \
    EXPECT_EQ_BASE(strncmp(expect, actual, alength + 1) == 0, expect, actual, "%s")


#define INIT_CONTEXT(context)\
    do{\
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

#define TEST_PARSE(err, msg, context)\
    do {\
        FREE_CONTEXT(context);\
        EXPECT_EQ_INT(err, parse_start_line(msg, &context));\
    }while(0)

void test_parse_start_line() {
    char *msg;
    http_context context;
    INIT_CONTEXT(context);

    /* request part */
    msg = "GET http://www.baidu.com/ HTTP/1.1\r\n";
    TEST_PARSE(SUCCESS, msg, context);
    EXPECT_EQ_STRING("http://www.baidu.com/", context.url, 21);
    EXPECT_EQ_INT(REQUEST, context.msg_type);
    msg = "CONNECT www.baidu.com:443 HTTP/1.0\r\n";
    TEST_PARSE(SUCCESS, msg, context);
    EXPECT_EQ_STRING("www.baidu.com:443", context.url, 17);
    EXPECT_EQ_INT(REQUEST, context.msg_type);
    msg = "GEThttp://www.baidu.com/ \r\n";
    TEST_PARSE(ILLEGAL_START_LINE, msg, context);
    EXPECT_EQ_INT(UNKNOWN, context.msg_type);
    /* response part */
    msg = "HTTP/1.1 200 OK\r\n";
    TEST_PARSE(SUCCESS, msg, context);
    EXPECT_EQ_INT(200, context.status_code);
    EXPECT_EQ_INT(RESPONSE, context.msg_type);
    msg = "HTTP/1.1 304 Not Modified\r\n";
    TEST_PARSE(SUCCESS, msg, context);
    EXPECT_EQ_INT(304, context.status_code);
    EXPECT_EQ_INT(RESPONSE, context.msg_type);
    msg = "HTTP/1.1 200OK\r\n";
    TEST_PARSE(ILLEGAL_START_LINE, msg, context);
    EXPECT_EQ_INT(UNKNOWN, context.msg_type);
    msg = "";
    TEST_PARSE(ILLEGAL_START_LINE, msg, context);
    EXPECT_EQ_INT(UNKNOWN, context.msg_type);

    FREE_CONTEXT(context);
}

void test_parse_header() {
    char *msg, *name, *value, *header_end;
    msg = "Date: \tTue, 12 Dec 2017 11:40:15 GMT\r\n\r\n"; name = "Date";
    EXPECT_EQ_INT(SUCCESS, parse_header(msg, name, &value, &header_end));
    EXPECT_EQ_STRING("Tue, 12 Dec 2017 11:40:15 GMT", value, 29);
    free(value); value = NULL;
    msg = "Host:    www.baidu.com  \t    \r\n\r\n"; name = "Host";
    EXPECT_EQ_INT(SUCCESS, parse_header(msg, name, &value, &header_end));
    EXPECT_EQ_STRING("www.baidu.com", value, 13);
    free(value); value = NULL;
    msg = "Host: \t \t  \r\n\r\n"; name = "Host";
    EXPECT_EQ_INT(SUCCESS, parse_header(msg, name, &value, &header_end));
    EXPECT_EQ_STRING("", value, 0);
    free(value); value = NULL;
    msg = ""; name = "error";
    EXPECT_EQ_INT(ILLEGAL_HEADERS, parse_header(msg, name, &value, &header_end));
    msg = "ff: fff\r"; name = "error";
    EXPECT_EQ_INT(ILLEGAL_HEADERS, parse_header(msg, name, &value, &header_end));
    msg = "\r\n"; name = "error";
    EXPECT_EQ_INT(HEADER_NOT_FOUND, parse_header(msg, name, &value, &header_end));
}

void test_parse_host() {
    char *msg;
    http_context context;
    INIT_CONTEXT(context);

    msg = "www.baidu.com:443";
    FREE_CONTEXT(context);
    EXPECT_EQ_INT(SUCCESS, parse_host(msg, &context));
    EXPECT_EQ_STRING("www.baidu.com", context.host, 13);
    EXPECT_EQ_INT(443, context.port);
    msg = "www.baidu.com";
    FREE_CONTEXT(context);
    EXPECT_EQ_INT(SUCCESS, parse_host(msg, &context));
    EXPECT_EQ_STRING("www.baidu.com", context.host, 13);
    EXPECT_EQ_INT(80, context.port);
    msg = "";
    FREE_CONTEXT(context);
    EXPECT_EQ_INT(ILLEGAL_HEADERS, parse_host(msg, &context));

    FREE_CONTEXT(context);
}

#define TEST_GET_IP(err, context, ip_addr, length)\
    do {\
        EXPECT_EQ_INT(err, get_ip_from_host(&context));\
        EXPECT_EQ_STRING(ip_addr, context.ip_addr, length);\
    }while(0)

void test_get_ip_from_host() {
    char ip_addr[IP_STR_MAXSIZE];
    http_context context;
    INIT_CONTEXT(context);

    context.host = "localhost"; memset(ip_addr, 0, IP_STR_MAXSIZE); memcpy_s(ip_addr, IP_STR_MAXSIZE, "127.0.0.1", 10);
    TEST_GET_IP(SUCCESS, context, ip_addr, IP_STR_MAXSIZE-1);
    context.host = "www.liaoxuefeng.com"; memset(ip_addr, 0, IP_STR_MAXSIZE); memcpy_s(ip_addr, IP_STR_MAXSIZE, "121.43.166.29", 14);
    TEST_GET_IP(SUCCESS, context, ip_addr, IP_STR_MAXSIZE-1);
    context.host = "hehhehhehehhe"; memset(ip_addr, 0, IP_STR_MAXSIZE); memcpy_s(ip_addr, IP_STR_MAXSIZE, "0.0.0.0", 8);
    TEST_GET_IP(GET_IP_FAIL, context, ip_addr, IP_STR_MAXSIZE-1);

    /* TODO */
    //context.url = "cuiqingcai.com"; ip_addr = "0.0.0.0";
    //get_ip_from_host(&context);
}

#define PROMPT_COMBINE_ERROR(expect, actual, when) \
    do{\
        if (expect != actual) {\
            printf(when);\
            printf(" ERROR: %d\n", WSAGetLastError());\
        }\
    }while(0)
void combine_test() {
    int ret;
    int time_out = 5000;
    http_context context;
    /* web cache socket */
    SOCKET web_cache_socket;
    struct sockaddr_in web_cache_addr;
    web_cache_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    web_cache_addr.sin_family = AF_INET;
    web_cache_addr.sin_addr.S_un.S_addr = INADDR_ANY;
    web_cache_addr.sin_port = htons(WEB_CACHE_PORT);

    /* http socket */
    char http_buf[16349];
    SOCKET http_socket;
    struct sockaddr_in http_addr;
    char buf[1024];
    struct in_addr http_target_addr;
    int http_addr_len, http_recv_len;
    http_addr.sin_family = AF_INET;

    /* client socket */
    SOCKET client;
    struct sockaddr_in client_addr;
    int client_addr_len;
    char client_buf[1025];
    int client_recv_len;

    /* bind and listen */
    ret = bind(web_cache_socket, (struct sockaddr*)&web_cache_addr, sizeof(web_cache_addr));
    PROMPT_COMBINE_ERROR(0, ret, "bind");
    ret = listen(web_cache_socket, DEFAULT_BACKLOG);
    PROMPT_COMBINE_ERROR(0, ret, "listen");

    while(1) {
        INIT_CONTEXT(context);

        /* listen request */
        printf("Waiting: %d\n", WSAGetLastError());
        client_addr_len = sizeof(client_addr);
        client_addr.sin_family = AF_INET;
        client = accept(web_cache_socket, (struct sockaddr*)&client_addr, &client_addr_len);
        printf("Accept: %d\n", WSAGetLastError());
        client_recv_len = recv(client, client_buf, 1024, 0);
        printf("Recv: %d\n", WSAGetLastError());
        if (client_recv_len <= 0) continue;
        client_buf[client_recv_len] = 0;
        printf("%s\n", client_buf);

        /* modify context */
        printf("parse: %d\n", parse(client_buf, &context));
        printf("get ip: %s\n", context.ip_addr);

        /* modify http_addr */
        inet_pton(AF_INET, context.ip_addr, (void*)&http_target_addr);
        http_addr.sin_addr.S_un.S_addr = http_target_addr.S_un.S_addr;
        printf("change: %d\n", WSAGetLastError());
        http_addr.sin_port = htons(context.port);

        /* send request */
        printf("Sending:%d\n", WSAGetLastError());
        http_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        setsockopt(http_socket, SOL_SOCKET, SO_RCVTIMEO, (char*)&time_out, sizeof(int));
        connect(http_socket, (struct sockaddr*)&http_addr, sizeof(http_addr));
        send(http_socket, client_buf, client_recv_len, 0);
        
        printf("ReSending: %d\n", WSAGetLastError());
        while(1) {
            /* listen response */
            http_recv_len = recv(http_socket, http_buf, 16348, 0);
            if (http_recv_len <= 0) break;
            printf("recv: %d btyes\n", http_recv_len);
            /* send to client */
            send(client, http_buf, http_recv_len, 0);
        }
        
        closesocket(client);
        closesocket(http_socket);

        FREE_CONTEXT(context);
    }
}

int main(int argc, char* argv[]) {
#ifdef _WINDOWS
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif
    int err;
    WSADATA wsa_data;
    err = WSAStartup(MAKEWORD(2, 2), &wsa_data);
    if (err) { printf("Line: %d WSAStartup failed!\n", __LINE__); return 1; }

    //test_parse_start_line();
    //test_parse_header();
    //test_parse_host();
    //test_get_ip_from_host();
    //printf("%d/%d (%3.2f%%) passed\n", test_pass, test_count, test_pass * 100.0 / test_count);

    combine_test();

    WSACleanup();
    return main_ret;
}