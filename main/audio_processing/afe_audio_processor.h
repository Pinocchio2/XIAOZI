#ifndef AFE_AUDIO_PROCESSOR_H
#define AFE_AUDIO_PROCESSOR_H

#include <esp_afe_sr_models.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/event_groups.h>

#include <string>
#include <vector>
#include <functional>

#include "audio_processor.h"
#include "audio_codec.h"

/**
 * @brief AfeAudioProcessor 类，实现 AudioProcessor 接口，用于处理来自 ESP AFE (Audio Front-End) 的音频数据。
 *
 * 该类主要设计用于与乐鑫 ESP 平台的音频前端硬件/SDK协同工作，特别适用于语音识别 (SR) 等应用场景。
 * 它负责初始化 AFE 相关资源，接收音频数据（通过 Feed 方法），利用 AFE 功能进行处理（例如语音活动检测 VAD），
 * 并通过注册的回调函数向上层应用提供处理后的音频数据和 VAD 状态。
 *
 * 核心功能:
 * - Initialize(AudioCodec* codec): 初始化 AFE 处理器，并可选择性关联一个音频编解码器。
 * - Feed(const std::vector<int16_t>& data): 向处理器喂入待处理的原始音频数据。
 * - Start()/Stop(): 启动/停止音频处理流程。处理器启动后，内部任务会开始处理通过 Feed 输入的数据。
 * - IsRunning(): 检查音频处理器是否正在运行。
 * - OnOutput(std::function<void(std::vector<int16_t>&& data)> callback): 注册回调函数，用于接收处理后的音频数据。
 * - OnVadStateChange(std::function<void(bool speaking)> callback): 注册回调函数，用于接收 VAD (Voice Activity Detection) 状态变化。
 * - GetFeedSize(): 获取推荐的单次喂入数据块大小，以便优化处理效率。
 *
 * 使用示例 (概念性):
 *
 * 构造函数参数:
 * - AfeAudioProcessor(): 无参数构造函数。内部状态会在 Initialize() 调用时进行设置。
 *
 * 特殊使用限制或潜在副作用:
 * - 依赖乐鑫 ESP-IDF AFE 库 (esp_afe_sr) 及可能相关的硬件支持。
 * - 内部通过一个独立的 FreeRTOS 任务 (AudioProcessorTask) 进行异步音频处理。
 * - 回调函数 (OnOutput, OnVadStateChange) 将在该内部任务的上下文中被调用，
 *   用户在回调中进行复杂或耗时操作时需注意线程安全和避免阻塞该任务。
 * - `Initialize()` 方法必须在调用其他功能方法（如 `Start`, `Feed` 等）之前被成功调用。
 * - 资源管理：内部创建的 FreeRTOS 事件组 (`event_group_`) 和 AFE 实例 (`afe_iface_`, `afe_data_`)
 *   的生命周期由类实例管理，并在析构时 (`~AfeAudioProcessor()`) 负责释放。
 * - `codec_` 成员变量用于存储传递给 `Initialize` 的 `AudioCodec` 指针，其具体用途取决于 AFE 实现细节，
 *   可能用于配置 AFE 或在数据处理中与编解码器交互。
 */
class AfeAudioProcessor : public AudioProcessor {
public:
    AfeAudioProcessor();
    ~AfeAudioProcessor();

    void Initialize(AudioCodec* codec) override;
    void Feed(const std::vector<int16_t>& data) override;
    void Start() override;
    void Stop() override;
    bool IsRunning() override;
    void OnOutput(std::function<void(std::vector<int16_t>&& data)> callback) override;
    void OnVadStateChange(std::function<void(bool speaking)> callback) override;
    size_t GetFeedSize() override;

private:
    EventGroupHandle_t event_group_ = nullptr;
    esp_afe_sr_iface_t* afe_iface_ = nullptr;
    esp_afe_sr_data_t* afe_data_ = nullptr;
    std::function<void(std::vector<int16_t>&& data)> output_callback_;

#endif 