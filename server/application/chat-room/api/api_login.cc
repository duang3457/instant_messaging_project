#include "api_login.h"

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


int verifyUserPassword(string &email, string &password) {
    int ret = -1;
    // 只能使用email
    CDBManager *db_manager = CDBManager::getInstance();
    CDBConn *db_conn = db_manager->GetDBConn("chatroom_slave");
    AUTO_REL_DBCONN(db_manager, db_conn);   //析构时自动归还连接
    if(!db_conn) {
        LOG_ERROR << "get db conn failed";
        return -1;
    }

    // 打印当前数据库的所有表
    LOG_INFO << "=== 查询数据库表信息 ===";
    string showTablesSQL = "SHOW TABLES";
    CResultSet *tables_result = db_conn->ExecuteQuery(showTablesSQL.c_str());
    if (tables_result) {
        LOG_INFO << "当前数据库中的表：";
        while (tables_result->Next()) {
            string table_name = tables_result->GetString(0);  // SHOW TABLES 返回的第一列是表名
            LOG_INFO << "  - " << table_name;
        }
        delete tables_result;
    } else {
        LOG_ERROR << "查询数据库表失败";
    }
    LOG_INFO << "=== 表信息查询完成 ===";

    //根据email查询密码
    string strSql = FormatString("select username, password_hash, salt from users where email='%s'", email.c_str());
    LOG_INFO << "执行:" << strSql;
    CResultSet *result_set = db_conn->ExecuteQuery(strSql.c_str());
    if (result_set && result_set->Next()) { //如果存在则读取密码 
        string username = result_set->GetString("username");
        string db_password_hash = result_set->GetString("password_hash");
        string salt = result_set->GetString("salt");
        MD5 md5(password + salt);  // 计算出新的密码
        string client_password_hash = md5.toString();  //  计算出新的密码
        if(db_password_hash == client_password_hash) {
            LOG_INFO << "username: " << username << " verify ok";
            ret = 0;
        } else {
            LOG_INFO << "username: " << username << " verify failed";
            ret = -1;
        }
    } else {
        ret = -1;
    }

    if(result_set)
        delete result_set;

    return ret;
}

int ApiUserLogin(std::string &post_data, std::string &resp_data){
    string email;
    string password;

    // json反序列化
      // 解析json
    if (decodeLoginJson(post_data, email, password) < 0) {
        LOG_ERROR << "decodeRegisterJson failed";
        encodeLoginJson(api_error_id::bad_request, "email or password no fill", resp_data);
        return -1;
    }

    //校验邮箱  密码
    int ret = verifyUserPassword(email, password);
    if(ret == 0) {
        // 设置cookie
        ApiSetCookie(email, resp_data);
    } else {
        encodeLoginJson(api_error_id::bad_request, "email password no match", resp_data);
    }
    return ret;
}
