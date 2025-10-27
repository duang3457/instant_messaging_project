#include "pub_sub_service.h"
#include "api_types.h"

static std::vector<Room> s_room_list = {
    {"0001", "程序员老廖2", 1, "", "", ""},
    {"0002", "码农mark2", 2, "", "", ""},
    {"0003", "程序员yt2", 3, "", "", ""},
    {"0004", "老周讲golang2", 4, "", "", ""},
    {"0005", "绝顶哥编程vico2", 5, "", "", ""}
};

static std::mutex s_metux_room_list;

 std::vector<Room> &PubSubService::GetRoomList() {
    //加锁
    std::lock_guard<std::mutex> lock(s_metux_room_list);
    return s_room_list;
 }