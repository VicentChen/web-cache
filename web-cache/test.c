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



#define INIT_INFO(info)\
    do{\
        info.domain = NULL;\
        memset(info.ip_addr, 0, IP_STR_MAXSIZE);\
        info.local_path = NULL;\
    } while(0)
#define FREE_INFO(info)\
    do{\
        if(info.domain) { free(info.domain); info.domain = NULL; }\
        if(info.local_path) { free(info.local_path); info.local_path = NULL; }\
    }while(0)

#define TEST_PARSE(err, msg, info)\
    do {\
        FREE_INFO(info);\
        EXPECT_EQ_INT(err, parse_start_line(msg, &info));\
    }while(0)

void test_parse_start_line() {
    char *msg;
    cache_info info;
    INIT_INFO(info);

    /* request part */
    msg = "GET http://www.baidu.com/ HTTP/1.1\r\n";
    TEST_PARSE(SUCCESS, msg, info);
    EXPECT_EQ_STRING("http://www.baidu.com/", info.domain, 21);
    msg = "CONNECT www.baidu.com:443 HTTP/1.0\r\n";
    TEST_PARSE(SUCCESS, msg, info);
    EXPECT_EQ_STRING("www.baidu.com:443", info.domain, 17);
    msg = "GEThttp://www.baidu.com/ \r\n";
    TEST_PARSE(ILLEGAL_START_LINE, msg, info);
    /* response part */
    msg = "HTTP/1.1 200 OK\r\n";
    TEST_PARSE(SUCCESS, msg, info);
    EXPECT_EQ_INT(200, info.status_code);
    msg = "HTTP/1.1 304 Not Modified\r\n";
    TEST_PARSE(SUCCESS, msg, info);
    EXPECT_EQ_INT(304, info.status_code);
    msg = "HTTP/1.1 200OK\r\n";
    TEST_PARSE(ILLEGAL_START_LINE, msg, info);

    FREE_INFO(info);
}

#define TEST_GET_IP(err, info, ip_addr, length)\
    do {\
        EXPECT_EQ_INT(err, get_ip_from_domain(&info));\
        EXPECT_EQ_STRING(ip_addr, info.ip_addr, length);\
    }while(0)

void test_get_ip_from_domain() {
    char ip_addr[IP_STR_MAXSIZE];
    cache_info info;
    INIT_INFO(info);

    info.domain = "localhost"; memset(ip_addr, 0, IP_STR_MAXSIZE); memcpy_s(ip_addr, IP_STR_MAXSIZE, "127.0.0.1", 10);
    TEST_GET_IP(SUCCESS, info, ip_addr, IP_STR_MAXSIZE-1);
    info.domain = "www.liaoxuefeng.com"; memset(ip_addr, 0, IP_STR_MAXSIZE); memcpy_s(ip_addr, IP_STR_MAXSIZE, "121.43.166.29", 14);
    TEST_GET_IP(SUCCESS, info, ip_addr, IP_STR_MAXSIZE-1);
    info.domain = "hehhehhehehhe"; memset(ip_addr, 0, IP_STR_MAXSIZE); memcpy_s(ip_addr, IP_STR_MAXSIZE, "0.0.0.0", 8);
    TEST_GET_IP(GET_IP_FAIL, info, ip_addr, IP_STR_MAXSIZE-1);

    /* TODO */
    //info.domain = "cuiqingcai.com"; ip_addr = "0.0.0.0";
    //get_ip_from_domain(&info);
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
