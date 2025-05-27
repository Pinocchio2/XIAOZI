#ifndef _AUDIO_CODEC_H
#define _AUDIO_CODEC_H

#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <driver/i2s_std.h>

#include <vector>
#include <string>
#include <functional>

#include "board.h"

#define AUDIO_CODEC_DMA_DESC_NUM 6
#define AUDIO_CODEC_DMA_FRAME_NUM 240

class AudioCodec {
public:
    // 构造函数
    AudioCodec();
    // 虚析构函数
    virtual ~AudioCodec();
    
    // 设置输出音量
    virtual void SetOutputVolume(int volume);
    // 启用输入
    virtual void EnableInput(bool enable);
    // 启用输出
    virtual void EnableOutput(bool enable);

    // 开始
    void Start();
    // 输出数据
    void OutputData(std::vector<int16_t>& data);
    // 输入数据
    bool InputData(std::vector<int16_t>& data);

   
    // 获取是否为双工模式
    inline bool duplex() const { return duplex_; }
    // 获取是否为输入参考
    inline bool input_reference() const { return input_reference_; }
    // 获取输入采样率
    inline int input_sample_rate() const { return input_sample_rate_; }
    // 获取输出采样率
    inline int output_sample_rate() const { return output_sample_rate_; }
    // 获取输入通道数
    inline int input_channels() const { return input_channels_; }
    // 获取输出通道数
    inline int output_channels() const { return output_channels_; }
    // 获取输出音量
    inline int output_volume() const { return output_volume_; }
    // 获取是否启用输入
    inline bool input_enabled() const { return input_enabled_; }
    // 获取是否启用输出
    inline bool output_enabled() const { return output_enabled_; }

protected:
    // 发送通道句柄
    i2s_chan_handle_t tx_handle_ = nullptr;
    // 接收通道句柄
    i2s_chan_handle_t rx_handle_ = nullptr;

    // 是否为双工模式
    bool duplex_ = false;
    // 是否为输入参考
    bool input_reference_ = false;
    // 是否启用输入
    bool input_enabled_ = false;
    // 是否启用输出
    bool output_enabled_ = false;
    // 输入采样率
    int input_sample_rate_ = 0;
    // 输出采样率
    int output_sample_rate_ = 0;
    // 输入通道数
    int input_channels_ = 1;
    // 输出通道数
    int output_channels_ = 1;
    // 输出音量
    int output_volume_ = 70;

    // 虚函数，读取数据
    virtual int Read(int16_t* dest, int samples) = 0;
    // 虚函数，写入数据
    virtual int Write(const int16_t* data, int samples) = 0;
};

#endif // _AUDIO_CODEC_H
