#ifndef _NO_AUDIO_CODEC_H
#define _NO_AUDIO_CODEC_H

#include "audio_codec.h"

#include <driver/gpio.h>
#include <driver/i2s_pdm.h>

/**
 * @brief NoAudioCodec 类，表示一个“无操作”的音频编解码器。
 *
 * @details
 * 该类继承自 AudioCodec，并为其核心的编解码方法（如 Write 和 Read）
 * 提供了空实现或返回表示无操作的状态。
 * 它主要用于需要一个 AudioCodec 对象但实际上不需要进行任何音频编码或解码的场景，
 * 例如，在测试中、或者当音频功能被禁用时，作为占位符或空对象（Null Object Pattern）使用。
 *
 * 核心功能:
 * - Write(const int16_t* data, int samples): 接收音频数据，但不进行任何实际的编码或写入操作。
 *   通常会直接返回，表示操作完成（尽管没有实际工作）或返回0表示没有写入样本。
 * - Read(int16_t* dest, int samples): 尝试读取（解码）音频数据，但不产生任何实际的音频数据。
 *   通常会直接返回，表示没有数据可读（例如返回0）。
 *
 * 构造函数:
 * - NoAudioCodec(): 默认构造函数。该类没有显式定义构造函数，因此会使用编译器生成的默认构造函数。
 *   它不接受任何参数。
 *
 * 使用示例 (概念性):
 * @code
 * std::unique_ptr<AudioCodec> codec;
 * if (isAudioProcessingEnabled) {
 *     codec = std::make_unique<RealAudioCodec>(/* ...params... *\/);
 * } else {
 *     // 当不需要实际音频处理时，使用 NoAudioCodec
 *     codec = std::make_unique<NoAudioCodec>();
 * }
 * // 后续代码可以统一处理 codec 指针，而无需担心其是否为 nullptr
 * codec->Write(someData, numSamples);
 * codec->Read(destinationBuffer, bufferSize);
 * @endcode
 *
 * 特殊使用限制或潜在副作用:
 * - 该编解码器不会对音频数据进行任何实际的编码、解码或修改。
 * - 任何依赖于实际音频数据处理的后续操作，在使用此编解码器时将不会得到预期的音频内容。
 *   例如，如果期望通过 Read() 方法获取解码后的音频，使用此编解码器将不会填充目标缓冲区。
 */
class NoAudioCodec : public AudioCodec {
private:
    virtual int Write(const int16_t* data, int samples) override;
    virtual int Read(int16_t* dest, int samples) override;

    
public:
    virtual ~NoAudioCodec();
};

