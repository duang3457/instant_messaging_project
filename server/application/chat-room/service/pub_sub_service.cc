#include "pub_sub_service.h"
#include "api_types.h"

static std::vector<Room> s_room_list = {
    {"0001", "官方1群", 1, "", "", ""},
    {"0002", "官方2群", 2, "", "", ""},
    {"0003", "官方3群", 3, "", "", ""},
    {"0004", "官方4群", 4, "", "", ""},
    {"0005", "官方5群", 5, "", "", ""}
};

static std::mutex s_metux_room_list;

 std::vector<Room> &PubSubService::GetRoomList() {
    //加锁
    std::lock_guard<std::mutex> lock(s_metux_room_list);
    return s_room_list;
 }