#include "box_audio_codec.h"

#include <esp_log.h>
#include <driver/i2c_master.h>
#include <driver/i2s_tdm.h>

static const char TAG[] = "BoxAudioCodec";

/**
 * @brief BoxAudioCodec 构造函数。
 * \n\t    初始化音频编解码器，配置I2S、I2C接口，并设置输入和输出音频设备。
 * \n\t    此构造函数特别用于双工音频操作，支持使用ES8311进行音频输出和ES7210进行音频输入。
 *
 * @param i2c_master_handle I2C主设备句柄，用于与音频编解码器芯片通信。
 * @param input_sample_rate 输入音频的采样率（单位：Hz）。
 * @param output_sample_rate 输出音频的采样率（单位：Hz）。
 * @param mclk GPIO引脚号，用于MCLK（主时钟）信号。
 * @param bclk GPIO引脚号，用于BCLK（位时钟）信号。
 * @param ws GPIO引脚号，用于WS（字选择/左右时钟）信号。
 * @param dout GPIO引脚号，用于I2S数据输出。
 * @param din GPIO引脚号，用于I2S数据输入。
 * @param pa_pin GPIO引脚号，用于控制功率放大器。
 * @param es8311_addr ES8311音频编解码器（通常用于输出/DAC）的I2C设备地址。
 * @param es7210_addr ES7210音频编解码器（通常用于输入/ADC）的I2C设备地址。
 * @param input_reference 布尔值，指示是否使用参考输入。如果为true，则输入通道配置为2（用于回声消除等场景）；否则为1。
 */
BoxAudioCodec::BoxAudioCodec(void* i2c_master_handle, int input_sample_rate, int output_sample_rate,
    gpio_num_t mclk, gpio_num_t bclk, gpio_num_t ws, gpio_num_t dout, gpio_num_t din,
    gpio_num_t pa_pin, uint8_t es8311_addr, uint8_t es7210_addr, bool input_reference) {
    duplex_ = true; // 是否双工
    input_reference_ = input_reference; // 是否使用参考输入，实现回声消除
    input_channels_ = input_reference_ ? 2 : 1; // 输入通道数
    input_sample_rate_ = input_sample_rate;
    output_sample_rate_ = output_sample_rate;

    CreateDuplexChannels(mclk, bclk, ws, dout, din);

    
    audio_codec_i2s_cfg_t i2s_cfg = {
        .port = I2S_NUM_0,
        .rx_handle = rx_handle_,
        .tx_handle = tx_handle_,
    };
    data_if_ = audio_codec_new_i2s_data(&i2s_cfg);
    assert(data_if_ != NULL);

    
    audio_codec_i2c_cfg_t i2c_cfg = {
        .port = (i2c_port_t)1,
        .addr = es8311_addr,
        .bus_handle = i2c_master_handle,
    };
    out_ctrl_if_ = audio_codec_new_i2c_ctrl(&i2c_cfg);
    assert(out_ctrl_if_ != NULL);

    gpio_if_ = audio_codec_new_gpio();
    assert(gpio_if_ != NULL);

    es8311_codec_cfg_t es8311_cfg = {};
    es8311_cfg.ctrl_if = out_ctrl_if_;
    es8311_cfg.gpio_if = gpio_if_;
    es8311_cfg.codec_mode = ESP_CODEC_DEV_WORK_MODE_DAC;
    es8311_cfg.pa_pin = pa_pin;
    es8311_cfg.use_mclk = true;
    es8311_cfg.hw_gain.pa_voltage = 5.0;
    es8311_cfg.hw_gain.codec_dac_voltage = 3.3;
    out_codec_if_ = es8311_codec_new(&es8311_cfg);
    assert(out_codec_if_ != NULL);

    esp_codec_dev_cfg_t dev_cfg = {
        .dev_type = ESP_CODEC_DEV_TYPE_OUT,
        .codec_if = out_codec_if_,
        .data_if = data_if_,
    };
    output_dev_ = esp_codec_dev_new(&dev_cfg);
    assert(output_dev_ != NULL);

    // Input
    i2c_cfg.addr = es7210_addr;
    in_ctrl_if_ = audio_codec_new_i2c_ctrl(&i2c_cfg);
    assert(in_ctrl_if_ != NULL);

    es7210_codec_cfg_t es7210_cfg = {};
    es7210_cfg.ctrl_if = in_ctrl_if_;
    es7210_cfg.mic_selected = ES7120_SEL_MIC1 | ES7120_SEL_MIC2 | ES7120_SEL_MIC3 | ES7120_SEL_MIC4;
    in_codec_if_ = es7210_codec_new(&es7210_cfg);
    assert(in_codec_if_ != NULL);

    dev_cfg.dev_type = ESP_CODEC_DEV_TYPE_IN;
    dev_cfg.codec_if = in_codec_if_;
    input_dev_ = esp_codec_dev_new(&dev_cfg);
    assert(input_dev_ != NULL);

    ESP_LOGI(TAG, "BoxAudioDevice initialized");
}