/**
 * @brief NoAudioCodecDuplex 类，用于实现无独立音频编解码芯片的全双工I2S通信。
 *
 * 该类继承自 NoAudioCodec，并扩展其功能以支持同时进行音频输入（录制）和输出（播放）。
 * 它通常用于直接通过微控制器的I2S接口与外部ADC（模数转换器）和DAC（数模转换器）
 * 或其他支持I2S的设备进行通信的场景，实现音频数据的双向传输。
 *
 * 核心功能：
 * - 初始化I2S总线以进行全双工操作。
 * - 配置用于I2S通信的GPIO引脚（BCLK, WS, Data Out, Data In）。
 * - 允许分别设置输入和输出的采样率。
 *
 * @note
 * - 此类假定目标硬件平台支持I2S全双工模式，并且相关的I2S驱动已正确配置。
 * - 使用的GPIO引脚不应与系统中其他功能冲突。
 * - 实际的音频数据读写操作（如 `read`, `write` 方法）通常由基类 `NoAudioCodec`
 *   或通过与该类协作的其他组件提供。此类主要负责I2S硬件层面的双工配置。
 * - 性能（如延迟、吞吐量）可能受限于微控制器的处理能力和I2S总线的具体配置。
 *
 * 构造函数参数：
 * @param input_sample_rate 输入音频流的采样率 (单位: Hz)。例如：16000, 44100, 48000。
 * @param output_sample_rate 输出音频流的采样率 (单位: Hz)。例如：16000, 44100, 48000。
 * @param bclk I2S总线的位时钟 (Bit Clock) GPIO引脚号。
 * @param ws I2S总线的字选择 (Word Select / LRCLK) GPIO引脚号。
 * @param dout I2S总线的数据输出 (Data Out) GPIO引脚号，用于音频播放（数据从MCU到外部设备）。
 * @param din I2S总线的数据输入 (Data In) GPIO引脚号，用于音频录制（数据从外部设备到MCU）。
 *
 * @code
 * // 示例：初始化一个NoAudioCodecDuplex实例
 * // 假设使用的是ESP32平台，gpio_num_t是其GPIO编号类型
 * // 并且已经包含了相关的头文件，例如 <driver/gpio.h>
 *
 * // 定义I2S引脚 (这些值应根据实际硬件连接进行修改)
 * gpio_num_t i2s_bclk_pin = GPIO_NUM_26; // 示例引脚
 * gpio_num_t i2s_ws_pin   = GPIO_NUM_25; // 示例引脚
 * gpio_num_t i2s_dout_pin = GPIO_NUM_22; // 连接到DAC或放大器
 * gpio_num_t i2s_din_pin  = GPIO_NUM_21; // 连接到ADC或麦克风模块
 *
 * int input_sr = 16000;  // 16kHz 输入采样率
 * int output_sr = 44100; // 44.1kHz 输出采样率
 *
 * // 创建NoAudioCodecDuplex对象
 * NoAudioCodecDuplex audio_interface(
 *     input_sr,
 *     output_sr,
 *     i2s_bclk_pin,
 *     i2s_ws_pin,
 *     i2s_dout_pin,
 *     i2s_din_pin
 * );
 *
 * // 之后可以使用 audio_interface 对象进行音频的读写操作
 * // (具体读写方法通常在基类 NoAudioCodec 中定义或通过其他方式实现)
 * // 例如:
 * // audio_interface.start();
 * // audio_interface.write(output_buffer, size);
 * // audio_interface.read(input_buffer, size);
 * @endcode
 */

class ATK_NoAudioCodecDuplex : public NoAudioCodec {
public:
    ATK_NoAudioCodecDuplex(int input_sample_rate, int output_sample_rate, gpio_num_t bclk, gpio_num_t ws, gpio_num_t dout, gpio_num_t din);
};


