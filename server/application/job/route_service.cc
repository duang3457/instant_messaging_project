#include "route_service.h"
#include <cstring>

RouteService::RouteService() 
    : redis_context_(nullptr)
    , redis_port_(6379) {
}

RouteService::~RouteService() {
    Close();
}

bool RouteService::Init(const string& redis_host, int redis_port, const string& redis_password) {
    redis_host_ = redis_host;
    redis_port_ = redis_port;
    redis_password_ = redis_password;

    return Reconnect();
}

void RouteService::Close() {
    if (redis_context_) {
        redisFree(redis_context_);
        redis_context_ = nullptr;
    }
}

bool RouteService::Reconnect() {
    if (redis_context_) {
        redisFree(redis_context_);
        redis_context_ = nullptr;
    }

    // 连接Redis，设置超时时间
    struct timeval timeout = { 2, 0 }; // 2秒超时
    redis_context_ = redisConnectWithTimeout(redis_host_.c_str(), redis_port_, timeout);

    if (!redis_context_ || redis_context_->err) {
        if (redis_context_) {
            LOG_ERROR << "Redis connection error: " << redis_context_->errstr;
            redisFree(redis_context_);
            redis_context_ = nullptr;
        } else {
            LOG_ERROR << "Redis connection error: can't allocate redis context";
        }
        return false;
    }

    // 如果有密码，进行认证
    if (!redis_password_.empty()) {
        redisReply* reply = (redisReply*)redisCommand(redis_context_, "AUTH %s", redis_password_.c_str());
        if (!reply || reply->type == REDIS_REPLY_ERROR) {
            LOG_ERROR << "Redis AUTH failed";
            if (reply) freeReplyObject(reply);
            Close();
            return false;
        }
        freeReplyObject(reply);
    }

    LOG_INFO << "Connected to Redis: " << redis_host_ << ":" << redis_port_;
    return true;
}

bool RouteService::CheckConnection() {
    if (!redis_context_) {
        LOG_WARN << "Redis not connected, attempting reconnect...";
        return Reconnect();
    }

    // 发送 PING 测试连接
    redisReply* reply = (redisReply*)redisCommand(redis_context_, "PING");
    if (!reply || reply->type == REDIS_REPLY_ERROR) {
        LOG_WARN << "Redis connection lost, reconnecting...";
        if (reply) freeReplyObject(reply);
        return Reconnect();
    }
    freeReplyObject(reply);
    return true;
}

bool RouteService::GetRoomConnections(const string& room_id, vector<string>& conn_ids) {
    if (!CheckConnection()) {
        return false;
    }

    conn_ids.clear();

    // SMEMBERS room:connections:{room_id}
    string key = "room:connections:" + room_id;
    redisReply* reply = (redisReply*)redisCommand(redis_context_, "SMEMBERS %s", key.c_str());

    if (!reply) {
        LOG_ERROR << "Redis command failed: SMEMBERS " << key;
        return false;
    }

    if (reply->type == REDIS_REPLY_ARRAY) {
        for (size_t i = 0; i < reply->elements; i++) {
            if (reply->element[i]->type == REDIS_REPLY_STRING) {
                conn_ids.push_back(reply->element[i]->str);
            }
        }
        LOG_INFO << "Room " << room_id << " has " << conn_ids.size() << " connections";
    } else {
        LOG_WARN << "No connections found for room: " << room_id;
    }

    freeReplyObject(reply);
    return true;
}

string RouteService::GetConnectionCometAddr(const string& conn_id) {
    if (!CheckConnection()) {
        return "";
    }

    // HGET connection:info:{conn_id} comet_id
    string key = "connection:info:" + conn_id;
    redisReply* reply = (redisReply*)redisCommand(redis_context_, "HGET %s comet_id", key.c_str());

    if (!reply) {
        LOG_ERROR << "Redis command failed: HGET " << key << " comet_id";
        return "";
    }

    string comet_addr;
    if (reply->type == REDIS_REPLY_STRING) {
        comet_addr = reply->str;
    }

    freeReplyObject(reply);
    return comet_addr;
}

