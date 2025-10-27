#include "api_register.h"
#include "api_common.h"
#include <iostream>

#include <jsoncpp/json/json.h>
#include <muduo/base/Logging.h> // Logger日志头文件

using namespace std;

void encdoeRegisterJson(api_error_id input, string message, string &str_json) {
    Json::Value root;
    root["id"] = api_error_id_to_string(input);
    root["message"] = message;
    Json::FastWriter writer;
    str_json = writer.write(root);
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
int registerUser(string &username, string &email, string &password, api_error_id &error_id) {
    int ret = -1;
    CDBManager *db_manager = CDBManager::getInstance();
    CDBConn *db_conn = db_manager->GetDBConn("chatroom_master");
    AUTO_REL_DBCONN(db_manager, db_conn);
    if(!db_conn) {
        LOG_ERROR << "GetDBConn(chatroom_master) failed" ;
        return 1;
    }
    // 先查询 用户名  邮箱是否存在 如果存在就报错
    string str_sql = FormatString("select id, username, email from users where username='%s' or email='%s' ", username.c_str(), email.c_str());

    CResultSet *result_set = db_conn->ExecuteQuery(str_sql.c_str());
    if(result_set && result_set->Next()) {
        if(result_set->GetString("username")) {
            if(result_set->GetString("username") == username)  {
                error_id = api_error_id::username_exists;
                LOG_WARN << "id: " << result_set->GetInt("id") << ", username: " <<  username <<  "  已经存在";
            }
           
        }

        if(result_set->GetString("email")) {
            if(result_set->GetString("email") == email) {
                error_id = api_error_id::email_exists;
                LOG_WARN << "id: " << result_set->GetInt("id") << ", email: " <<  email <<  "  已经存在";
            }
        }
        delete result_set;
        return -1;
    }

    // 注册账号
    // 随机数生成盐值
    string salt = RandomString(16);  //随机混淆码, 混淆码对字符串唯一性要求没有那么高
    MD5 md5(password+salt);
    string password_hash = md5.toString();
    LOG_INFO << "salt: " << salt;

    //插入语句
    str_sql = "insert into users  (`username`,`email`,`password_hash`,`salt`) values(?,?,?,?)";
    LOG_INFO << "执行: " <<  str_sql;
    // 预处理方式写入数据
    CPrepareStatement *stmt = new CPrepareStatement();
    if (stmt->Init(db_conn->GetMysql(), str_sql)) {
        uint32_t index = 0;
        stmt->SetParam(index++, username);
        stmt->SetParam(index++, email);
        stmt->SetParam(index++, password_hash);
        stmt->SetParam(index++, salt);
        bool bRet = stmt->ExecuteUpdate(); //真正提交要写入的数据
        if (bRet) {     //提交正常返回 true
            ret = 0;
            LOG_INFO << "insert user_id: " <<  db_conn->GetInsertId() <<  ", username: " <<  username ;
        } else {
            LOG_ERROR << "insert users failed. " <<  str_sql;
            ret = 1;
        }
    }
    delete stmt;

    return ret;
}

int ApiRegisterUser(string &post_data, string &resp_json) {
    int ret = 0;
    string username;
    string email;
    string password;
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
        encdoeRegisterJson(api_error_id::bad_request, "请求参数不全", resp_json);
        return -1;
    }
 
    // 注册账号
    // 先在数据库查询用户名 昵称 是否存在 如果不存在才去注册
    api_error_id error_id = api_error_id::bad_request;
    ret = registerUser(username, email, password, error_id);
    if(ret != 0) {
        encdoeRegisterJson(api_error_id::username_exists, "", resp_json);
    }
    else {
        // 注册成功，设置token
        ApiSetCookie(email, resp_json); //获取token  
    }

    return ret;
}