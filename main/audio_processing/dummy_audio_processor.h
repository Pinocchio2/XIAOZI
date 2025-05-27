#ifndef DUMMY_AUDIO_PROCESSOR_H
#define DUMMY_AUDIO_PROCESSOR_H

#include <vector>
#include <functional>

#include "audio_processor.h"
#include "audio_codec.h"

/**
 * @brief DummyAudioProcessor 类是 AudioProcessor 接口的一个简单或占位符实现。
 *
 * 该类主要用于测试目的、作为默认的音频处理器，或者在不需要实际音频处理功能的场景下使用。
 * 它实现了 AudioProcessor 接口定义的核心方法，但通常不执行任何复杂的音频信号处理或分析。
 * 其行为更多的是管理处理状态（如启动/停止）和通过回调机制传递数据或事件。
 *
 * 核心功能:
 * - 初始化: 通过 `Initialize(AudioCodec* codec)` 方法设置所使用的音频编解码器。
 * - 数据供给: 通过 `Feed(const std::vector<int16_t>& data)` 方法接收待处理的音频数据。
 * - 状态控制: 使用 `Start()` 和 `Stop()` 方法控制处理流程的启动与停止，`IsRunning()` 用于查询当前运行状态。
 * - 回调注册:
 *   - `OnOutput()`: 允许注册一个回调函数，用于接收处理后的音频数据。在此 "Dummy" 实现中，
 *     输出数据可能是原始输入数据的直接传递，或者是预定义的空数据。
 *   - `OnVadStateChange()`: 允许注册一个回调函数，用于接收语音活动检测 (VAD) 状态变化的通知。
 *     此 "Dummy" 实现可能不触发 VAD 状态变化，或仅模拟一个固定的状态。
 * - 获取预期输入大小: `GetFeedSize()` 方法返回期望的音频数据块大小。
 *
 * 使用示例:
 * @code
 * // 假设 AudioCodec 类已定义，并且有一个实例 myCodec
 * // AudioCodec myCodec;
 *
 * DummyAudioProcessor audioProcessor;
 * // audioProcessor.Initialize(&myCodec); // 初始化，传入编解码器实例
 *
 * audioProcessor.OnOutput([](std::vector<int16_t>&& data) {
 *     // 处理接收到的数据，注意这可能是原始数据或空数据
 *     if (!data.empty()) {
 *         // 对 data 进行处理...
 *     }
 * });
 *
 * audioProcessor.OnVadStateChange([](bool isSpeaking) {
 *     // 根据 isSpeaking 状态执行相应操作...
 * });
 *
 * audioProcessor.Start();
 * if (audioProcessor.IsRunning()) {
 *     // 假设 GetFeedSize() 返回一个值，例如 160
 *     std::vector<int16_t> sampleData(audioProcessor.GetFeedSize()); 
 *     // ... 填充 sampleData ...
 *     audioProcessor.Feed(sampleData);
 * }
 * audioProcessor.Stop();
 * @endcode
 *
 * 构造函数:
 * - `DummyAudioProcessor()`: 默认构造函数。不接受任何参数。
 *
 * 特殊使用限制或潜在副作用:
 * - 必须在调用 `Feed` 或 `Start` 等依赖编解码器的方法之前调用 `Initialize()` 并传入有效的 `AudioCodec` 指针。
 * - 作为一个 "Dummy" (虚拟) 实现，它不应被期望执行任何实际的音频信号处理。
 *   其主要目的是满足 `AudioProcessor` 接口的要求，常用于测试集成或作为无操作 (no-op) 的替代方案。
 * - `OnOutput` 回调接收到的数据和 `OnVadStateChange` 回调接收到的状态可能不反映真实处理结果，
 *   而是由该虚拟实现的具体逻辑决定（例如，直接返回输入、返回空值或固定值）。
 */
class DummyAudioProcessor : public AudioProcessor {
public:
    DummyAudioProcessor() = default;
    ~DummyAudioProcessor() = default;

    void Initialize(AudioCodec* codec) override;
    void Feed(const std::vector<int16_t>& data) override;
    void Start() override;
    void Stop() override;
    bool IsRunning() override;
    void OnOutput(std::function<void(std::vector<int16_t>&& data)> callback) override;
    void OnVadStateChange(std::function<void(bool speaking)> callback) override;
    size_t GetFeedSize() override;

private:
    AudioCodec* codec_ = nullptr;

#endif 