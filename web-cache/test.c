#ifdef _WINDOWS
#define _CRTDBG_MAP_ALLOC
#include <crtdbg.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "web-cache.h"


static int main_ret = 0;
static int test_count = 0;
static int test_pass = 0;

static char* http_get_msg = ""
"GET http://www.baidu.com/ HTTP/1.1\r\n"
"Accept: text/html, application/xhtml+xml, image/jxr, */*\r\n"
"Accept-Language: zh-Hans-CN,zh-Hans;q=0.8,en-US;q=0.5,en;q=0.3\r\n"
"User-Agent: Mozilla/5.0 (Windows NT 10.0; WOW64; Trident/7.0; Touch; rv:11.0) like Gecko\r\n"
"Accept-Encoding: gzip, deflate\r\n"
"Host: www.baidu.com\r\n"
"Proxy-Connection: Keep-Alive\r\n"
"\r\n";

static char* https_get_msg = ""
"CONNECT www.baidu.com:443 HTTP/1.0\r\n"
"User-Agent: Mozilla/5.0 (Windows NT 10.0; WOW64; Trident/7.0; Touch; rv:11.0) like Gecko\r\n"
"Content-Length: 0\r\n"
"Host: www.baidu.com\r\n"
"Proxy-Connection: Keep-Alive\r\n"
"Pragma: no-cache\r\n"
"\r\n";

static char* https_response_msg = ""
"HTTP/1.1 200 OK\r\n"
"Bdpagetype: 2\r\n"
"Bdqid: 0x9be33b1400004616\r\n"
"Bduserid: 98016631\r\n"
"Cache-Control: private\r\n"
"Connection: Keep-Alive\r\n"
"Content-Encoding: gzip\r\n"
"Content-Type: text/html;charset=utf-8\r\n"
"Date: Mon, 11 Dec 2017 07:43:31 GMT\r\n"
"Expires: Mon, 11 Dec 2017 07:43:31 GMT\r\n"
"Server: BWS/1.1\r\n"
"Set-Cookie: BDSVRTM=333; path=/\r\n"
"Set-Cookie: BD_HOME=1; path=/\r\n"
"Set-Cookie: H_PS_PSSID=1444_13548_21112_17001_25178; path=/; domain=.baidu.com\r\n"
"Strict-Transport-Security: max-age=172800\r\n"
"X-Ua-Compatible: IE=Edge,chrome=1\r\n"
"Transfer-Encoding: chunked\r\n"
"\r\n";

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
#define EXPECT_EQ_STRING(expect, actual, alength) \
    EXPECT_EQ_BASE(sizeof(expect) - 1 == alength && memcmp(expect, actual, alength + 1) == 0, expect, actual, "%s")



#define INIT_INFO(info)\
    do{\
        info.domain = NULL;\
        info.ip_addr = NULL;\
        info.local_path = NULL;\
    } while(0)

#define CLEAN_TEST_INFO(info) \
    do {\
        if(info.domain) {\
            free(info.domain);\
            info.domain = NULL; \
        }\
    }while(0)

#define TEST_CHECK(err, msg, info)\
    do {\
        CLEAN_TEST_INFO(info);\
        EXPECT_EQ_INT(err, check_start_line(msg, &info));\
    }while(0)

void test_check_request_start_line() {
    char *msg;
    cache_info info;
    INIT_INFO(info);
    /* request part */
    msg = "GET http://www.baidu.com/ HTTP/1.1\r\n";
    TEST_CHECK(SUCCESS, msg, info);
    EXPECT_EQ_STRING("http://www.baidu.com/", info.domain, 21);
    msg = "CONNECT www.baidu.com:443 HTTP/1.0\r\n";
    TEST_CHECK(SUCCESS, msg, info);
    EXPECT_EQ_STRING("www.baidu.com:443", info.domain, 17);
    msg = "GEThttp://www.baidu.com/ \r\n";
    TEST_CHECK(START_LINE_ILLEGAL, msg, info);
    /* response part */
    msg = "HTTP/1.1 200 OK\r\n";
    EXPECT_EQ_INT(SUCCESS, check_start_line(msg, NULL));
    msg = "HTTP/1.1 200OK\r\n";
    EXPECT_EQ_INT(START_LINE_ILLEGAL, check_start_line(msg, NULL));
}

int main(int argc, char* argv[]) {
#ifdef _WINDOWS
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif
    test_check_request_start_line();
    printf("%d/%d (%3.2f%%) passed\n", test_pass, test_count, test_pass * 100.0 / test_count);
    return main_ret;
}
