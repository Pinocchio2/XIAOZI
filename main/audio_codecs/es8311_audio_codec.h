#ifndef _ES8311_AUDIO_CODEC_H
#define _ES8311_AUDIO_CODEC_H

#include "audio_codec.h"

#include <driver/i2c_master.h>
#include <driver/gpio.h>
#include <esp_codec_dev.h>
#include <esp_codec_dev_defaults.h>

/**
 * @brief ES8311 音频编解码器驱动类。
 *
 * 该类封装了与 ES8311 音频编解码芯片交互的逻辑，提供了音频输入、输出、
 * 音量控制等功能。它继承自 AudioCodec 基类，并实现了其定义的接口。
 * ES8311 是一款低功耗、高性能的单声道音频编解码芯片，常用于与 ESP32 等
 * 微控制器配合进行音频处理。
 *
 * 核心功能:
 * - 初始化 ES8311 芯片，通过 I2C 配置其内部寄存器。
 * - 配置 I2S 接口用于音频数据传输（包括双工通道的创建）。
 * - 实现音频数据的读取 (ADC 路径) 和写入 (DAC 路径)。
 * - 控制输出音量。
 * - 使能或禁用音频输入和输出通道。
 * - （可选）通过指定 GPIO 控制外部功率放大器 (PA) 的使能。
 *
 * 使用说明:
 * 实例化此类时，需要提供 I2C 控制器句柄、I2C 端口号、I2S 通信所需的各项
 * GPIO 引脚号（MCLK, BCLK, WS, DOUT, DIN）、输入和输出采样率、
 * ES8311 的 I2C 从设备地址，以及可选的功放使能引脚。
 * 确保在实例化前，相关的 I2C 和 GPIO 外设已由底层驱动正确初始化。
 *
 * 构造函数参数:
 * @param i2c_master_handle  I2C 主机控制器句柄 (例如 ESP-IDF 中的 `i2c_master_bus_handle_t` 或类似句柄，具体类型取决于I2C驱动实现)。
 * @param i2c_port           I2C 端口号 (例如 `I2C_NUM_0`)。
 * @param input_sample_rate  输入音频流的采样率 (单位: Hz)，例如 8000, 16000, 44100, 48000。
 * @param output_sample_rate 输出音频流的采样率 (单位: Hz)，例如 8000, 16000, 44100, 48000。
 * @param mclk               I2S MCLK (Master Clock) GPIO 引脚号。
 * @param bclk               I2S BCLK (Bit Clock) GPIO 引脚号。
 * @param ws                 I2S WS (Word Select / Left-Right Clock) GPIO 引脚号。
 * @param dout               I2S Data Out (数据从微控制器输出到 ES8311) GPIO 引脚号。
 * @param din                I2S Data In (数据从 ES8311 输入到微控制器) GPIO 引脚号。
 * @param pa_pin             外部功率放大器 (PA) 使能 GPIO 引脚号。如果未使用外部功放或不由该类控制，可设为 `GPIO_NUM_NC`。
 * @param es8311_addr        ES8311 芯片的 I2C 从设备地址 (通常是 0x18 或 0x19，取决于 AD0 引脚电平)。
 * @param use_mclk           (可选, 默认为 true) 是否为 ES8311 提供 MCLK。如果 ES8311
 *                           配置为从 BCLK/LRCK 生成内部时钟，或者 MCLK 由其他源提供，则可以根据 ES8311 的具体配置设为 false。
 *                           通常情况下，建议提供 MCLK 以获得更佳性能和更灵活的时钟配置。
 *
 * 注意事项:
 * - 使用此类前，请确保 ESP32 (或其他微控制器) 的 I2C 和 I2S 外设已正确初始化，并且所选 GPIO 引脚未被其他功能占用。
 * - `pa_pin` 用于控制外部功放，其状态会直接影响最终的音频输出。如果使用，请确保其连接正确。
 * - 该类依赖于 ESP-IDF 的编解码器设备抽象层接口 (如 `esp_codec_dev_handle_t` 及其相关函数) 来管理底层的编解码器设备。
 * - ES8311 芯片必须已正确连接到微控制器，并已上电。
 * - 音频质量和稳定性也取决于 PCB 布局、电源质量和正确的时钟配置。
 */

class Es8311AudioCodec : public AudioCodec {
private:
    // 音频数据接口
    const audio_codec_data_if_t* data_if_ = nullptr;
    // 音频控制接口
    const audio_codec_ctrl_if_t* ctrl_if_ = nullptr;
    // 音频接口
    const audio_codec_if_t* codec_if_ = nullptr;
    // 音频GPIO接口
    const audio_codec_gpio_if_t* gpio_if_ = nullptr;

    // 输出设备句柄
    esp_codec_dev_handle_t output_dev_ = nullptr;
    // 输入设备句柄
    esp_codec_dev_handle_t input_dev_ = nullptr;
    // PA引脚
    gpio_num_t pa_pin_ = GPIO_NUM_NC;

    // 创建双工通道
    void CreateDuplexChannels(gpio_num_t mclk, gpio_num_t bclk, gpio_num_t ws, gpio_num_t dout, gpio_num_t din);

    // 重写读取函数
    virtual int Read(int16_t* dest, int samples) override;
    // 重写写入函数
    virtual int Write(const int16_t* data, int samples) override;

public:
    // 构造函数
    Es8311AudioCodec(void* i2c_master_handle, i2c_port_t i2c_port, int input_sample_rate, int output_sample_rate,
        gpio_num_t mclk, gpio_num_t bclk, gpio_num_t ws, gpio_num_t dout, gpio_num_t din,
        gpio_num_t pa_pin, uint8_t es8311_addr, bool use_mclk = true);
    // 析构函数
    virtual ~Es8311AudioCodec();

 }
#endif // _ES8311_AUDIO_CODEC_H