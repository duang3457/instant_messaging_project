#include "api_register.h"
#include <iostream>

#include <jsoncpp/json/json.h>
#include "muduo/base/Logging.h" // Logger日志头文件

using namespace std;

int encdoeRegisterJson(api_error_id input, string message, string &str_json) {
    Json::Value root;
    root["id"] = api_error_id_to_string(input);
    root["message"] = message;
    Json::FastWriter writer;
    str_json = writer.write(root);
    return 0;
}
// post_data, username, email, password
int decodeRegisterJson(const std::string &str_json, string &username,
                       string &email, string &password)
{   
    bool res;
    Json::Value root;
    Json::Reader jsonReader;
    res = jsonReader.parse(str_json, root);    
    if (!res) {
        LOG_ERROR << "parse reg json failed ";
        return -1;
    }
    // 用户名
    if (root["username"].isNull()) {
        LOG_ERROR << "username null";
        return -1;
    }
    username = root["username"].asString();

   //邮箱  
    if (root["email"].isNull()) {
        LOG_WARN << "email null";
    } else {
        email = root["email"].asString();
    }

    //密码
    if (root["password"].isNull()) {
        LOG_ERROR << "password null";
        return -1;
    }
    password = root["password"].asString();

    return 0;
}
/*
* 先根据用户名查询数据库该用户是否存在，不存在才插入
*/
int registerUser(string &username, string &email, string &password) {
    int ret = 0; //ret = 2 用户已经存在  = 1注册异常  =0注册成功
    uint32_t user_id = 0;
    CDBManager *db_manager = CDBManager::getInstance();
    CDBConn *db_conn = db_manager->GetDBConn("chatroom_master");
    AUTO_REL_DBCONN(db_manager, db_conn);
    if(!db_conn) {
        LOG_ERROR << "GetDBConn(chatroom_master) failed" ;
        return 1;
    }
    //查询数据库是否存在
    string str_sql = FormatString("select id from users where username='%s'", username.c_str());
    CResultSet *result_set = db_conn->ExecuteQuery(str_sql.c_str());
    if(result_set && result_set->Next()) {
        LOG_WARN << "id: " << result_set->GetInt("id") << ", username: " <<  username <<  "  已经存在";
        delete result_set;
        ret = 2; //已经存在对应的用户名
    } else {
        str_sql = "insert into users  (`username`,`email`,`password`) values(?,?,?)";
        LOG_INFO << "执行: " <<  str_sql;
         // 预处理方式写入数据
        CPrepareStatement *stmt = new CPrepareStatement();
        if (stmt->Init(db_conn->GetMysql(), str_sql)) {
            uint32_t index = 0;
            stmt->SetParam(index++, username);
            stmt->SetParam(index++, email);
            stmt->SetParam(index++, password);
            bool bRet = stmt->ExecuteUpdate(); //真正提交要写入的数据
            if (bRet) {     //提交正常返回 true
                ret = 0;
                user_id = db_conn->GetInsertId();   
                LOG_INFO << "insert user_id: " <<  user_id <<  ", username: " <<  username ;
            } else {
                LOG_ERROR << "insert users failed. " <<  str_sql;
                ret = 1;
            }
        }
        delete stmt;
    }

    return ret;
}

int ApiRegisterUser(string &post_data, string &resp_json) {

    int ret = 0;
    string username;
    string email;
    string password;
 
    LOG_INFO << "post_data: " <<  post_data;
    
    // 判断数据是否为空
    if (post_data.empty()) {
        LOG_ERROR << "decodeRegisterJson failed";
        //序列化 把结果返回给客户端
        // code = 1
        encdoeRegisterJson(api_error_id::bad_request, "", resp_json);
        return -1;
    }
    // json反序列化
    ret = decodeRegisterJson(post_data, username, email, password);
    if(ret < 0) {
        encdoeRegisterJson(api_error_id::bad_request, "", resp_json);
        return -1;
    }
 
    // 注册账号
    // 先在数据库查询用户名 昵称 是否存在 如果不存在才去注册
    ret = registerUser(username, email, password);  
    if(ret != 0) {
        encdoeRegisterJson(api_error_id::username_exists, "", resp_json);
    }
    else {
        //设置token
        SetCookie(email, resp_json); //获取token  
    }

    return ret;
}