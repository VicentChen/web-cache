#ifdef _WINDOWS
#define _CRTDBG_MAP_ALLOC
#include <crtdbg.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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
    EXPECT_EQ_BASE(sizeof(expect) - 1 == alength && memcmp(expect, actual, alength + 1) == 0, expect, actual, "%s")



#define INIT_CONTEXT(context)\
    do{\
        context.msg = NULL;\
        context.url = NULL;\
        context.domain = NULL;\
        context.local_path = NULL;\
        memset(context.ip_addr, 0, IP_STR_MAXSIZE);\
    } while(0)
#define FREE_CONTEXT(context)\
    do{\
        context.msg = NULL;\
        if(context.url) { free(context.url); context.url = NULL; }\
        if(context.domain) { free(context.domain); context.domain = NULL; }\
        if(context.local_path) { free(context.local_path); context.local_path = NULL; }\
    }while(0)

#define TEST_PARSE(err, msg, context)\
    do {\
        FREE_CONTEXT(context);\
        EXPECT_EQ_INT(err, parse(msg, &context));\
    }while(0)

void test_parse_start_line() {
    char *msg;
    http_context context;
    INIT_CONTEXT(context);

    /* request part */
    msg = "GET http://www.baidu.com/ HTTP/1.1\r\n";
    TEST_PARSE(SUCCESS, msg, context);
    EXPECT_EQ_STRING("http://www.baidu.com/", context.url, 21);
    msg = "CONNECT www.baidu.com:443 HTTP/1.0\r\n";
    TEST_PARSE(SUCCESS, msg, context);
    EXPECT_EQ_STRING("www.baidu.com:443", context.url, 17);
    msg = "GEThttp://www.baidu.com/ \r\n";
    TEST_PARSE(ILLEGAL_START_LINE, msg, context);
    /* response part */
    msg = "HTTP/1.1 200 OK\r\n";
    TEST_PARSE(SUCCESS, msg, context);
    EXPECT_EQ_INT(200, context.status_code);
    msg = "HTTP/1.1 304 Not Modified\r\n";
    TEST_PARSE(SUCCESS, msg, context);
    EXPECT_EQ_INT(304, context.status_code);
    msg = "HTTP/1.1 200OK\r\n";
    TEST_PARSE(ILLEGAL_START_LINE, msg, context);

    FREE_CONTEXT(context);
}

#define TEST_GET_IP(err, context, ip_addr, length)\
    do {\
        EXPECT_EQ_INT(err, get_ip_from_domain(&context));\
        EXPECT_EQ_STRING(ip_addr, context.ip_addr, length);\
    }while(0)

void test_get_ip_from_domain() {
    char ip_addr[IP_STR_MAXSIZE];
    http_context context;
    INIT_CONTEXT(context);

    context.domain = "localhost"; memset(ip_addr, 0, IP_STR_MAXSIZE); memcpy_s(ip_addr, IP_STR_MAXSIZE, "127.0.0.1", 10);
    TEST_GET_IP(SUCCESS, context, ip_addr, IP_STR_MAXSIZE-1);
    context.domain = "www.liaoxuefeng.com"; memset(ip_addr, 0, IP_STR_MAXSIZE); memcpy_s(ip_addr, IP_STR_MAXSIZE, "121.43.166.29", 14);
    TEST_GET_IP(SUCCESS, context, ip_addr, IP_STR_MAXSIZE-1);
    context.domain = "hehhehhehehhe"; memset(ip_addr, 0, IP_STR_MAXSIZE); memcpy_s(ip_addr, IP_STR_MAXSIZE, "0.0.0.0", 8);
    TEST_GET_IP(GET_IP_FAIL, context, ip_addr, IP_STR_MAXSIZE-1);

    /* TODO */
    //context.url = "cuiqingcai.com"; ip_addr = "0.0.0.0";
    //get_ip_from_domain(&context);
}

int main(int argc, char* argv[]) {
#ifdef _WINDOWS
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif
    int err;
    WSADATA wsa_data;
    err = WSAStartup(MAKEWORD(2, 2), &wsa_data);
    if (err) { printf("Line: %d WSAStartup failed!\n", __LINE__); return 1; }

    test_parse_start_line();
    test_get_ip_from_domain();
    printf("%d/%d (%3.2f%%) passed\n", test_pass, test_count, test_pass * 100.0 / test_count);

    WSACleanup();
    return main_ret;
}