/**
 * @brief BoxAudioCodec 类的析构函数。
 *
 * 负责清理和释放与 BoxAudioCodec 实例相关的所用资源。
 * 此析构函数确保正确关闭和删除音频输入输出设备，
 * 并释放所有已分配的编解码器接口，以防止内存泄漏和资源占用。
 *
 * 主要执行以下操作：
 *  - 关闭并删除输出音频设备 (`output_dev_`)。
 *  - 关闭并删除输入音频设备 (`input_dev_`)。
 *  - 删除输入编解码器接口 (`in_codec_if_`)。
 *  - 删除输入控制接口 (`in_ctrl_if_`)。
 *  - 删除输出编解码器接口 (`out_codec_if_`)。
 *  - 删除输出控制接口 (`out_ctrl_if_`)。
 *  - 删除 GPIO 接口 (`gpio_if_`)。
 *  - 删除数据接口 (`data_if_`)。
 */
BoxAudioCodec::~BoxAudioCodec() {
    ESP_ERROR_CHECK(esp_codec_dev_close(output_dev_));
    esp_codec_dev_delete(output_dev_);
    ESP_ERROR_CHECK(esp_codec_dev_close(input_dev_));
    esp_codec_dev_delete(input_dev_);

    audio_codec_delete_codec_if(in_codec_if_);
    audio_codec_delete_ctrl_if(in_ctrl_if_);

/**
 * @brief 创建并初始化用于音频输入和输出的 I2S 双工通道。
 *
 * 此函数在 I2S_NUM_0 (ESP32 的 I2S 控制器0) 上设置并初始化一对 I2S 通道：
 * 一个发送 (TX) 通道和一个接收 (RX) 通道，从而实现全双工音频通信。
 * 两个通道均配置为 I2S 主模式 (I2S_ROLE_MASTER)。
 *
 * 发送 (TX) 通道:
 * - 使用 `i2s_channel_init_std_mode` 初始化。
 * - 配置为标准的 I2S 立体声模式 (I2S_SLOT_MODE_STEREO)。
 * - 用于音频数据输出。
 *
 * 接收 (RX) 通道:
 * - 使用 `i2s_channel_init_tdm_mode` 初始化。
 * - 配置为 TDM (时分复用) 模式。
 * - `slot_mode` 设置为 `I2S_SLOT_MODE_STEREO`。
 * - `slot_mask` 设置为 `i2s_tdm_slot_mask_t(I2S_TDM_SLOT0 | I2S_TDM_SLOT1 | I2S_TDM_SLOT2 | I2S_TDM_SLOT3)`，
 *   表示启用了4个 TDM 时隙。然而，由于 `slot_mode` 为立体声，仅前两个启用的时隙 (通常是 SLOT0 和 SLOT1)
 *   将用于接收立体声音频数据。
 * - 用于音频数据输入。
 *
 * 函数开始时会通过 `assert` 检查以确保成员变量 `input_sample_rate_` (输入采样率)
 * 和 `output_sample_rate_` (输出采样率) 的值相等。
 * DMA (直接内存访问) 通道配置了 `AUDIO_CODEC_DMA_DESC_NUM` 个描述符和
 * 每个描述符 `AUDIO_CODEC_DMA_FRAME_NUM` 帧。DMA 回调后的自动清除功能 (`auto_clear_after_cb`) 已启用。
 *
 * @param mclk   GPIO 引脚编号，用于 I2S 主时钟 (MCLK) 信号。此引脚由 TX 和 RX 通道共享。
 * @param bclk   GPIO 引脚编号，用于 I2S 位时钟 (BCLK) 信号。此引脚由 TX 和 RX 通道共享。
 * @param ws     GPIO 引脚编号，用于 I2S 字选择 (WS) 或左右时钟 (LRCK) 信号。此引脚由 TX 和 RX 通道共享。
 * @param dout   GPIO 引脚编号，用于 I2S 数据输出 (DOUT) 信号，专用于 TX 通道。
 * @param din    GPIO 引脚编号，用于 I2S 数据输入 (DIN) 信号，专用于 RX 通道。
 *
 * @note
 * - 必须在调用此函数之前设置好 `input_sample_rate_` 和 `output_sample_rate_` 成员变量，并且它们的值必须相同。
 * - 如果任何 I2S 初始化步骤失败 (例如 `i2s_new_channel`, `i2s_channel_init_std_mode`, `i2s_channel_init_tdm_mode` 返回错误)，
 *   `ESP_ERROR_CHECK` 将会捕获错误并可能导致程序中止或重启。
 * - TX 通道的 `din` GPIO 和 RX 通道的 `dout` GPIO 分别设置为 `I2S_GPIO_UNUSED`，表示这些引脚在各自通道中未使用。
 */
void BoxAudioCodec::CreateDuplexChannels(gpio_num_t mclk, gpio_num_t bclk, gpio_num_t ws, gpio_num_t dout, gpio_num_t din) {
    assert(input_sample_rate_ == output_sample_rate_);

    i2s_chan_config_t chan_cfg = {
        .id = I2S_NUM_0,
        .role = I2S_ROLE_MASTER,
        .dma_desc_num = AUDIO_CODEC_DMA_DESC_NUM,
        .dma_frame_num = AUDIO_CODEC_DMA_FRAME_NUM,
        .auto_clear_after_cb = true,
        .auto_clear_before_cb = false,
        .intr_priority = 0,
    };
    ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, &tx_handle_, &rx_handle_));

    i2s_std_config_t std_cfg = {
        .clk_cfg = {
            .sample_rate_hz = (uint32_t)output_sample_rate_,
            .clk_src = I2S_CLK_SRC_DEFAULT,
            .ext_clk_freq_hz = 0,
            .mclk_multiple = I2S_MCLK_MULTIPLE_256
        },
        .slot_cfg = {
            .data_bit_width = I2S_DATA_BIT_WIDTH_16BIT,
            .slot_bit_width = I2S_SLOT_BIT_WIDTH_AUTO,
            .slot_mode = I2S_SLOT_MODE_STEREO,
            .slot_mask = I2S_STD_SLOT_BOTH,
            .ws_width = I2S_DATA_BIT_WIDTH_16BIT,
            .ws_pol = false,
            .bit_shift = true,
            .left_align = true,
            .big_endian = false,
            .bit_order_lsb = false
        },
        .gpio_cfg = {
            .mclk = mclk,
            .bclk = bclk,
            .ws = ws,
            .dout = dout,
            .din = I2S_GPIO_UNUSED,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv = false
            }
        }
    };

    i2s_tdm_config_t tdm_cfg = {
        .clk_cfg = {
            .sample_rate_hz = (uint32_t)input_sample_rate_,
            .clk_src = I2S_CLK_SRC_DEFAULT,
            .ext_clk_freq_hz = 0,
            .mclk_multiple = I2S_MCLK_MULTIPLE_256,
            .bclk_div = 8,
        },
        .slot_cfg = {
            .data_bit_width = I2S_DATA_BIT_WIDTH_16BIT,
            .slot_bit_width = I2S_SLOT_BIT_WIDTH_AUTO,
            .slot_mode = I2S_SLOT_MODE_STEREO,
            .slot_mask = i2s_tdm_slot_mask_t(I2S_TDM_SLOT0 | I2S_TDM_SLOT1 | I2S_TDM_SLOT2 | I2S_TDM_SLOT3),
            .ws_width = I2S_TDM_AUTO_WS_WIDTH,
            .ws_pol = false,
            .bit_shift = true,
            .left_align = false,
            .big_endian = false,
            .bit_order_lsb = false,
            .skip_mask = false,
            .total_slot = I2S_TDM_AUTO_SLOT_NUM
        },
        .gpio_cfg = {
            .mclk = mclk,
            .bclk = bclk,
            .ws = ws,
            .dout = I2S_GPIO_UNUSED,
            .din = din,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv = false
            }
        }
    };


