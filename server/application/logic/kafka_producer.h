#pragma once

#include <librdkafka/rdkafkacpp.h>
#include <string>
#include <memory>
#include <iostream>

class KafkaProducer {
public:
    KafkaProducer();
    ~KafkaProducer();

    // 初始化并连接到Kafka
    bool init(const std::string& brokers, const std::string& topic);
    
    // 发送消息到Kafka
    bool sendMessage(const std::string& message);
    
    // 关闭连接
    void close();

private:
    static void deliveryCallback(RdKafka::Message& message, void* opaque);

    std::string topic_name_;
    RdKafka::Producer* producer_;
    RdKafka::Topic* topic_;
    bool is_initialized_;
}; 