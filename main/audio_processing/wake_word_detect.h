#ifndef WAKE_WORD_DETECT_H
#define WAKE_WORD_DETECT_H

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/event_groups.h>

#include <esp_afe_sr_models.h>
#include <esp_nsn_models.h>

#include <list>
#include <string>
#include <vector>
#include <functional>
#include <mutex>
#include <condition_variable>

#include "audio_codec.h"

/**
 * @brief 唤醒词检测类 (WakeWordDetect Class)
 * 
 * 该类用于从音频流中检测预定义的唤醒词。它封装了底层的音频处理和
 * 唤醒词识别引擎（例如，可能基于 ESP-IDF 中的 AFE 和 WakeNet 技术），
 * 提供了一个相对高层的接口来实现语音唤醒功能。
 * 
 * 核心功能:
 * - 初始化唤醒词检测服务，包括加载模型和配置参数。
 * - 接收并向底层引擎馈送PCM音频数据。
 * - 启动和停止唤醒词检测过程。
 * - 通过注册回调函数，在检测到唤醒词时通知应用程序。
 * - 获取最近一次检测到的唤醒词字符串。
 * - (可选功能) 缓存包含唤醒词的原始音频数据，并能将其编码为 Opus 格式。
 * 
 * 使用示例:
 * 
 * 构造函数参数:
 * - `WakeWordDetect()`: 默认构造函数，不接受参数。对象的初始化主要通过 `Initialize()` 方法完成。
 * 
 * 特殊使用限制或潜在副作用:
 * - 必须在调用大多数其他方法（如 `StartDetection`, `Feed` 等）之前调用 `Initialize()` 方法。
 * - 传递给 `Initialize()` 的 `AudioCodec` 指针所指向的对象必须在 `WakeWordDetect` 实例
 *   使用它的整个生命周期内保持有效。
 * - 类的正常工作可能依赖于特定的硬件加速或底层 SDK（如 ESP-IDF AFE/WakeNet 组件）。
 * - 内部会创建并管理线程/任务（如 `wake_word_encode_task_`）和同步原语（如 `std::mutex`, `EventGroupHandle_t`），
 *   这会消耗系统资源。
 * - `Feed()` 方法应以特定大小（可通过 `GetFeedSize()` 获取）的音频数据块进行调用，以匹配底层引擎的期望。
 */
class WakeWordDetect {
public:
    // 构造函数
    WakeWordDetect();
    // 析构函数
    ~WakeWordDetect();

    // 初始化音频编解码器
    void Initialize(AudioCodec* codec);
    // 输入音频数据
    void Feed(const std::vector<int16_t>& data);
    // 设置唤醒词检测回调函数
    void OnWakeWordDetected(std::function<void(const std::string& wake_word)> callback);
    // 开始唤醒词检测
    void StartDetection();
    // 停止唤醒词检测
    void StopDetection();
    // 判断唤醒词检测是否正在进行
    bool IsDetectionRunning();
    // 获取输入音频数据的大小
    size_t GetFeedSize();
    // 编码唤醒词数据
    void EncodeWakeWordData();
    // 获取唤醒词的Opus编码数据
    bool GetWakeWordOpus(std::vector<uint8_t>& opus);
    // 获取最后一次检测到的唤醒词
    const std::string& GetLastDetectedWakeWord() const { return last_detected_wake_word_; }

private:
    // AFE接口
    esp_afe_sr_iface_t* afe_iface_ = nullptr;
    // AFE数据
    esp_afe_sr_data_t* afe_data_ = nullptr;
    // 唤醒词模型
    char* wakenet_model_ = NULL;
    // 唤醒词列表
    std::vector<std::string> wake_words_;
    // 事件组
    EventGroupHandle_t event_group_;
    // 唤醒词检测回调函数
    std::function<void(const std::string& wake_word)> wake_word_detected_callback_;
    // 音频编解码器
    AudioCodec* codec_ = nullptr;
    // 最后一次检测到的唤醒词
    std::string last_detected_wake_word_;

    // 唤醒词编码任务句柄
    TaskHandle_t wake_word_encode_task_ = nullptr;
    // 唤醒词编码任务缓冲区
    StaticTask_t wake_word_encode_task_buffer_;
    // 唤醒词编码任务堆栈
    StackType_t* wake_word_encode_task_stack_ = nullptr;
    // 唤醒词PCM数据列表
    std::list<std::vector<int16_t>> wake_word_pcm_;
}

#endif
