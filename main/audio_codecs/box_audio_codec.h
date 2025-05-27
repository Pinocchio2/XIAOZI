#ifndef _BOX_AUDIO_CODEC_H
#define _BOX_AUDIO_CODEC_H

#include "audio_codec.h"

#include <esp_codec_dev.h>
#include <esp_codec_dev_defaults.h>

/**
 * @brief BoxAudioCodec 类，用于管理特定 "Box" 硬件平台上的音频输入和输出功能。
 *
 * 该类封装了与音频编解码器芯片（如 ES8311 和 ES7210）的交互，
 * 通过 I2S 接口进行音频数据传输，并通过 I2C 接口进行控制。
 * 它继承自 AudioCodec 基类，并实现了其核心的音频读写和控制方法，
 * 旨在为基于 ESP32 系列芯片的特定硬件盒子提供音频解决方案。
 *
 * 核心功能:
 * - 初始化和配置音频输入（通常来自 ES7210 麦克风阵列）和输出（通常至 ES8311驱动的扬声器）通道。
 * - 从配置的音频输入设备读取PCM音频数据。
 * - 向配置的音频输出设备写入PCM音频数据。
 * - 控制音频输出的音量。
 * - 独立启用或禁用音频输入和输出链路。
 * - 管理底层 ESP-IDF 音频编解码器设备句柄。
 *
 * 使用示例 (概念性):
 *
 * 构造函数参数:
 * @param i2c_master_handle (void*): 指向已初始化的 I2C 主设备驱动的句柄，用于与 ES8311 和 ES7210 芯片通信。
 * @param input_sample_rate (int): 输入音频流的目标采样率，单位 Hz (例如 16000, 48000)。
 * @param output_sample_rate (int): 输出音频流的目标采样率，单位 Hz (例如 44100, 48000)。
 * @param mclk (gpio_num_t): I2S 主时钟 (MCLK) 输出的 GPIO 引脚号。
 * @param bclk (gpio_num_t): I2S 位时钟 (BCLK) 输出/输入的 GPIO 引脚号。
 * @param ws (gpio_num_t): I2S 帧同步/字选择 (WS/LRCK) 输出/输入的 GPIO 引脚号。
 * @param dout (gpio_num_t): I2S 数据输出 (DOUT) 的 GPIO 引脚号 (MCU 将数据发送到输出编解码器如ES8311)。
 * @param din (gpio_num_t): I2S 数据输入 (DIN) 的 GPIO 引脚号 (MCU 从输入编解码器如ES7210接收数据)。
 * @param pa_pin (gpio_num_t): 控制外部功率放大器 (PA) 使能的 GPIO 引脚号。
 * @param es8311_addr (uint8_t): ES8311 音频编解码器芯片的 I2C 从设备地址 (通常用于音频输出)。
 * @param es7210_addr (uint8_t): ES7210 音频编解码器芯片的 I2C 从设备地址 (通常用于音频输入)。
 * @param input_reference (bool): 一个布尔标志，用于配置输入参考。其具体作用（如选择参考电压、启用特定偏置等）
 *                                取决于 ES7210 芯片的配置和硬件电路设计。
 *
 * 特殊使用限制或潜在副作用:
 * - 在实例化此类之前，必须确保 I2C 主控制器已成功初始化。
 * - 所有指定的 GPIO 引脚必须是有效的，并且没有被系统其他部分占用。
 * - 该类设计用于特定的硬件配置，即包含 ES8311 和 ES7210 音频芯片并通过 I2S 和 I2C 连接的系统。
 * - 不正确的 GPIO 配置或 I2C 地址可能导致硬件初始化失败或工作异常。
 * - 音频质量（如信噪比、失真度）会受到 `input_reference` 参数设置、采样率选择以及整体硬件布局和电源质量的影响。
 * - 内部依赖 ESP-IDF 的 `esp_codec_dev` 组件进行编解码器设备的抽象和管理。
 * - 析构函数会尝试释放通过 `esp_codec_dev_close` 创建的编解码器设备句柄。
 */
class BoxAudioCodec : public AudioCodec {
private:
    const audio_codec_data_if_t* data_if_ = nullptr;
    const audio_codec_ctrl_if_t* out_ctrl_if_ = nullptr;
    const audio_codec_if_t* out_codec_if_ = nullptr;
    const audio_codec_ctrl_if_t* in_ctrl_if_ = nullptr;
    const audio_codec_if_t* in_codec_if_ = nullptr;
    const audio_codec_gpio_if_t* gpio_if_ = nullptr;

    esp_codec_dev_handle_t output_dev_ = nullptr;
    esp_codec_dev_handle_t input_dev_ = nullptr;

    void CreateDuplexChannels(gpio_num_t mclk, gpio_num_t bclk, gpio_num_t ws, gpio_num_t dout, gpio_num_t din);

    virtual int Read(int16_t* dest, int samples) override;
    virtual int Write(const int16_t* data, int samples) override;

public:
    BoxAudioCodec(void* i2c_master_handle, int input_sample_rate, int output_sample_rate,
        gpio_num_t mclk, gpio_num_t bclk, gpio_num_t ws, gpio_num_t dout, gpio_num_t din,
        gpio_num_t pa_pin, uint8_t es8311_addr, uint8_t es7210_addr, bool input_reference);
    virtual ~BoxAudioCodec();

    virtual void SetOutputVolume(int volume) override;
    virtual void EnableInput(bool enable) override;

#endif // _BOX_AUDIO_CODEC_H
