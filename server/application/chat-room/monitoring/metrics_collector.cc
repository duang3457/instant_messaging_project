#include "metrics_collector.h"
#include <muduo/base/Logging.h>
#include <chrono>

using namespace prometheus;

// ==================== 单例实现 ====================

MetricsCollector& MetricsCollector::GetInstance() {
    static MetricsCollector instance;
    return instance;
}

MetricsCollector::MetricsCollector() 
    : registry_(std::make_shared<Registry>()),
      exposer_(nullptr),
      service_name_("unknown"),
      request_total_family_(nullptr),
      latency_family_(nullptr),
      active_connections_gauge_(nullptr),
      error_total_family_(nullptr),
      kafka_produced_family_(nullptr),
      kafka_consumed_family_(nullptr),
      grpc_calls_family_(nullptr),
      websocket_push_family_(nullptr),
      redis_ops_family_(nullptr) {
}

MetricsCollector::~MetricsCollector() {
    Shutdown();
}

void MetricsCollector::Initialize(const std::string& bind_address, const std::string& service_name) {
    service_name_ = service_name;
    
    LOG_INFO << "Initializing MetricsCollector for service: " << service_name 
             << " on " << bind_address;

    // 创建 Exposer（HTTP 服务器）
    try {
        exposer_ = std::make_unique<Exposer>(bind_address);
        exposer_->RegisterCollectable(registry_);
        LOG_INFO << "Metrics endpoint started at http://" << bind_address << "/metrics";
    } catch (const std::exception& e) {
        LOG_ERROR << "Failed to start metrics exposer: " << e.what();
        return;
    }

    // ==================== 初始化指标族 ====================

    // 1. QPS: 请求总数（Counter）
    request_total_family_ = &BuildCounter()
        .Name("http_requests_total")
        .Help("Total number of HTTP requests")
        .Labels({{"service", service_name_}})
        .Register(*registry_);

    // 2. 延迟: 请求处理时间（Histogram）
    // 桶: 100us, 500us, 1ms, 5ms, 10ms, 50ms, 100ms, 500ms, 1s, 5s, 10s
    latency_family_ = &BuildHistogram()
        .Name("http_request_duration_microseconds")
        .Help("HTTP request latency in microseconds")
        .Labels({{"service", service_name_}})
        .Register(*registry_);

    // 3. 连接数: 当前活跃连接（Gauge）
    auto& connections_family = BuildGauge()
        .Name("active_connections")
        .Help("Number of active connections")
        .Labels({{"service", service_name_}})
        .Register(*registry_);
    active_connections_gauge_ = &connections_family.Add({{"type", "websocket"}});

    // 4. 错误: 错误总数（Counter）
    error_total_family_ = &BuildCounter()
        .Name("errors_total")
        .Help("Total number of errors")
        .Labels({{"service", service_name_}})
        .Register(*registry_);

    // 5. Kafka 指标
    kafka_produced_family_ = &BuildCounter()
        .Name("kafka_messages_produced_total")
        .Help("Total number of Kafka messages produced")
        .Labels({{"service", service_name_}})
        .Register(*registry_);

    kafka_consumed_family_ = &BuildCounter()
        .Name("kafka_messages_consumed_total")
        .Help("Total number of Kafka messages consumed")
        .Labels({{"service", service_name_}})
        .Register(*registry_);

    // 6. gRPC 指标
    grpc_calls_family_ = &BuildCounter()
        .Name("grpc_calls_total")
        .Help("Total number of gRPC calls")
        .Labels({{"service", service_name_}})
        .Register(*registry_);

    // 7. WebSocket 推送指标
    websocket_push_family_ = &BuildCounter()
        .Name("websocket_messages_pushed_total")
        .Help("Total number of WebSocket messages pushed")
        .Labels({{"service", service_name_}})
        .Register(*registry_);

    // 8. Redis 指标
    redis_ops_family_ = &BuildCounter()
        .Name("redis_operations_total")
        .Help("Total number of Redis operations")
        .Labels({{"service", service_name_}})
        .Register(*registry_);

    LOG_INFO << "MetricsCollector initialized successfully";
}

void MetricsCollector::Shutdown() {
    if (exposer_) {
        LOG_INFO << "Shutting down MetricsCollector";
        exposer_.reset();
    }
}

// ==================== QPS 相关 ====================

void MetricsCollector::IncrementRequestCount(const std::string& endpoint, const std::string& method) {
    if (!request_total_family_) return;

    auto& counter = GetOrCreateCounter(
        request_total_family_,
        request_counters_,
        request_mutex_,
        {{"endpoint", endpoint}, {"method", method}}
    );
    counter.Increment();
}

// ==================== 延迟 相关 ====================

void MetricsCollector::ObserveLatency(const std::string& endpoint, double latency_us) {
    if (!latency_family_) return;

    auto& histogram = GetOrCreateHistogram(
        latency_family_,
        latency_histograms_,
        latency_mutex_,
        {{"endpoint", endpoint}}
    );
    histogram.Observe(latency_us);
}

MetricsCollector::LatencyTimer::LatencyTimer(MetricsCollector& collector, const std::string& endpoint)
    : collector_(collector), 
      endpoint_(endpoint),
      start_time_(std::chrono::steady_clock::now()) {
}

MetricsCollector::LatencyTimer::~LatencyTimer() {
    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time_);
    collector_.ObserveLatency(endpoint_, static_cast<double>(duration.count()));
}

// ==================== 连接数 相关 ====================

void MetricsCollector::IncrementActiveConnections() {
    if (active_connections_gauge_) {
        active_connections_gauge_->Increment();
    }
}

