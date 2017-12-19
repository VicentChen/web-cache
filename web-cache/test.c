#define _WINDOWS
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

void test_queue() {
    int num_len = 4;
    int num[4];
    queue q;
    init_queue(q); set_queue_size(&q, num_len);
    for (int i = 0; i < num_len; i++) num[i] = i;

    for (int i = 0; i < num_len; i++)
        EXPECT_EQ_INT(SUCCESS, en_queue(&q, num + i));
    EXPECT_EQ_INT(QUEUE_FULL, en_queue(&q, num));
    for (int i = 0; i < num_len; i++)
        EXPECT_EQ_INT(num[i], *(int*)(de_queue(&q)));
    EXPECT_EQ_INT(0, ((int)de_queue(&q)));

    for (int i = 0; i < num_len - 1; i++)
        EXPECT_EQ_INT(SUCCESS, en_queue(&q, num + i));
    for (int i = 0; i < num_len - 1; i++)
        EXPECT_EQ_INT(num[i], *(int*)(de_queue(&q)));
    for (int i = 0; i < num_len; i++)
        EXPECT_EQ_INT(SUCCESS, en_queue(&q, num + i));
    EXPECT_EQ_INT(QUEUE_FULL, en_queue(&q, num));
    for (int i = 0; i < num_len; i++)
        EXPECT_EQ_INT(num[i], *(int*)(de_queue(&q)));
    EXPECT_EQ_INT(0, ((int)de_queue(&q)));

    for (int i = 0; i < num_len / 2; i++)
        EXPECT_EQ_INT(SUCCESS, en_queue(&q, num + i));
    for (int i = 0; i < num_len / 2; i++)
        EXPECT_EQ_INT(num[i], *(int*)(de_queue(&q)));
    for (int i = 0; i < num_len; i++) num[i] = i;
    for (int i = 0; i < num_len; i++)
        EXPECT_EQ_INT(SUCCESS, en_queue(&q, num + i));
    EXPECT_EQ_INT(QUEUE_FULL, en_queue(&q, num));
    for (int i = 0; i < num_len; i++)
        EXPECT_EQ_INT(num[i], *(int*)(de_queue(&q)));
    EXPECT_EQ_INT(0, ((int)de_queue(&q)));

    free_queue(q);
}

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
    EXPECT_EQ_INT(GET, context.method_type);
    msg = "CONNECT www.baidu.com:443 HTTP/1.0\r\n";
    TEST_PARSE(SUCCESS, msg, context);
    EXPECT_EQ_STRING("www.baidu.com:443", context.url, 17);
    EXPECT_EQ_INT(REQUEST, context.msg_type);
    EXPECT_EQ_INT(NOT_GET, context.method_type);
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

void test_get_local_path() {
    http_context context;
    char* ans;
    INIT_CONTEXT(context);
    context.host = "www.baidu.com"; context.url = "http://www.baidu.com/";
    ans = "www.baidu.com/2089601105";
    get_local_path(&context);
    EXPECT_EQ_STRING(ans, context.local_path, 24);
    free(context.local_path);  context.local_path = NULL;
    context.host = "tools.ietf.org"; context.url = "https://tools.ietf.org/html/rfc1630";
    ans = "tools.ietf.org/3750935845";
    get_local_path(&context);
    EXPECT_EQ_STRING(ans, context.local_path, 25);
    free(context.local_path);  context.local_path = NULL;
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

void system_test() {
    simple_cache();
}

int main(int argc, char* argv[]) {
#ifdef _WINDOWS
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif
    int err;
    WSADATA wsa_data;
    err = WSAStartup(MAKEWORD(2, 2), &wsa_data);
    if (err) { printf("Line: %d WSAStartup failed!\n", __LINE__); return 1; }

    //test_queue();
    test_parse_start_line();
    //test_parse_header();
    //test_parse_host();
    //test_get_ip_from_host();
    //test_get_local_path();
    printf("%d/%d (%3.2f%%) passed\n", test_pass, test_count, test_pass * 100.0 / test_count);

    //system_test();

    WSACleanup();
    return main_ret;
}