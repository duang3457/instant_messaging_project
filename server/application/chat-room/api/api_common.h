#ifndef _API_COMMON_H_
#define _API_COMMON_H_
#include "cache_pool.h" // redis操作头文件
#include "db_pool.h"    //MySQL操作头文件
#include <jsoncpp/json/json.h> // jsoncpp头文件
#include "muduo/base/Logging.h" // Logger日志头文件
#include <string>
#include "base64.h" 

using std::string;

// 某些参数没有使用时用该宏定义避免报警告
#define UNUSED(expr)                                                           \
    do {                                                                       \
        (void)(expr);                                                          \
    } while (0)
 
// 它是一个模板函数，实现也需要放在头文件里，否则报错
template <typename... Args>
std::string FormatString(const std::string &format, Args... args) {
    auto size = std::snprintf(nullptr, 0, format.c_str(), args...) +
                1; // Extra space for '\0'
    std::unique_ptr<char[]> buf(new char[size]);
    std::snprintf(buf.get(), size, format.c_str(), args...);
    return std::string(buf.get(), buf.get() + size - 1); // We don't want the '\0' inside
}
 
// Used within api_error, as a way to communicate specific error conditions
// to the client.
enum class api_error_id
{
    // generic, when there is not a more specific error ID
    bad_request = 0,

    // A login attempt failed (e.g. bad username or password)
    login_failed,

    // Failure during account creation, the selected email already exists
    email_exists,

    // Failure during account creation, the selected username already exists
    username_exists,
};


// Error code enum for errors originated within our application
enum class errc
{
    redis_parse_error = 1,  // Data retrieved from Redis didn't match the format we expected
    redis_command_failed,   // A Redis command failed execution (e.g. we provided the wrong number of args)
    websocket_parse_error,  // Data received from the client didn't match the format we expected
    username_exists,        // couldn't create user, duplicate username
    email_exists,           // couldn't create user, duplicate username
    not_found,              // couldn't retrieve a certain resource, it doesn't exist
    invalid_password_hash,  // we found a password hash that was malformed
    already_exists,         // an entity can't be created because it already exists
    requires_auth,   // the requested resource requires authentication, but credentials haven't been provided
                     // or are invalid
    invalid_base64,  // attempt to decode an invalid base64 string
    uncaught_exception,    // an API handler threw an unexpected exception
    invalid_content_type,  // an endpoint received an unsupported Content-Type
};


std::string api_error_id_to_string(api_error_id input);

 std::string generate_identifier();
 int SetCookie(string &user_name, string &token);

int GetUsernameByEmail(string &email,string &username);
//根据token获取用户名，如果获取失败返回非0 值，正常返回0
 int GetUsernameByToken(string &username, string &token);
 int GetUserIdByUsername(int32_t &userid,string &username);
#endif