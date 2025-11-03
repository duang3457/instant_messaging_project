#ifndef METRICS_COLLECTOR_H
#define METRICS_COLLECTOR_H

#include <prometheus/counter.h>
#include <prometheus/exposer.h>
#include <prometheus/family.h>
#include <prometheus/gauge.h>
#include <prometheus/histogram.h>
#include <prometheus/registry.h>
#include <memory>
#include <string>
#include <map>
#include <mutex>

/**
 * @brief 监控指标收集器
 * 
 * 封装 Prometheus C++ 客户端，提供以下核心指标：
 * - QPS (Queries Per Second): 通过 Counter 计算 rate
 * - 延迟 (Latency): 通过 Histogram 记录请求处理时间
 * - 连接数 (Connections): 通过 Gauge 记录当前活跃连接
 * - 错误率 (Error Rate): 通过 Counter 计算 error/total ratio
 */
class MetricsCollector {
public:
    /**
     * @brief 获取单例实例
     */
    static MetricsCollector& GetInstance();

    /**
     * @brief 初始化 Metrics Exposer（HTTP 服务器）
     * @param bind_address 绑定地址，格式: "0.0.0.0:9091"
     * @param service_name 服务名称（用于标签）: "comet", "logic", "job"
     */
    void Initialize(const std::string& bind_address, const std::string& service_name);

    /**
     * @brief 停止 Metrics Exposer
     */
    void Shutdown();

    // ==================== QPS 相关 ====================
    
    /**
     * @brief 增加请求计数（用于计算 QPS）
     * @param endpoint API 端点名称（如 "/send", "/websocket"）
     * @param method HTTP 方法（如 "GET", "POST"）
     */
    void IncrementRequestCount(const std::string& endpoint, const std::string& method = "POST");

    // ==================== 延迟 相关 ====================
    
    /**
     * @brief 记录请求延迟（微秒）
     * @param endpoint API 端点名称
     * @param latency_us 延迟（微秒）
     */
    void ObserveLatency(const std::string& endpoint, double latency_us);

    /**
     * @brief RAII 风格的延迟计时器
     */
    class LatencyTimer {
    public:
        LatencyTimer(MetricsCollector& collector, const std::string& endpoint);
        ~LatencyTimer();

    private:
        MetricsCollector& collector_;
        std::string endpoint_;
        std::chrono::steady_clock::time_point start_time_;
    };

    // ==================== 连接数 相关 ====================
    
    /**
     * @brief 增加活跃连接数
     */
    void IncrementActiveConnections();

    /**
     * @brief 减少活跃连接数
     */
    void DecrementActiveConnections();

    /**
     * @brief 设置活跃连接数（直接设置值）
     */
    void SetActiveConnections(int count);

    // ==================== 错误率 相关 ====================
    
    /**
     * @brief 增加错误计数
     * @param error_type 错误类型（如 "timeout", "parse_error", "db_error"）
     * @param endpoint API 端点（可选）
     */
    void IncrementErrorCount(const std::string& error_type, const std::string& endpoint = "");

    // ==================== 业务指标 ====================
    
    /**
     * @brief 记录 Kafka 消息生产
     */
    void IncrementKafkaProduced(const std::string& topic);

    /**
     * @brief 记录 Kafka 消息消费
     */
    void IncrementKafkaConsumed(const std::string& topic);

    /**
     * @brief 记录 gRPC 调用
     */
    void IncrementGrpcCall(const std::string& method, bool success);

    /**
     * @brief 记录 WebSocket 消息推送
     */
    void IncrementWebSocketPush(const std::string& room_id);

    /**
     * @brief 记录 Redis 操作
     */
    void IncrementRedisOp(const std::string& operation, bool success);

    /**
     * @brief 通用计数器（用于自定义指标）
     * @param name 指标名称（如 "logic_forward"）
     * @param label 标签值（如 "success", "failed"）
     */
    void IncrementCounter(const std::string& name, const std::string& label);

private:
    MetricsCollector();
    ~MetricsCollector();

    // 禁止拷贝和赋值
    MetricsCollector(const MetricsCollector&) = delete;
    MetricsCollector& operator=(const MetricsCollector&) = delete;

    // Prometheus 核心对象
    std::shared_ptr<prometheus::Registry> registry_;
    std::unique_ptr<prometheus::Exposer> exposer_;
    std::string service_name_;

    // ==================== 指标族 ====================
    
    // QPS: 请求总数（Counter）
    prometheus::Family<prometheus::Counter>* request_total_family_;
    std::map<std::string, prometheus::Counter*> request_counters_;
    std::mutex request_mutex_;

    // 延迟: 请求处理时间（Histogram）
    prometheus::Family<prometheus::Histogram>* latency_family_;
    std::map<std::string, prometheus::Histogram*> latency_histograms_;
    std::mutex latency_mutex_;

    // 连接数: 当前活跃连接（Gauge）
    prometheus::Gauge* active_connections_gauge_;

    // 错误: 错误总数（Counter）
    prometheus::Family<prometheus::Counter>* error_total_family_;
    std::map<std::string, prometheus::Counter*> error_counters_;
    std::mutex error_mutex_;

    // 业务指标: Kafka
    prometheus::Family<prometheus::Counter>* kafka_produced_family_;
    prometheus::Family<prometheus::Counter>* kafka_consumed_family_;
    std::map<std::string, prometheus::Counter*> kafka_produced_counters_;
    std::map<std::string, prometheus::Counter*> kafka_consumed_counters_;
    std::mutex kafka_mutex_;

    // 业务指标: gRPC
    prometheus::Family<prometheus::Counter>* grpc_calls_family_;
    std::map<std::string, prometheus::Counter*> grpc_call_counters_;
    std::mutex grpc_mutex_;

    // 业务指标: WebSocket
    prometheus::Family<prometheus::Counter>* websocket_push_family_;
    std::map<std::string, prometheus::Counter*> websocket_push_counters_;
    std::mutex websocket_mutex_;

    // 业务指标: Redis
    prometheus::Family<prometheus::Counter>* redis_ops_family_;
    std::map<std::string, prometheus::Counter*> redis_op_counters_;
    std::mutex redis_mutex_;

    // 辅助函数
    prometheus::Counter& GetOrCreateCounter(
        prometheus::Family<prometheus::Counter>* family,
        std::map<std::string, prometheus::Counter*>& cache,
        std::mutex& mutex,
        const std::map<std::string, std::string>& labels);

    prometheus::Histogram& GetOrCreateHistogram(
        prometheus::Family<prometheus::Histogram>* family,
        std::map<std::string, prometheus::Histogram*>& cache,
        std::mutex& mutex,
        const std::map<std::string, std::string>& labels);
};

#endif // METRICS_COLLECTOR_H
