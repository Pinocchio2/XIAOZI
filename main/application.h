#ifndef _APPLICATION_H_
#define _APPLICATION_H_

#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <freertos/task.h>
#include <esp_timer.h>

#include <string>
#include <mutex>
#include <list>
#include <vector>
#include <condition_variable>
#include <memory>

#include <opus_encoder.h>
#include <opus_decoder.h>
#include <opus_resampler.h>

#include "protocol.h"
#include "ota.h"
#include "background_task.h"
#include "audio_processor.h"

#if CONFIG_USE_WAKE_WORD_DETECT
#include "wake_word_detect.h"
#endif

#define SCHEDULE_EVENT (1 << 0)
#define AUDIO_INPUT_READY_EVENT (1 << 1)
#define AUDIO_OUTPUT_READY_EVENT (1 << 2)
#define CHECK_NEW_VERSION_DONE_EVENT (1 << 3)

// 枚举设备状态
enum DeviceState {
    // 未知状态
    kDeviceStateUnknown,
    // 启动状态
    kDeviceStateStarting,
    // 配置WiFi状态
    kDeviceStateWifiConfiguring,
    // 空闲状态
    kDeviceStateIdle,
    // 连接状态
    kDeviceStateConnecting,
    // 监听状态
    kDeviceStateListening,
    // 说话状态
    kDeviceStateSpeaking,
    // 升级状态
    kDeviceStateUpgrading,
    // 激活状态
    kDeviceStateActivating,
    // 致命错误状态
    kDeviceStateFatalError
};

#define OPUS_FRAME_DURATION_MS 60

class Application {
public:
    // 获取Application实例的静态方法
    static Application& GetInstance() {
        // 定义一个静态的Application实例
        static Application instance;
        // 返回该实例
        return instance;
    }
    // 删除拷贝构造函数和赋值运算符
    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;

    // 启动应用程序
    void Start();
    // 获取设备状态
    DeviceState GetDeviceState() const { return device_state_; }
    // 判断是否检测到声音
    bool IsVoiceDetected() const { return voice_detected_; }
    // 调度回调函数
    void Schedule(std::function<void()> callback);
    // 设置设备状态
    void SetDeviceState(DeviceState state);
    // 发送提示信息
    void Alert(const char* status, const char* message, const char* emotion = "", const std::string_view& sound = "");
    // 关闭提示信息
    void DismissAlert();
    // 中止说话
    void AbortSpeaking(AbortReason reason);
    // 切换聊天状态
    void ToggleChatState();
    // 开始监听
    void StartListening();
    // 停止监听
    void StopListening();
    // 更新物联网状态
    void UpdateIotStates();
    // 重启设备
    void Reboot();
    // 唤醒词调用
    void WakeWordInvoke(const std::string& wake_word);
    // 播放声音
    void PlaySound(const std::string_view& sound);
    // 判断是否可以进入睡眠模式
    bool CanEnterSleepMode();

private:
    // 构造函数
    Application();
    // 析构函数
    ~Application();

#if CONFIG_USE_WAKE_WORD_DETECT
    // 唤醒词检测
    WakeWordDetect wake_word_detect_;
#endif
    // 音频处理
    std::unique_ptr<AudioProcessor> audio_processor_;
    // OTA
    Ota ota_;
    // 互斥锁
    std::mutex mutex_;
    // 主任务列表
    std::list<std::function<void()>> main_tasks_;
    // 协议
    std::unique_ptr<Protocol> protocol_;
    // 事件组句柄
    EventGroupHandle_t event_group_ = nullptr;
    // 定时器句柄
    esp_timer_handle_t clock_timer_handle_ = nullptr;
    // 设备状态
    volatile DeviceState device_state_ = kDeviceStateUnknown;
    // 听写模式
    ListeningMode listening_mode_ = kListeningModeAutoStop;
#if CONFIG_USE_DEVICE_AEC || CONFIG_USE_SERVER_AEC
    // 实时聊天是否启用
    bool realtime_chat_enabled_ = true;
#else
    // 实时聊天是否启用
    bool realtime_chat_enabled_ = false;
#endif
    // 是否已中止
    bool aborted_ = false;
    // 是否已检测到声音
    bool voice_detected_ = false;
    // 是否正在解码音频
    bool busy_decoding_audio_ = false;
    // 时钟滴答数
    int clock_ticks_ = 0;
    // 检查新版本任务句柄
    TaskHandle_t check_new_version_task_handle_ = nullptr;

    // Audio encode / decode
    TaskHandle_t audio_loop_task_handle_ = nullptr;
    BackgroundTask* background_task_ = nullptr;
    std::chrono::steady_clock::time_point last_output_time_;
    std::list<AudioStreamPacket> audio_decode_queue_;
    std::condition_variable audio_decode_cv_;

    // 新增：用于维护音频包的timestamp队列
    std::list<uint32_t> timestamp_queue_;
    std::mutex timestamp_mutex_;
    std::atomic<uint32_t> last_output_timestamp_ = 0;

    std::unique_ptr<OpusEncoderWrapper> opus_encoder_;
    std::unique_ptr<OpusDecoderWrapper> opus_decoder_;

    OpusResampler input_resampler_;
    OpusResampler reference_resampler_;
    OpusResampler output_resampler_;

    void MainEventLoop();
    void OnAudioInput();
    void OnAudioOutput();
    void ReadAudio(std::vector<int16_t>& data, int sample_rate, int samples);
    void ResetDecoder();
    void SetDecodeSampleRate(int sample_rate, int frame_duration);
    void CheckNewVersion();
    void ShowActivationCode();
    void OnClockTimer();
    void SetListeningMode(ListeningMode mode);
    void AudioLoop();
};

#endif // _APPLICATION_H_
