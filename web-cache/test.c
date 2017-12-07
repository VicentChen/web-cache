#ifdef _WINDOWS
#define _CRTDBG_MAP_ALLOC
#include <crtdbg.h>
#endif
#include <stdio.h>
#include "web-cache.h"


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

static char* https_get_msg = ""
"CONNECT www.baidu.com:443 HTTP/1.0\r\n"
"User-Agent: Mozilla/5.0 (Windows NT 10.0; WOW64; Trident/7.0; Touch; rv:11.0) like Gecko\r\n"
"Content-Length: 0\r\n"
"Host: www.baidu.com\r\n"
"Proxy-Connection: Keep-Alive\r\n"
"Pragma: no-cache\r\n"
"\r\n";

static char* http_get_msg = ""
"GET http://www.baidu.com/ HTTP/1.1\r\n"
"Accept: text/html, application/xhtml+xml, image/jxr, */*\r\n"
"Accept-Language: zh-Hans-CN,zh-Hans;q=0.8,en-US;q=0.5,en;q=0.3\r\n"
"User-Agent: Mozilla/5.0 (Windows NT 10.0; WOW64; Trident/7.0; Touch; rv:11.0) like Gecko\r\n"
"Accept-Encoding: gzip, deflate\r\n"
"Host: www.baidu.com\r\n"
"Proxy-Connection: Keep-Alive\r\n"
"\r\n";

void test_check_start_line() {
    /* request part */
    EXPECT_EQ_INT(MSG_CHECK_OK, check_start_line(http_get_msg));
    EXPECT_EQ_INT(MSG_CHECK_OK, check_start_line(https_get_msg));
    // TODO: add post message
    // TODO: add fault message
    char *fault_msg = "GET http://www.baidu.com/ HTTP/1.1\r";
    EXPECT_EQ_INT(MSG_HTTP_ILLEGAL, check_start_line(fault_msg));
    fault_msg = "GET http://www.baidu.com/ HTTP/1.1\n";
    EXPECT_EQ_INT(MSG_HTTP_ILLEGAL, check_start_line(fault_msg));
    fault_msg = "GET http://www.baidu.com/ HTTP/1.1";
    EXPECT_EQ_INT(MSG_HTTP_ILLEGAL, check_start_line(fault_msg));
    fault_msg = "GET http://www.baidu.com/ ";
    EXPECT_EQ_INT(MSG_HTTP_ILLEGAL, check_start_line(fault_msg));
    fault_msg = "GET http://www.baidu.com/ HTTPS/1.1";
    EXPECT_EQ_INT(MSG_HTTP_ILLEGAL, check_start_line(fault_msg));
    fault_msg = "GET http://www.baidu.com/ T/1.1";
    EXPECT_EQ_INT(MSG_HTTP_ILLEGAL, check_start_line(fault_msg));
    /* TODO: response part */
}

int main(int argc, char* argv[]) {
#ifdef _WINDOWS
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif
    test_check_start_line();
    printf("%d/%d (%3.2f%%) passed\n", test_pass, test_count, test_pass * 100.0 / test_count);
    return main_ret;
}
