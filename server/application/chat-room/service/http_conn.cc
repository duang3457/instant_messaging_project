#include <string.h>
#include <regex>

#include "muduo/base/Logging.h" 

#include "http_conn.h"
#include "http_parser.h"
#include "api_login.h"
#include "api_register.h"

void CHttpConn::OnRead(muduo::net::Buffer *buf) // CHttpConn业务层面的OnRead
{
    LOG_INFO << "进入OnRead" ;
    const char *in_buf = buf->peek();
    int32_t len = buf->readableBytes();
    
    LOG_INFO << "收到数据长度: " << len;
    http_parser_.ParseHttpContent(in_buf, len);
    
    LOG_INFO << "HTTP解析状态 - IsReadAll: " << http_parser_.IsReadAll();
    
    if(http_parser_.IsReadAll()) {
        string url = http_parser_.GetUrlString();
        LOG_INFO << "======================";
        LOG_INFO << "url : " << url;
        string content = http_parser_.GetBodyContentString();
        LOG_INFO << "content length: " << content.length();

        if (strncmp(url.c_str(), "/api/login", 10) == 0) { // 登录
            _HandleLoginRequest(url, content);
        } else if (url == "/" || url == "/index.html") { // 处理根路径请求
            // 返回简单的欢迎页面
            string html_content = 
                "<!DOCTYPE html>"
                "<html><head><title>ChatRoom Server</title></head>"
                "<body>"
                "<h1>Welcome to ChatRoom Server!</h1>"
                "<p>Server is running on port 8080</p>"
                "<p>Available APIs:</p>"
                "<ul>"
                "<li>POST /api/login - User login</li>"
                "<li>POST /api/create-account - User registration</li>"
                "<li>GET /api/html - Test page</li>"
                "</ul>"
                "</body></html>";
            
            uint32_t len_html = html_content.size();
            LOG_INFO << "HTML content length: " << len_html;
            
            char *resp_content = new char[2048];  // 增加缓冲区大小
            snprintf(resp_content, 2048, HTTP_RESPONSE_HTML, len_html, html_content.c_str());
            
            LOG_INFO << "Sending response: " << std::string(resp_content, min(200, (int)strlen(resp_content)));
            tcp_conn_->send(resp_content);
            delete[] resp_content;
        } else if (strncmp(url.c_str(), "/api/html", 9) == 0) {   //  测试网页
            _HandleHtml(url, content);
        } else if (strncmp(url.c_str(), "/api/memhtml", 12) == 0) {   //  测试网页
            _HandleMemHtml(url, content);
        }
        else if(strncmp(url.c_str(), "/api/create-account", 18) == 0) {   //  创建账号
            LOG_INFO << "进去注册逻辑";
            _HandleRegisterRequest(url, content);
        } 
        else {
            LOG_WARN << "未匹配到路径: " << url;
            char *resp_content = new char[256];
            string str_json = "{\"code\": 1, \"message\": \"Not Found\"}"; 
            uint32_t len_json = str_json.size();
            //暂时先放这里
            #define HTTP_RESPONSE_REQ                                                     \
                "HTTP/1.1 404 Not Found\r\n"                                                      \
                "Connection: close\r\n"                                                     \
                "Content-Length: %d\r\n"                                                    \
                "Content-Type: application/json; charset=utf-8\r\n\r\n%s"
            snprintf(resp_content, 256, HTTP_RESPONSE_REQ, len_json, str_json.c_str()); 	
            tcp_conn_->send(resp_content);
            delete[] resp_content;
        }
        
        buf->retrieveAll(); // 清空缓冲区
    } else {
        LOG_WARN << "HTTP请求未完全接收，等待更多数据...";
    }
}

// 账号注册处理
int CHttpConn::_HandleRegisterRequest(string &url, string &post_data) {
    
    string resp_json="";
	int ret = ApiRegisterUser(post_data, resp_json);
	char *http_data = new char[HTTP_RESPONSE_JSON_MAX];
    int code = 200;
    string code_msg;
	
    if(ret == 0) {
        LOG_INFO << "注册正常, cookie: " <<resp_json;
        //注册正常      //返回token
        code = 204;
        code_msg= "No Content";
        snprintf(http_data, HTTP_RESPONSE_JSON_MAX, HTTP_RESPONSE_WITH_COOKIE, code, code_msg.c_str(), 
            resp_json.c_str(), 0, ""); 	
    } else {
        LOG_INFO << "注册失败, resp_json: " <<resp_json;
        code = 400;
        code_msg= "Bad Request";
        snprintf(http_data, HTTP_RESPONSE_JSON_MAX, HTTP_RESPONSE_WITH_CODE, code, code_msg.c_str(), 
            (int)resp_json.length(),  resp_json.c_str()); 	
    }
    tcp_conn_->send(http_data);
    LOG_INFO << "================ " ;
    LOG_INFO << "http_data: "<< http_data;
    delete[] http_data;
    
    return 0;
}