bool RouteService::GetConnectionInfoBatch(const vector<string>& conn_ids, 
                                          vector<ConnectionInfo>& conn_infos) {
    if (!CheckConnection()) {
        return false;
    }

    conn_infos.clear();

    for (const auto& conn_id : conn_ids) {
        // HGETALL connection:info:{conn_id}
        string key = "connection:info:" + conn_id;
        redisReply* reply = (redisReply*)redisCommand(redis_context_, "HGETALL %s", key.c_str());

        if (!reply) {
            LOG_ERROR << "Redis command failed: HGETALL " << key;
            continue;
        }

        if (reply->type == REDIS_REPLY_ARRAY && reply->elements > 0) {
            ConnectionInfo info;
            info.conn_id = conn_id;

            // 解析 Hash 字段
            for (size_t i = 0; i < reply->elements; i += 2) {
                if (i + 1 >= reply->elements) break;

                string field = reply->element[i]->str;
                string value = reply->element[i + 1]->str;

                if (field == "comet_id") {
                    info.comet_addr = value;
                } else if (field == "user_id") {
                    info.user_id = value;
                } else if (field == "room_id") {
                    info.room_id = value;
                }
            }

            if (!info.comet_addr.empty()) {
                conn_infos.push_back(info);
            }
        }

        freeReplyObject(reply);
    }

    return !conn_infos.empty();
}

bool RouteService::GroupConnectionsByComet(const vector<string>& conn_ids, 
                                           map<string, vector<string>>& comet_groups) {
    if (!CheckConnection()) {
        return false;
    }

    comet_groups.clear();

    // 批量查询连接信息并分组
    vector<ConnectionInfo> conn_infos;
    if (!GetConnectionInfoBatch(conn_ids, conn_infos)) {
        LOG_WARN << "Failed to get connection info for grouping";
        return false;
    }

    // 按 comet_addr 分组
    for (const auto& info : conn_infos) {
        comet_groups[info.comet_addr].push_back(info.conn_id);
    }

    LOG_INFO << "Grouped " << conn_ids.size() << " connections into " 
             << comet_groups.size() << " comet nodes";

    return !comet_groups.empty();
}

bool RouteService::IsUserOnline(const string& user_id) {
    if (!CheckConnection()) {
        return false;
    }

    // EXISTS user:online:{user_id}
    string key = "user:online:" + user_id;
    redisReply* reply = (redisReply*)redisCommand(redis_context_, "EXISTS %s", key.c_str());

    if (!reply) {
        LOG_ERROR << "Redis command failed: EXISTS " << key;
        return false;
    }

    bool online = (reply->type == REDIS_REPLY_INTEGER && reply->integer == 1);
    freeReplyObject(reply);

    return online;
}

string RouteService::GetUserConnectionId(const string& user_id) {
    if (!CheckConnection()) {
        return "";
    }

    // GET user:online:{user_id}
    string key = "user:online:" + user_id;
    redisReply* reply = (redisReply*)redisCommand(redis_context_, "GET %s", key.c_str());

    if (!reply) {
        LOG_ERROR << "Redis command failed: GET " << key;
        return "";
    }

    string conn_id;
    if (reply->type == REDIS_REPLY_STRING) {
        conn_id = reply->str;
    }

    freeReplyObject(reply);
    return conn_id;
}

// ==================== 分布式锁实现 ====================

bool RouteService::AcquireLock(const string& lock_key, const string& lock_value, int ttl_seconds) {
    if (!CheckConnection()) {
        return false;
    }

    // 使用 SET key value NX EX ttl 原子操作
    // NX: 仅在 key 不存在时设置
    // EX: 设置过期时间（秒）
    redisReply* reply = (redisReply*)redisCommand(redis_context_, 
        "SET %s %s NX EX %d", lock_key.c_str(), lock_value.c_str(), ttl_seconds);

    if (!reply) {
        LOG_ERROR << "Redis command failed: SET " << lock_key << " NX EX " << ttl_seconds;
        return false;
    }

    bool acquired = false;
    if (reply->type == REDIS_REPLY_STATUS && strcmp(reply->str, "OK") == 0) {
        acquired = true;
        LOG_DEBUG << "Lock acquired: " << lock_key << " (TTL: " << ttl_seconds << "s)";
    } else {
        LOG_DEBUG << "Lock already held: " << lock_key;
    }

    freeReplyObject(reply);
    return acquired;
}

bool RouteService::ReleaseLock(const string& lock_key, const string& lock_value) {
    if (!CheckConnection()) {
        return false;
    }

    // Lua 脚本：确保只有持有锁的客户端才能释放（避免误删）
    // 原子操作：先检查值是否匹配，再删除
    const char* lua_script = 
        "if redis.call('get', KEYS[1]) == ARGV[1] then "
        "    return redis.call('del', KEYS[1]) "
        "else "
        "    return 0 "
        "end";

    vector<string> keys = {lock_key};
    vector<string> args = {lock_value};
    
    long long result = 0;
    bool success = ExecuteLuaScript(lua_script, keys, args, &result);
    
    if (success && result == 1) {
        LOG_DEBUG << "Lock released: " << lock_key;
        return true;
    } else {
        LOG_WARN << "Failed to release lock: " << lock_key << " (may have expired or value mismatch)";
        return false;
    }
}