void MetricsCollector::DecrementActiveConnections() {
    if (active_connections_gauge_) {
        active_connections_gauge_->Decrement();
    }
}

void MetricsCollector::SetActiveConnections(int count) {
    if (active_connections_gauge_) {
        active_connections_gauge_->Set(count);
    }
}

// ==================== 错误率 相关 ====================

void MetricsCollector::IncrementErrorCount(const std::string& error_type, const std::string& endpoint) {
    if (!error_total_family_) return;

    std::map<std::string, std::string> labels = {{"error_type", error_type}};
    if (!endpoint.empty()) {
        labels["endpoint"] = endpoint;
    }

    auto& counter = GetOrCreateCounter(
        error_total_family_,
        error_counters_,
        error_mutex_,
        labels
    );
    counter.Increment();
}

// ==================== 业务指标 ====================

void MetricsCollector::IncrementKafkaProduced(const std::string& topic) {
    if (!kafka_produced_family_) return;

    std::lock_guard<std::mutex> lock(kafka_mutex_);
    std::string key = topic;
    auto it = kafka_produced_counters_.find(key);
    if (it == kafka_produced_counters_.end()) {
        auto& counter = kafka_produced_family_->Add({{"topic", topic}});
        kafka_produced_counters_[key] = &counter;
        counter.Increment();
    } else {
        it->second->Increment();
    }
}

void MetricsCollector::IncrementKafkaConsumed(const std::string& topic) {
    if (!kafka_consumed_family_) return;

    std::lock_guard<std::mutex> lock(kafka_mutex_);
    std::string key = topic;
    auto it = kafka_consumed_counters_.find(key);
    if (it == kafka_consumed_counters_.end()) {
        auto& counter = kafka_consumed_family_->Add({{"topic", topic}});
        kafka_consumed_counters_[key] = &counter;
        counter.Increment();
    } else {
        it->second->Increment();
    }
}

void MetricsCollector::IncrementGrpcCall(const std::string& method, bool success) {
    if (!grpc_calls_family_) return;

    std::lock_guard<std::mutex> lock(grpc_mutex_);
    std::string key = method + "_" + (success ? "success" : "failure");
    auto it = grpc_call_counters_.find(key);
    if (it == grpc_call_counters_.end()) {
        auto& counter = grpc_calls_family_->Add({
            {"method", method},
            {"status", success ? "success" : "failure"}
        });
        grpc_call_counters_[key] = &counter;
        counter.Increment();
    } else {
        it->second->Increment();
    }
}

void MetricsCollector::IncrementWebSocketPush(const std::string& room_id) {
    if (!websocket_push_family_) return;

    std::lock_guard<std::mutex> lock(websocket_mutex_);
    std::string key = room_id;
    auto it = websocket_push_counters_.find(key);
    if (it == websocket_push_counters_.end()) {
        auto& counter = websocket_push_family_->Add({{"room_id", room_id}});
        websocket_push_counters_[key] = &counter;
        counter.Increment();
    } else {
        it->second->Increment();
    }
}

void MetricsCollector::IncrementRedisOp(const std::string& operation, bool success) {
    if (!redis_ops_family_) return;

    std::lock_guard<std::mutex> lock(redis_mutex_);
    std::string key = operation + "_" + (success ? "success" : "failure");
    auto it = redis_op_counters_.find(key);
    if (it == redis_op_counters_.end()) {
        auto& counter = redis_ops_family_->Add({
            {"operation", operation},
            {"status", success ? "success" : "failure"}
        });
        redis_op_counters_[key] = &counter;
        counter.Increment();
    } else {
        it->second->Increment();
    }
}

// ==================== 辅助函数 ====================

Counter& MetricsCollector::GetOrCreateCounter(
    Family<Counter>* family,
    std::map<std::string, Counter*>& cache,
    std::mutex& mutex,
    const std::map<std::string, std::string>& labels) {
    
    // 构造缓存 key
    std::string cache_key;
    for (const auto& label : labels) {
        cache_key += label.first + ":" + label.second + ";";
    }

    std::lock_guard<std::mutex> lock(mutex);
    auto it = cache.find(cache_key);
    if (it == cache.end()) {
        auto& counter = family->Add(labels);
        cache[cache_key] = &counter;
        return counter;
    }
    return *it->second;
}

Histogram& MetricsCollector::GetOrCreateHistogram(
    Family<Histogram>* family,
    std::map<std::string, Histogram*>& cache,
    std::mutex& mutex,
    const std::map<std::string, std::string>& labels) {
    
    // 构造缓存 key
    std::string cache_key;
    for (const auto& label : labels) {
        cache_key += label.first + ":" + label.second + ";";
    }

    std::lock_guard<std::mutex> lock(mutex);
    auto it = cache.find(cache_key);
    if (it == cache.end()) {
        // 定义 Histogram 桶边界（微秒）
        auto& histogram = family->Add(
            labels,
            Histogram::BucketBoundaries{
                100, 500, 1000, 5000, 10000, 50000, 
                100000, 500000, 1000000, 5000000, 10000000
            }
        );
        cache[cache_key] = &histogram;
        return histogram;
    }
    return *it->second;
}

// ==================== 通用计数器 ====================

void MetricsCollector::IncrementCounter(const std::string& name, const std::string& label) {
    if (!grpc_calls_family_) return;  // 复用现有的 family
    
    std::map<std::string, std::string> labels = {
        {"service", service_name_},
        {"name", name},
        {"status", label}
    };
    
    auto& counter = GetOrCreateCounter(grpc_calls_family_, grpc_call_counters_, grpc_mutex_, labels);
    counter.Increment();
}