int CHttpConn::_HandleLoginRequest(string &url, string &post_data)
{
	string resp_json;
	int ret = ApiUserLogin(post_data, resp_json);
	char *http_data = new char[HTTP_RESPONSE_JSON_MAX];
	int code = 200;
    string code_msg;
	
    if(ret == 0) {
        LOG_INFO << "登录正常, cookie: " <<resp_json;
        //登录正常      //返回token
        code = 204;
        code_msg= "No Content";
        snprintf(http_data, HTTP_RESPONSE_JSON_MAX, HTTP_RESPONSE_WITH_COOKIE, code, code_msg.c_str(), 
            resp_json.c_str(), 0, ""); 	
    } else {
        LOG_INFO << "登录失败, resp_json: " <<resp_json;
        code = 400;
        code_msg= "Bad Request";
        snprintf(http_data, HTTP_RESPONSE_JSON_MAX, HTTP_RESPONSE_WITH_CODE, code, code_msg.c_str(), 
            (int)resp_json.length(),  resp_json.c_str()); 	
    }

    tcp_conn_->send(http_data);
    LOG_INFO << "  http_data: "<< http_data;
    delete[] http_data;
    return 0;
}

// 处理HTML页面请求
int CHttpConn::_HandleHtml(string &url, string &post_data) {
    (void)url;      // 避免未使用参数警告
    (void)post_data;
    
    std::string html_content = R"(
<!DOCTYPE html>
<html>
<head>
    <title>ChatRoom Test</title>
</head>
<body>
    <h1>Welcome to ChatRoom</h1>
    <p>This is a test page.</p>
</body>
</html>
)";
    
    char *http_data = new char[HTTP_RESPONSE_HTM_MAX];
    snprintf(http_data, HTTP_RESPONSE_HTM_MAX, HTTP_RESPONSE_HTML, (int)html_content.length(), html_content.c_str());
    tcp_conn_->send(http_data);
    delete[] http_data;
    return 0;
}

// 处理内存HTML页面请求
int CHttpConn::_HandleMemHtml(string &url, string &post_data) {
    (void)url;      // 避免未使用参数警告
    (void)post_data;
    
    std::string html_content = R"(
        <!DOCTYPE html>
        <html>
        <head>
            <title>ChatRoom Memory Test</title>
        </head>
        <body>
            <h1>Memory Test Page</h1>
            <p>This is a memory test page for ChatRoom.</p>
        </body>
        </html>
        )";
    
    char *http_data = new char[HTTP_RESPONSE_HTM_MAX];
    snprintf(http_data, HTTP_RESPONSE_HTM_MAX, HTTP_RESPONSE_HTML, (int)html_content.length(), html_content.c_str());
    tcp_conn_->send(http_data);
    delete[] http_data;
    return 0;
}

CHttpConn::CHttpConn(muduo::net::TcpConnectionPtr tcp_conn):
    tcp_conn_(tcp_conn)
{
    // 安全地获取 UUID
    try {
        if (!tcp_conn_->getContext().empty()) {
            uuid_ = boost::any_cast<uint32_t>(tcp_conn_->getContext());
        } else {
            LOG_ERROR << "TCP connection context is empty";
            uuid_ = 0;  // 设置默认值
        }
    } catch (const boost::bad_any_cast& e) {
        LOG_ERROR << "Bad any_cast when getting UUID in CHttpConn constructor: " << e.what();
        uuid_ = 0;  // 设置默认值
    }
    LOG_INFO << "构造CHttpConn uuid: "<< uuid_ ;
}

CHttpConn::~CHttpConn() {
    LOG_INFO << "析构CHttpConn uuid: "<< uuid_ ;
}

void CHttpConn::send(const string &data) {
    LOG_INFO << "send: " << data;
    tcp_conn_->send(data.c_str(), data.size());
}

std::string  CHttpConn::getSubdirectoryFromHttpRequest(const std::string& httpRequest) {
    // 正则表达式匹配请求行中的路径部分
    std::regex pathRegex(R"(GET\s+([^\s]+)\s+HTTP\/1\.1)");
    std::smatch pathMatch;

    // 查找路径
    if (std::regex_search(httpRequest, pathMatch, pathRegex)) {
        return pathMatch[1];  // 返回路径部分
    }
    return "";  // 如果没有找到路径，返回空字符串
}