#include "audio_codec.h"
#include "board.h"
#include "settings.h"

#include <esp_log.h>
#include <cstring>
#include <driver/i2s_common.h>

#define TAG "AudioCodec"

AudioCodec::AudioCodec() {
}

AudioCodec::~AudioCodec() {
}

// 输出音频数据
void AudioCodec::OutputData(std::vector<int16_t>& data) {
    // 将数据写入输出流
    Write(data.data(), data.size());
}

bool AudioCodec::InputData(std::vector<int16_t>& data) {
    // 读取数据
    int samples = Read(data.data(), data.size());
    // 如果读取到的样本数大于0，则返回true
    if (samples > 0) {
        return true;
    }
    // 否则返回false
    return false;
}

void AudioCodec::Start() {
    // 创建一个Settings对象，用于获取音频设置
    Settings settings("audio", false);
    // 获取输出音量，如果获取失败，则使用默认值
    output_volume_ = settings.GetInt("output_volume", output_volume_);
    // 如果输出音量小于等于0，则设置为默认值10
    if (output_volume_ <= 0) {
        ESP_LOGW(TAG, "Output volume value (%d) is too small, setting to default (10)", output_volume_);
        output_volume_ = 10;
    }

    // 启用I2S通道
    ESP_ERROR_CHECK(i2s_channel_enable(tx_handle_));
    ESP_ERROR_CHECK(i2s_channel_enable(rx_handle_));

    // 启用输入和输出
    EnableInput(true);
    EnableOutput(true);
    ESP_LOGI(TAG, "Audio codec started");
}

void AudioCodec::SetOutputVolume(int volume) {
    output_volume_ = volume;
    ESP_LOGI(TAG, "Set output volume to %d", output_volume_);
    
    Settings settings("audio", true);
    settings.SetInt("output_volume", output_volume_);
}

void AudioCodec::EnableInput(bool enable) {
    if (enable == input_enabled_) {
        return;
    }
    input_enabled_ = enable;
    ESP_LOGI(TAG, "Set input enable to %s", enable ? "true" : "false");
}

void AudioCodec::EnableOutput(bool enable) {
    if (enable == output_enabled_) {
        return;
    }
    output_enabled_ = enable;
    ESP_LOGI(TAG, "Set output enable to %s", enable ? "true" : "false");
}
