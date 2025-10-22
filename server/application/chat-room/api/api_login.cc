#include "api_login.h"

#include <iostream>
#include "muduo/base/Logging.h" // Logger日志头文件
#include <jsoncpp/json/json.h>

using namespace std;

// / 解析登录信息
int decodeLoginJson(const std::string &str_json, string &email,
                    string &password) {
    bool res;
    Json::Value root;
    Json::Reader jsonReader;
    res = jsonReader.parse(str_json, root);
    if (!res) {
        LOG_ERROR << "parse login json failed ";
        return -1;
    }
 
    if (root["email"].isNull()) {
        LOG_ERROR << "email null";
        return -1;
    }
    email = root["email"].asString();

    //密码
    if (root["password"].isNull()) {
        LOG_ERROR << "password null";
        return -1;
    }
    password = root["password"].asString();

    return 0;
}


int encodeLoginJson(api_error_id input, string message, string &str_json) {
  
    Json::Value root;
    root["id"] = api_error_id_to_string(input);
    root["message"] = message;
    Json::FastWriter writer;
    str_json = writer.write(root);
    return 0;
}


int verifyUserPassword(string &email, string &password, string &username) {
    int ret = 0;
    CDBManager *db_manager = CDBManager::getInstance();
    CDBConn *db_conn = db_manager->GetDBConn("chatroom_slave");
    AUTO_REL_DBCONN(db_manager, db_conn);   //析构时自动归还连接

    // 根据用户名查询密码
    string strSql = FormatString("select username, password from users where email='%s'", email.c_str());
    CResultSet *result_set = db_conn->ExecuteQuery(strSql.c_str());
    if (result_set && result_set->Next()) { //如果存在则读取密码
        // 存在在返回
        string db_password = result_set->GetString("password");
        username = result_set->GetString("username");
        LOG_INFO <<"username: " << username <<"mysql-pwd: " << db_password << ", user-pwd: " <<  password;
        if (db_password == password)       //对比密码是否一致
            ret = 0;                    //对比成功
        else
            ret = -1;                   //对比失败
    } else {                        // 说明用户不存在
        ret = -1;
    }
    delete result_set;
    return ret;
}

// 登录 使用  邮箱  密码
int ApiUserLogin(std::string &post_data, std::string &resp_json){
    string username;
    string email;
    string password;

    // 判断数据是否为空
    if (post_data.empty()) {
        encodeLoginJson(api_error_id::bad_request, "data is empty", resp_json);
        return -1;
    }

     // 解析json
    if (decodeLoginJson(post_data, email, password) < 0) {
        LOG_ERROR << "decodeRegisterJson failed";
        encodeLoginJson(api_error_id::bad_request, "email or password no fill", resp_json);
        return -1;
    }
    // 验证账号和密码是否匹配
    if (verifyUserPassword(email, password, username) != 0) {
        LOG_ERROR << "verifyUserPassword failed";
        encodeLoginJson(api_error_id::bad_request, "email password no match", resp_json);
        return -1;
    }
 
    if (SetCookie(email, resp_json) < 0) {
        LOG_ERROR << "SetCookie failed";
        encodeLoginJson(api_error_id::bad_request, "set cookie failed", resp_json);
        return -1;
    }
 
    return 0;
}