void BoxAudioCodec::SetOutputVolume(int volume) {
    ESP_ERROR_CHECK(esp_codec_dev_set_out_vol(output_dev_, volume));
    AudioCodec::SetOutputVolume(volume);
}

void BoxAudioCodec::EnableInput(bool enable) {
    if (enable == input_enabled_) {
        return;
    }
    if (enable) {
        esp_codec_dev_sample_info_t fs = {
            .bits_per_sample = 16,
            .channel = 4,
            .channel_mask = ESP_CODEC_DEV_MAKE_CHANNEL_MASK(0),
            .sample_rate = (uint32_t)output_sample_rate_,
            .mclk_multiple = 0,
        };
        if (input_reference_) {
            fs.channel_mask |= ESP_CODEC_DEV_MAKE_CHANNEL_MASK(1);
        }
        ESP_ERROR_CHECK(esp_codec_dev_open(input_dev_, &fs));
        ESP_ERROR_CHECK(esp_codec_dev_set_in_channel_gain(input_dev_, ESP_CODEC_DEV_MAKE_CHANNEL_MASK(0), 40.0));
    } else {
        ESP_ERROR_CHECK(esp_codec_dev_close(input_dev_));
    }
    AudioCodec::EnableInput(enable);
}/**
 * @brief 设置音频编解码器的输出音量。
 *
 * 此函数调用底层 ESP 编解码器开发接口来设置指定输出设备的音量，
 * 然后调用基类 AudioCodec 的 SetOutputVolume 方法。
 *
 * @param volume 要设置的音量级别。具体范围和单位取决于底层编解码器驱动。
 *               通常，较大的值表示较大的音量。
 */
