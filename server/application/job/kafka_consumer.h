#pragma once

#include <string>
#include <librdkafka/rdkafkacpp.h>
#include <memory>

class KafkaConsumer {
public:
    // 构造函数只初始化基本参数
    KafkaConsumer(const std::string& brokers, const std::string& topic, 
                 const std::string& group_id = "my_consumer_group");
    ~KafkaConsumer();

    // 初始化连接
    bool init();
    
    // 订阅主题
    bool subscribe();
    
    // 消费消息，返回消息内容
    std::string consume(int timeout_ms = 1000);

    // 检查连接状态
    bool is_connected() const { return consumer_ != nullptr; }

private:
    std::string brokers_;
    std::string topic_;
    std::string group_id_;
    bool initialized_{false};
    std::unique_ptr<RdKafka::KafkaConsumer> consumer_;
    std::unique_ptr<RdKafka::Conf> conf_;
};