// ==================== 消息去重实现 ====================

bool RouteService::CheckAndMarkMessageProcessed(const string& msg_id, const string& room_id, int ttl_seconds) {
    if (!CheckConnection()) {
        return false; // 连接失败，保守处理：认为未处理
    }

    // 使用复合 key: msg:processed:{room_id}:{msg_id}
    string dedup_key = "msg:processed:" + room_id + ":" + msg_id;

    // 尝试设置（NX：仅在不存在时设置）
    redisReply* reply = (redisReply*)redisCommand(redis_context_, 
        "SET %s 1 NX EX %d", dedup_key.c_str(), ttl_seconds);

    if (!reply) {
        LOG_ERROR << "Redis command failed: SET " << dedup_key << " NX EX " << ttl_seconds;
        return false; // 连接失败，保守处理
    }

    bool is_new_message = false;
    if (reply->type == REDIS_REPLY_STATUS && strcmp(reply->str, "OK") == 0) {
        // 成功设置，说明是新消息（未处理过）
        is_new_message = true;
        LOG_DEBUG << "New message marked as processed: " << msg_id << " (TTL: " << ttl_seconds << "s)";
    } else {
        // 设置失败，说明消息已处理过（重复消息）
        is_new_message = false;
        LOG_WARN << "Duplicate message detected: " << msg_id << " in room " << room_id;
    }

    freeReplyObject(reply);
    return is_new_message; // true=未处理过（应该继续处理），false=已处理过（应该跳过）
}

bool RouteService::IsRoomInCooldown(const string& room_id, int cooldown_seconds) {
    if (!CheckConnection()) {
        return false; // 连接失败，允许继续（避免阻塞）
    }

    // 使用 key: room:cooldown:{room_id}
    string cooldown_key = "room:cooldown:" + room_id;

    // 尝试设置冷却期
    redisReply* reply = (redisReply*)redisCommand(redis_context_, 
        "SET %s 1 NX EX %d", cooldown_key.c_str(), cooldown_seconds);

    if (!reply) {
        LOG_ERROR << "Redis command failed: SET " << cooldown_key << " NX EX " << cooldown_seconds;
        return false; // 连接失败，允许继续
    }

    bool in_cooldown = false;
    if (reply->type == REDIS_REPLY_STATUS && strcmp(reply->str, "OK") == 0) {
        // 成功设置，说明不在冷却期（允许广播）
        in_cooldown = false;
        LOG_DEBUG << "Room " << room_id << " cooldown started (" << cooldown_seconds << "s)";
    } else {
        // 设置失败，说明在冷却期内（跳过广播）
        in_cooldown = true;
        LOG_DEBUG << "Room " << room_id << " is in cooldown period, skipping broadcast";
    }

    freeReplyObject(reply);
    return in_cooldown; // true=在冷却期（应该跳过），false=不在冷却期（可以广播）
}

// ==================== Lua 脚本执行 ====================

bool RouteService::ExecuteLuaScript(const string& script, const vector<string>& keys, 
                                     const vector<string>& args, long long* result) {
    if (!CheckConnection()) {
        return false;
    }

    // 构建 EVAL 命令参数
    // EVAL script numkeys key1 key2 ... arg1 arg2 ...
    vector<const char*> argv;
    vector<size_t> argvlen;

    // 命令名
    argv.push_back("EVAL");
    argvlen.push_back(4);

    // 脚本
    argv.push_back(script.c_str());
    argvlen.push_back(script.length());

    // key 数量
    string numkeys = std::to_string(keys.size());
    argv.push_back(numkeys.c_str());
    argvlen.push_back(numkeys.length());

    // keys
    for (const auto& key : keys) {
        argv.push_back(key.c_str());
        argvlen.push_back(key.length());
    }

    // args
    for (const auto& arg : args) {
        argv.push_back(arg.c_str());
        argvlen.push_back(arg.length());
    }

    // 执行命令
    redisReply* reply = (redisReply*)redisCommandArgv(redis_context_, 
        argv.size(), &argv[0], &argvlen[0]);

    if (!reply) {
        LOG_ERROR << "Lua script execution failed";
        return false;
    }

    bool success = false;
    if (reply->type == REDIS_REPLY_INTEGER) {
        if (result) {
            *result = reply->integer;
        }
        success = true;
    } else if (reply->type == REDIS_REPLY_STATUS || reply->type == REDIS_REPLY_STRING) {
        success = true;
    } else {
        LOG_ERROR << "Unexpected Lua script reply type: " << reply->type;
    }

    freeReplyObject(reply);
    return success;
}
