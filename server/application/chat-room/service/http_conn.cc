
#include <string.h>

#include "muduo/base/Logging.h" 
#include "http_conn.h"
#include "http_parser.h"
#include "api_login.h"

void CHttpConn::OnRead(muduo::net::Buffer *buf) // CHttpConn业务层面的OnRead
{
    const char *in_buf = buf->peek();
    int32_t len = buf->readableBytes();
    
    http_parser_.ParseHttpContent(in_buf, len);
    
    if(http_parser_.IsReadAll()) {
        string url = http_parser_.GetUrlString();
        string content = http_parser_.GetBodyContentString();
        LOG_INFO << "url: " << url << ", content: " << content;   

        if (strncmp(url.c_str(), "/api/login", 10) == 0) { // 登录
            _HandleLoginRequest(url, content);
        }   else if (strncmp(url.c_str(), "/api/html", 9) == 0) {   //  测试网页
            _HandleHtml(url, content);
        } else if (strncmp(url.c_str(), "/api/memhtml", 12) == 0) {   //  测试网页
            _HandleMemHtml(url, content);
        }
        else if(strncmp(url.c_str(), "/api/create-account", 18) == 0) {   //  创建账号
            _HandleRegisterRequest(url, content);
        } 
        
        else {
            char *resp_content = new char[256];
            string str_json = "{\"code\": 1}"; 
            uint32_t len_json = str_json.size();
            //暂时先放这里
            #define HTTP_RESPONSE_REQ                                                     \
                "HTTP/1.1 404 OK\r\n"                                                      \
                "Connection:close\r\n"                                                     \
                "Content-Length:%d\r\n"                                                    \
                "Content-Type:application/json;charset=utf-8\r\n\r\n%s"
            snprintf(resp_content, 256, HTTP_RESPONSE_REQ, len_json, str_json.c_str()); 	
            tcp_conn_->send(resp_content);
        }
    }
}

int CHttpConn::_HandleLoginRequest(string &url, string &post_data)
{
	string resp_json;
	int ret = ApiUserLogin(post_data, resp_json);
	char *http_body = new char[HTTP_RESPONSE_JSON_MAX];
	int code = 200;
    string code_msg;
	
    if(ret == 0) {
        LOG_INFO << "登录正常, cookie: " <<resp_json;
        //注册正常      //返回token
        code = 204;
        code_msg= "No Content";
        snprintf(http_body, HTTP_RESPONSE_JSON_MAX, HTTP_RESPONSE_WITH_COOKIE, code, code_msg.c_str(), 
            resp_json.c_str(), 0, ""); 	
    } else {
        LOG_INFO << "登录失败, resp_json: " <<resp_json;
        uint32_t len = resp_json.length();
        code = 404;
        code_msg= "Bad Request";
        snprintf(http_body, HTTP_RESPONSE_JSON_MAX, HTTP_RESPONSE_WITH_CODE, code, code_msg.c_str(), 
            resp_json.length(),  resp_json.c_str()); 	
    }

	
    tcp_conn_->send(http_body);
    LOG_INFO << "  http_body: "<< http_body;
    delete[] http_body;
    return 0;
}

