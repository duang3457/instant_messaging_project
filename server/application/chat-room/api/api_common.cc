#include "api_common.h"

#include <openssl/rand.h>
#include <uuid/uuid.h>
 

int GetUsernameByToken(string &username, string &token) {
    int ret = 0;
    CacheManager *cache_manager = CacheManager::getInstance();
    CacheConn *cache_conn = cache_manager->GetCacheConn("token");
    AUTO_REL_CACHECONN(cache_manager, cache_conn);
    string email;
    if (cache_conn) {
        email  = cache_conn->Get(token);    //校验token和用户名的关系
        LOG_INFO << "email: " << email;
        if(email.empty()) {
            return -1;
        } else {
            return GetUsernameByEmail(email, username);
        }
    } else {
        return -1;
    }
}

int GetUserIdByUsername(int32_t &userid,string &username) {
    int ret = 0;
    CDBManager *db_manager = CDBManager::getInstance();
    CDBConn *db_conn = db_manager->GetDBConn("chatroom_slave");
    AUTO_REL_DBCONN(db_manager, db_conn);   //析构时自动归还连接

   //获取用户id
    string strSql = FormatString("select id from users where username='%s'", username.c_str());
    CResultSet *result_set = db_conn->ExecuteQuery(strSql.c_str());
    if (result_set && result_set->Next()) {  
        // 存在在返回
        userid = result_set->GetInt("id");
        LOG_INFO <<"username: " << username <<", userid: " << userid;
        ret = 0;
    } else {                        // 说明用户不存在
        ret = -1;
    }

    delete result_set;

    return ret;
}
 
int GetUsernameByEmail(string &email,string &username) {
    int ret = 0;
    CDBManager *db_manager = CDBManager::getInstance();
    CDBConn *db_conn = db_manager->GetDBConn("chatroom_slave");
    AUTO_REL_DBCONN(db_manager, db_conn);   //析构时自动归还连接

   //获取用户id
    string strSql = FormatString("select username from users where email='%s'", email.c_str());
    CResultSet *result_set = db_conn->ExecuteQuery(strSql.c_str());
    if (result_set && result_set->Next()) {  
        // 存在在返回
        username = result_set->GetString("username");
        LOG_INFO <<"username: " << username;
        ret = 0;
    } else {                        // 说明用户不存在
        ret = -1;
    }

    delete result_set;

    return ret;
}
 
std::string api_error_id_to_string(api_error_id input)
{
    switch (input)
    {
    case api_error_id::login_failed: return "LOGIN_FAILED";
    case api_error_id::username_exists: return "USERNAME_EXISTS";
    case api_error_id::email_exists: return "EMAIL_EXISTS";
    case api_error_id::bad_request:
    default: return "BAD_REQUEST";
    }
}

std::string generateUUID() {
    uuid_t uuid;
    uuid_generate_time_safe(uuid);  //调用uuid的接口
 
    char uuidStr[40] = {0};
    uuid_unparse(uuid, uuidStr);     //调用uuid的接口
 
    return std::string(uuidStr);
}


int SetCookie(string &email, string &token) {
    int ret = 0;

    CacheManager *cache_manager = CacheManager::getInstance();
    CacheConn *cache_conn = cache_manager->GetCacheConn("token");
    AUTO_REL_CACHECONN(cache_manager, cache_conn);

    token = generateUUID(); // 生成唯一的token

    if (cache_conn) {
        //token - 用户名, 86400有效时间为24小时  有效期可以自己修改
        cache_conn->SetEx(token, 86400, email); // redis做超时
    } else {
        ret = -1;
    }

    return ret;
}
 