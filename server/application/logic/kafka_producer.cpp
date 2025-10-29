#include "kafka_producer.h"
#include "muduo/base/Logging.h"
KafkaProducer::KafkaProducer() 
    : producer_(nullptr)
    , topic_(nullptr)
    , is_initialized_(false) {
}

KafkaProducer::~KafkaProducer() {
    close();
}

bool KafkaProducer::init(const std::string& brokers, const std::string& topic) {
    if (is_initialized_) {
        return true;
    }

    // 创建Kafka配置
    std::string errstr;
    RdKafka::Conf* conf = RdKafka::Conf::create(RdKafka::Conf::CONF_GLOBAL);
    
    // 设置Kafka服务器地址
    if (conf->set("bootstrap.servers", brokers, errstr) != RdKafka::Conf::CONF_OK) {
        std::cerr << "Failed to set bootstrap.servers: " << errstr << std::endl;
        delete conf;
        return false;
    }

    // 创建producer实例
    producer_ = RdKafka::Producer::create(conf, errstr);
    delete conf;

    if (!producer_) {
        std::cerr << "Failed to create producer: " << errstr << std::endl;
        return false;
    }

    // 创建topic配置
    RdKafka::Conf* tconf = RdKafka::Conf::create(RdKafka::Conf::CONF_TOPIC);
    
    // 创建topic
    topic_ = RdKafka::Topic::create(producer_, topic, tconf, errstr);
    delete tconf;

    if (!topic_) {
        std::cerr << "Failed to create topic: " << errstr << std::endl;
        delete producer_;
        producer_ = nullptr;
        return false;
    }

    topic_name_ = topic;
    is_initialized_ = true;
    LOG_INFO << "Kafka producer initialized successfully";
    return true;
}

bool KafkaProducer::sendMessage(const std::string& message) {
    LOG_INFO << "Sending message to Kafka: " << message;
    if (!is_initialized_) {
        std::cerr << "Kafka producer not initialized" << std::endl;
        return false;
    }
    LOG_INFO << "producer_->produce"; 
    RdKafka::ErrorCode resp = producer_->produce(
        topic_,
        RdKafka::Topic::PARTITION_UA,  // 使用自动分区分配
        RdKafka::Producer::RK_MSG_COPY,  // 复制消息
        const_cast<char*>(message.c_str()),
        message.size(),
        nullptr,  // 可选key
        nullptr   // 消息opaque
    );
    LOG_INFO << "producer_->produce done";
    if (resp != RdKafka::ERR_NO_ERROR) {
        std::cerr << "Failed to produce message: " 
                  << RdKafka::err2str(resp) << std::endl;
        return false;
    }

    // 确保消息被发送
    producer_->poll(0);
    LOG_INFO << "producer_->poll done";
    return true;
}

void KafkaProducer::close() {
    if (topic_) {
        delete topic_;
        topic_ = nullptr;
    }
    
    if (producer_) {
        // 等待所有消息发送完成
        while (producer_->outq_len() > 0) {
            producer_->poll(100);
        }
        delete producer_;
        producer_ = nullptr;
    }
    
    is_initialized_ = false;
}

void KafkaProducer::deliveryCallback(RdKafka::Message& message, void* opaque) {
    if (message.err()) {
        std::cerr << "Message delivery failed: " << message.errstr() << std::endl;
    }
} 