void BoxAudioCodec::SetOutputVolume(int volume) {
    // 调用底层编解码器接口设置输出音量
    ESP_ERROR_CHECK(esp_codec_dev_set_out_vol(output_dev_, volume));
    // 调用基类方法，可能会更新输出音量状态
    AudioCodec::SetOutputVolume(volume);
}

/**
 * @brief 启用或禁用音频编解码器的输入。
 *
 * 如果请求的状态与当前状态相同，则函数直接返回。
 * 启用输入时，会根据配置（如是否使用参考输入 input_reference_）
 * 设置采样信息（位数、通道数、采样率等），然后打开输入设备并设置输入通道增益。
 * 禁用输入时，会关闭输入设备。
 * 最后，此函数会调用基类 AudioCodec 的 EnableInput 方法。
 *
 * @note 输入采样率当前设置为与 output_sample_rate_ 相同。
 * @note 启用输入时，默认配置为16位采样，4个通道（具体使用通道由 channel_mask 控制），
 *       并为通道0设置固定的输入增益。
 *
 * @param enable 如果为 true，则启用输入；如果为 false，则禁用输入。
 */
void BoxAudioCodec::EnableInput(bool enable) {
    if (enable == input_enabled_) { // input_enabled_ 可能是基类成员或此类的成员，用于跟踪当前状态
        return;
    }
    if (enable) {
        esp_codec_dev_sample_info_t fs = {
            .bits_per_sample = 16,
            .channel = 4, // 声明总通道数，实际使用通道由 channel_mask 指定
            .channel_mask = ESP_CODEC_DEV_MAKE_CHANNEL_MASK(0), // 默认启用通道0
            .sample_rate = (uint32_t)output_sample_rate_, // 使用输出采样率作为输入采样率
            .mclk_multiple = 0, // MCLK 倍频，0 表示由驱动自动确定或不使用
        };
        if (input_reference_) { // 如果启用了参考输入（例如用于回声消除）
            fs.channel_mask |= ESP_CODEC_DEV_MAKE_CHANNEL_MASK(1); // 额外启用通道1作为参考
        }
        ESP_ERROR_CHECK(esp_codec_dev_open(input_dev_, &fs));
        // 为主输入通道（通道0）设置增益，例如 40.0 dB 或某个相对值
        ESP_ERROR_CHECK(esp_codec_dev_set_in_channel_gain(input_dev_, ESP_CODEC_DEV_MAKE_CHANNEL_MASK(0), 40.0));
    } else {
        ESP_ERROR_CHECK(esp_codec_dev_close(input_dev_));
    }
    AudioCodec::EnableInput(enable); // 调用基类方法，可能会更新 input_enabled_ 状态
}