/**
 * @brief NoAudioCodecSimplex 类，一个简化的无外部音频编解码器（CODEC）I2S音频驱动实现。
 *
 * 该类继承自 NoAudioCodec，专为直接通过I2S接口处理音频输入和输出而设计，
 * 适用于不使用外部音频编解码芯片的场景。它通过构造函数接收I2S通信所需的
 *各项参数，如采样率、各个I2S信号线的GPIO引脚号以及可选的声道掩码。
 *
 * 核心功能：
 * - 通过构造函数配置并初始化I2S通信所需的参数，包括采样率和GPIO引脚。
 * - 为麦克风输入和扬声器输出设置独立的I2S引脚。
 * - （可选）支持通过 `slot_mask` 参数配置I2S声道，适用于TDM或特定声道选择场景。
 *
 * 使用示例（概念性，具体实例化取决于NoAudioCodec基类的使用方式）：
 *
 * 构造函数参数：
 *
 * 构造函数 1 (无 slot_mask):
 * @param input_sample_rate  int: 输入音频流的采样率 (单位: Hz)。例如 8000, 16000, 44100, 48000。
 * @param output_sample_rate int: 输出音频流的采样率 (单位: Hz)。例如 8000, 16000, 44100, 48000。
 * @param spk_bclk           gpio_num_t: 扬声器I2S接口的位时钟 (BCLK) GPIO引脚。
 * @param spk_ws             gpio_num_t: 扬声器I2S接口的字选择 (WS/LRCK) GPIO引脚。
 * @param spk_dout           gpio_num_t: 扬声器I2S接口的数据输出 (DOUT) GPIO引脚 (MCU -> 扬声器/功放)。
 * @param mic_sck            gpio_num_t: 麦克风I2S接口的串行时钟 (SCK/BCLK) GPIO引脚。
 * @param mic_ws             gpio_num_t: 麦克风I2S接口的字选择 (WS/LRCK) GPIO引脚。
 * @param mic_din            gpio_num_t: 麦克风I2S接口的数据输入 (DIN) GPIO引脚 (麦克风 -> MCU)。
 *
 * 构造函数 2 (带 slot_mask):
 * @param input_sample_rate  int: 输入音频流的采样率 (单位: Hz)。
 * @param output_sample_rate int: 输出音频流的采样率 (单位: Hz)。
 * @param spk_bclk           gpio_num_t: 扬声器I2S接口的位时钟 (BCLK) GPIO引脚。
 * @param spk_ws             gpio_num_t: 扬声器I2S接口的字选择 (WS/LRCK) GPIO引脚。
 * @param spk_dout           gpio_num_t: 扬声器I2S接口的数据输出 (DOUT) GPIO引脚。
 * @param spk_slot_mask      i2s_std_slot_mask_t: 扬声器I2S标准模式下的声道掩码，用于选择活动声道 (例如 `I2S_STD_SLOT_LEFT`, `I2S_STD_SLOT_RIGHT`, `I2S_STD_SLOT_BOTH`)。
 * @param mic_sck            gpio_num_t: 麦克风I2S接口的串行时钟 (SCK/BCLK) GPIO引脚。
 * @param mic_ws             gpio_num_t: 麦克风I2S接口的字选择 (WS/LRCK) GPIO引脚。
 * @param mic_din            gpio_num_t: 麦克风I2S接口的数据输入 (DIN) GPIO引脚。
 * @param mic_slot_mask      i2s_std_slot_mask_t: 麦克风I2S标准模式下的声道掩码，用于选择活动声道。
 *
 * 注意事项：
 * - 确保所选的GPIO引脚对于目标硬件平台是有效的I2S引脚，并且未被其他外设占用。
 * - "Simplex" 在类名中可能指代一种简化的I2S配置方式，或者暗示其设计目标是针对基础的音频流处理，
 *   具体I2S工作模式（如是否使用同一I2S外设控制器实现收发）依赖于底层`NoAudioCodec`基类和硬件驱动的实现。
 * - `i2s_std_slot_mask_t` 参数通常与特定平台的I2S驱动（如ESP-IDF中的I2S驱动）的“标准模式”配合使用，
 *   用于TDM（时分复用）或在立体声/多声道配置中指定活动声道。
 * - 实际的I2S初始化和数据读写操作通常由基类 `NoAudioCodec` 中定义的虚方法（由本类或其子类实现）或具体方法来完成。
 */
class NoAudioCodecSimplex : public NoAudioCodec {
public:
    NoAudioCodecSimplex(int input_sample_rate, int output_sample_rate, gpio_num_t spk_bclk, gpio_num_t spk_ws, gpio_num_t spk_dout, gpio_num_t mic_sck, gpio_num_t mic_ws, gpio_num_t mic_din);
}

/**
 * @brief NoAudioCodecSimplexPdm 类，一个简化的无外部音频编解码器（CODEC）PDM音频驱动实现。
 *
 * 该类继承自 NoAudioCodec，专为直接通过PDM接口处理音频输入和输出而设计，
 * 适用于不使用外部音频编解码芯片的场景。它通过构造函数接收PDM通信所需的
*/
 class NoAudioCodecSimplexPdm : public NoAudioCodec {
public:
    NoAudioCodecSimplexPdm(int input_sample_rate, int output_sample_rate, gpio_num_t spk_bclk, gpio_num_t spk_ws, gpio_num_t spk_dout, gpio_num_t mic_sck,  gpio_num_t mic_din);
    int Read(int16_t* dest, int samples);
};

#endif // _NO_AUDIO_CODEC_H
