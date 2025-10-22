
#ifndef __PUB_SUB_SERVICE_H__
#define __PUB_SUB_SERVICE_H__
#include <unordered_set>
 

//房间主题
class RoomTopic
{
public:
    //拥有者后续可以删除该主题
    RoomTopic(string room_topic, int32_t owner_user_id):
        room_topic_(room_topic), owner_user_id_(owner_user_id) {
    }
    void AddSubscriber(int32_t user_id) {
        user_ids_.insert(user_id);  //增加订阅者
    }
    void DeleteSubscriber(int32_t user_id) {
        user_ids_.erase(user_id);  //删除订阅者
    }
    // 获取所有订阅者
    std::unordered_set<int32_t> &getSubscribers() {
        return user_ids_;
    }
    std::unordered_set<int32_t> user_ids_;
    string room_topic_;
    int32_t owner_user_id_;
};
using RoomTopicPtr = std::shared_ptr<RoomTopic>;


using PubSubCallback = std::function<void(std::unordered_set<int32_t>&)>;
// This is an interface to reduce compile times.
class PubSubService
{
public:
    // 获取单例实例的静态方法
    static PubSubService& GetInstance() {
        static PubSubService instance; // 局部静态变量，线程安全（C++11 起）
        return instance;
    }

    virtual ~PubSubService() {}
    bool AddRoomTopic(string room_topic, int32_t owner_user_id) {
        //创建主题
        RoomTopicPtr room_topic_ptr = std::make_shared<RoomTopic>(room_topic, owner_user_id);
        // 需要考虑判断主题是不是已经存在了
        topic_ids.insert({ room_topic, room_topic_ptr});
        return true;
    }
    //删除主题时需要本人，或者管理员
    bool DeleteRoomTopic(string room_topic, int32_t owner_user_id) {
        //删除主题
        topic_ids.erase(room_topic);
        return true;
    }
    // 增加订阅者
    bool AddSubscriber(string room_topic, int32_t user_id) {
        // 先找到主题
        RoomTopicPtr &room_topic_ptr =  topic_ids[room_topic];
        if(room_topic_ptr) {
            room_topic_ptr->AddSubscriber(user_id);
            return true;
        } else {
            return false;
        }
    }
    bool DeleteSubscriber(string room_topic, int32_t user_id) {
        // 先找到主题
        RoomTopicPtr &room_topic_ptr =  topic_ids[room_topic];
        if(room_topic_ptr) {
            room_topic_ptr->DeleteSubscriber(user_id);
            return true;
        } else {
            return false;
        }
    }

    //发送主题消息
    bool PubSubPublish(string room_topic, PubSubCallback callback) {
        //获取对应主题的用户id
        RoomTopicPtr &room_topic_ptr =  topic_ids[room_topic];
        if(room_topic_ptr) {
            std::unordered_set<int32_t> user_ids = room_topic_ptr->getSubscribers();
            //然后调用回调函数
            callback(user_ids);
            return true;
        } else {
            return  false;
        }
    }

     std::map<std::string, RoomTopicPtr> topic_ids;

};


#endif