/**
 * @brief 启用或禁用音频编解码器的输出。
 *
 * 如果请求的状态与当前输出启用状态 (output_enabled_) 相同，则函数直接返回。
 *
 * 当启用输出时：
 * 1. 配置输出设备的采样信息 (esp_codec_dev_sample_info_t):
 *    - bits_per_sample: 设置为 16 位。
 *    - channel: 设置为 1 (单声道)。
 *    - channel_mask: 设置为 0 (具体含义取决于 ESP_CODEC_DEV API，通常表示默认或单通道)。
 *    - sample_rate: 设置为成员变量 output_sample_rate_ 的值。
 *    - mclk_multiple: 设置为 0 (MCLK 倍频，0 通常表示由驱动自动确定或不使用)。
 * 2. 调用 esp_codec_dev_open() 打开输出设备 (output_dev_) 并应用上述采样信息。
 * 3. 调用 esp_codec_dev_set_out_vol() 设置输出设备的音量为成员变量 output_volume_ 中存储的值。
 *
 * 当禁用输出时：
 * 1. 调用 esp_codec_dev_close() 关闭输出设备 (output_dev_)。
 *
 * 无论启用还是禁用，最后都会调用基类 AudioCodec 的 EnableOutput 方法，
 * 这可能会更新通用的输出启用状态或执行其他与具体硬件无关的逻辑。
 *
 * @note 此函数中的错误检查通过 ESP_ERROR_CHECK 宏完成，如果底层 API 调用失败，
 *       程序可能会中止。
 * @note `output_enabled_` 和 `output_volume_` 是用于跟踪当前状态和音量级别的成员变量。
 *
 * @param enable 如果为 true，则启用输出；如果为 false，则禁用输出。
 */
