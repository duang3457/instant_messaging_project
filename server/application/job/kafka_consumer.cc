#include "kafka_consumer.h"
#include <iostream>
#include <stdexcept>

KafkaConsumer::KafkaConsumer(const std::string& brokers, const std::string& topic, 
                           const std::string& group_id)
    : brokers_(brokers), topic_(topic), group_id_(group_id) {
}

bool KafkaConsumer::init() {
    if (initialized_) {
        return true;
    }

    try {
        std::string errstr;
        // 创建配置对象
        conf_ = std::unique_ptr<RdKafka::Conf>(
            RdKafka::Conf::create(RdKafka::Conf::CONF_GLOBAL));
        
        // 设置基本配置
        if (conf_->set("bootstrap.servers", brokers_, errstr) != RdKafka::Conf::CONF_OK ||
            conf_->set("group.id", group_id_, errstr) != RdKafka::Conf::CONF_OK ||
            conf_->set("auto.offset.reset", "earliest", errstr) != RdKafka::Conf::CONF_OK) {
            std::cerr << "Configuration error: " << errstr << std::endl;
            return false;
        }
        
        // 创建消费者实例
        consumer_ = std::unique_ptr<RdKafka::KafkaConsumer>(
            RdKafka::KafkaConsumer::create(conf_.get(), errstr));
        
        if (!consumer_) {
            std::cerr << "Failed to create consumer: " << errstr << std::endl;
            return false;
        }

        initialized_ = true;
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Initialization error: " << e.what() << std::endl;
        return false;
    }
}

bool KafkaConsumer::subscribe() {
    if (!initialized_) {
        std::cerr << "Consumer not initialized. Call init() first." << std::endl;
        return false;
    }

    std::vector<std::string> topics = {topic_};
    RdKafka::ErrorCode err = consumer_->subscribe(topics);
    
    if (err != RdKafka::ERR_NO_ERROR) {
        std::cerr << "Failed to subscribe to topic: " << 
            std::string(RdKafka::err2str(err)) << std::endl;
        return false;
    }
    return true;
}

std::string KafkaConsumer::consume(int timeout_ms) {
    std::string message_payload;
    
    // consume内部用epoll/poll，阻塞操作
    std::unique_ptr<RdKafka::Message> msg(
        consumer_->consume(timeout_ms));
    
    switch (msg->err()) {
        case RdKafka::ERR_NO_ERROR:
            // 成功接收到消息
            if (msg->payload()) {
                message_payload = std::string(
                    static_cast<const char*>(msg->payload()), 
                    msg->len());
            }
            break;
            
        case RdKafka::ERR__TIMED_OUT:
            // 超时，没有新消息
            break;
            
        case RdKafka::ERR__PARTITION_EOF:
            // 到达分区末尾
            break;
            
        default:
            // 处理其他错误
            std::cerr << "Consume error: " 
                     << msg->errstr() << std::endl;
            break;
    }
    return message_payload;
}

KafkaConsumer::~KafkaConsumer() {
    if (consumer_) {
        consumer_->close();
    }
}