void BoxAudioCodec::EnableOutput(bool enable) {
    if (enable == output_enabled_) { // output_enabled_ 可能是基类或此类中用于跟踪状态的成员
        return;
    }
    if (enable) {
        // 配置为播放 16位, 单声道音频
        esp_codec_dev_sample_info_t fs = {
            .bits_per_sample = 16,
            .channel = 1,                                 // 单声道输出
            .channel_mask = 0,                            // 通道掩码，具体含义依赖驱动，0 可能表示通道0或默认
            .sample_rate = (uint32_t)output_sample_rate_, // 使用成员变量中存储的输出采样率
            .mclk_multiple = 0,                           // MCLK 倍频，0 通常表示自动或不适用
        };
        ESP_ERROR_CHECK(esp_codec_dev_open(output_dev_, &fs)); // 打开输出设备并配置采样参数
        ESP_ERROR_CHECK(esp_codec_dev_set_out_vol(output_dev_, output_volume_)); // 设置为当前存储的音量
    } else {
        ESP_ERROR_CHECK(esp_codec_dev_close(output_dev_)); // 关闭输出设备
    }
    AudioCodec::EnableOutput(enable); // 调用基类方法，可能会更新 output_enabled_ 状态
}

/**
 * @brief 从音频编解码器的输入设备读取音频样本。
 *
 * 只有当输入 (input_enabled_) 当前被启用时，此函数才会尝试从输入设备 (input_dev_) 读取数据。
 * 它会尝试读取指定数量的 16 位音频样本。
 *
 * 读取操作通过调用底层 `esp_codec_dev_read` 函数完成。
 * 错误通过 `ESP_ERROR_CHECK_WITHOUT_ABORT` 进行检查，这意味着如果发生错误，
 * 错误会被记录，但程序不一定会中止。
 *
 * @param dest 指向用于存储读取到的音频样本的目标缓冲区的指针。
 *             缓冲区必须足够大以容纳 `samples` 个 `int16_t` 类型的样本。
 * @param samples 要读取的 16 位音频样本的数量。
 * @return int 函数总是返回请求读取的样本数量 (`samples`)，
 *             即使在输入未启用或底层读取操作可能遇到问题（但未中止程序）的情况下。
 *             调用者应注意，返回的值不一定代表实际成功读取的样本数，
 *             尤其是在使用 `ESP_ERROR_CHECK_WITHOUT_ABORT` 时。
 *
 * @note 实际读取的字节数是 `samples * sizeof(int16_t)`。
 */
int BoxAudioCodec::Read(int16_t* dest, int samples) {
    if (input_enabled_) { // 检查输入是否已启用
        // 尝试从输入设备读取数据，错误检查但通常不中止程序
        ESP_ERROR_CHECK_WITHOUT_ABORT(esp_codec_dev_read(input_dev_, (void*)dest, samples * sizeof(int16_t)));
    }
    return samples; // 总是返回请求的样本数
}

/**
 * @brief 将音频样本写入音频编解码器的输出设备。
 *
 * 只有当输出 (output_enabled_) 当前被启用时，此函数才会尝试向输出设备 (output_dev_) 写入数据。
 * 它会尝试写入指定数量的 16 位音频样本。
 *
 * 写入操作通过调用底层 `esp_codec_dev_write` 函数完成。
 * 错误通过 `ESP_ERROR_CHECK_WITHOUT_ABORT` 进行检查，这意味着如果发生错误，
 * 错误会被记录，但程序不一定会中止。
 *
 * @param data 指向包含要写入的音频样本的源缓冲区的指针。
 * @param samples 要写入的 16 位音频样本的数量。
 * @return int 函数总是返回请求写入的样本数量 (`samples`)，
 *             即使在输出未启用或底层写入操作可能遇到问题（但未中止程序）的情况下。
 *             调用者应注意，返回的值不一定代表实际成功写入的样本数，
 *             尤其是在使用 `ESP_ERROR_CHECK_WITHOUT_ABORT` 时。
 *
 * @note 实际写入的字节数是 `samples * sizeof(int16_t)`。
 */
int BoxAudioCodec::Write(const int16_t* data, int samples) {
    if (output_enabled_) { // 检查输出是否已启用
        // 尝试向输出设备写入数据，错误检查但通常不中止程序
        ESP_ERROR_CHECK_WITHOUT_ABORT(esp_codec_dev_write(output_dev_, (void*)data, samples * sizeof(int16_t)));
    }
    return samples; // 总是返回请求的样本数
}
