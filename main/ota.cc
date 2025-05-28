#include "ota.h"
#include "system_info.h"
#include "settings.h"
#include "assets/lang_config.h"

#include <cJSON.h>
#include <esp_log.h>
#include <esp_partition.h>
#include <esp_ota_ops.h>
#include <esp_app_format.h>
#include <esp_efuse.h>
#include <esp_efuse_table.h>
#ifdef SOC_HMAC_SUPPORTED
#include <esp_hmac.h>
#endif

#include <cstring>
#include <vector>
#include <sstream>
#include <algorithm>

#define TAG "Ota"
  //
// Ota类构造函数
// 功能：初始化Ota对象，读取OTA配置信息和设备序列号
// 注意事项：该构造函数会优先从设置中读取OTA URL，若未设置则使用默认值
//          在支持EFUSE的平台上会尝试从用户数据区读取序列号
//
Ota::Ota() {
    {
        Settings settings("wifi", false);
        check_version_url_ = settings.GetString("ota_url");
        if (check_version_url_.empty()) {
            check_version_url_ = CONFIG_OTA_URL;
        }
    }


#ifdef ESP_EFUSE_BLOCK_USR_DATA
    // 从efuse用户数据区读取序列号
    // 初始化33字节的缓冲区用于存储序列号（32字节有效数据+1个终止符）
    uint8_t serial_number[33] = {0};
    // 尝试读取efuse中的用户数据
    if (esp_efuse_read_field_blob(ESP_EFUSE_USER_DATA, serial_number, 32 * 8) == ESP_OK) {
        if (serial_number[0] == 0) {
            has_serial_number_ = false;
        } else {
            // 将二进制序列号转换为字符串格式并设置标志位
            serial_number_ = std::string(reinterpret_cast<char*>(serial_number), 32);
            has_serial_number_ = true;
        }
    }
#endif
}

Ota::~Ota() {
}

void Ota::SetHeader(const std::string& key, const std::string& value) {
    headers_[key] = value;
}


/**
 * 设置HTTP对象以进行OTA更新
 * 
 * 此函数负责初始化和配置HTTP客户端，用于执行OTA（Over-The-Air）更新它根据设备的具体信息和应用程序的描述设置必要的HTTP头信息
 * 
 * @return 返回配置好的HTTP对象指针，用于执行OTA更新操作
 */
Http* Ota::SetupHttp() {
    // 获取单板对象实例
    auto& board = Board::GetInstance();
    // 获取应用程序描述信息
    auto app_desc = esp_app_get_description();

    // 创建HTTP对象
    auto http = board.CreateHttp();
    // 设置HTTP请求头信息，遍历预定义的头部信息并设置到HTTP对象中
    for (const auto& header : headers_) {
        http->SetHeader(header.first, header.second);
    }

    // 根据设备是否有序列号设置激活版本
    http->SetHeader("Activation-Version", has_serial_number_ ? "2" : "1");
    // 设置设备ID为设备的MAC地址
    http->SetHeader("Device-Id", SystemInfo::GetMacAddress().c_str());
    // 设置客户端ID为单板的UUID
    http->SetHeader("Client-Id", board.GetUuid());
    // 设置用户代理为板子名称和应用程序版本
    http->SetHeader("User-Agent", std::string(BOARD_NAME "/") + app_desc->version);
    // 设置接受语言为系统预设的语言代码
    http->SetHeader("Accept-Language", Lang::CODE);
    // 设置内容类型为JSON格式
    http->SetHeader("Content-Type", "application/json");

    // 返回配置好的HTTP对象
    return http;
}

/**
 * 检查是否有新的固件版本可用
 * 通过HTTP请求版本检查URL获取最新版本信息，并与当前版本进行比较
 * 
 * @return bool 成功解析响应并完成版本检查返回true，否则返回false
 */
bool Ota::CheckVersion() {
    auto& board = Board::GetInstance();
    auto app_desc = esp_app_get_description();

    /**
     * 获取并记录当前固件版本
     * 使用esp_app_get_description()获取应用程序描述信息
     * 记录日志：当前固件版本
     */
    current_version_ = app_desc->version;
    ESP_LOGI(TAG, "Current version: %s", current_version_.c_str());

    if (check_version_url_.length() < 10) {
        ESP_LOGE(TAG, "Check version URL is not properly set");
        return false;
    }

    auto http = SetupHttp();

    /**
     * 发送HTTP请求获取版本信息
     * 根据是否有数据体选择POST或GET方法
     * 失败时记录错误日志并清理资源
     */
    std::string data = board.GetJson();
    std::string method = data.length() > 0 ? "POST" : "GET";
    if (!http->Open(method, check_version_url_, data)) {
        ESP_LOGE(TAG, "Failed to open HTTP connection");
        delete http;
        return false;
    }

    data = http->GetBody();
    delete http;

    // Response: { "firmware": { "version": "1.0.0", "url": "http://" } }
    // Parse the JSON response and check if the version is newer
    // If it is, set has_new_version_ to true and store the new version and URL
    
    cJSON *root = cJSON_Parse(data.c_str());
    if (root == NULL) {
        ESP_LOGE(TAG, "Failed to parse JSON response");
        return false;
    }

    has_activation_code_ = false;
    has_activation_challenge_ = false;
    /**
     * 解析JSON响应中的激活信息
     * 提取message、code和challenge字段（如果存在）
     * 设置对应的成员变量和标志位
     */
    cJSON *activation = cJSON_GetObjectItem(root, "activation");
    if (activation != NULL) {
        cJSON* message = cJSON_GetObjectItem(activation, "message");
        if (message != NULL) {
            activation_message_ = message->valuestring;
        }
        cJSON* code = cJSON_GetObjectItem(activation, "code");
        if (code != NULL) {
            activation_code_ = code->valuestring;
            has_activation_code_ = true;
        }
        cJSON* challenge = cJSON_GetObjectItem(activation, "challenge");
        if (challenge != NULL) {
            activation_challenge_ = challenge->valuestring;
            has_activation_challenge_ = true;
        }
        cJSON* timeout_ms = cJSON_GetObjectItem(activation, "timeout_ms");
        if (timeout_ms != NULL) {
            activation_timeout_ms_ = timeout_ms->valueint;
        }
    }

    has_mqtt_config_ = false;
    /**
     * 解析JSON响应中的MQTT配置信息
     * 如果存在，使用Settings类保存配置值
     * 设置has_mqtt_config_标志位为true
     */
    cJSON *mqtt = cJSON_GetObjectItem(root, "mqtt");
    if (mqtt != NULL) {
        Settings settings("mqtt", true);
        cJSON *item = NULL;
        cJSON_ArrayForEach(item, mqtt) {
            if (item->type == cJSON_String) {
                if (settings.GetString(item->string) != item->valuestring) {
                    settings.SetString(item->string, item->valuestring);
                }
            }
        }
        has_mqtt_config_ = true;
    } else {
        ESP_LOGI(TAG, "No mqtt section found !");
    }

    has_websocket_config_ = false;
    /**
     * 解析JSON响应中的WebSocket配置信息
     * 如果存在，使用Settings类保存配置值
     * 支持字符串和数字类型
     * 设置has_websocket_config_标志位为true
     */
    cJSON *websocket = cJSON_GetObjectItem(root, "websocket");
    if (websocket != NULL) {
        Settings settings("websocket", true);
        cJSON *item = NULL;
        cJSON_ArrayForEach(item, websocket) {
            if (item->type == cJSON_String) {
                settings.SetString(item->string, item->valuestring);
            } else if (item->type == cJSON_Number) {
                settings.SetInt(item->string, item->valueint);
            }
        }
        has_websocket_config_ = true;
    } else {
        ESP_LOGI(TAG, "No websocket section found!");
    }

    has_server_time_ = false;
    /**
     * 解析JSON响应中的服务器时间信息
     * 如果存在timestamp字段，设置系统时间
     * 如果有时区偏移，进行相应的时间调整
     */
    cJSON *server_time = cJSON_GetObjectItem(root, "server_time");
    if (server_time != NULL) {
        cJSON *timestamp = cJSON_GetObjectItem(server_time, "timestamp");
        cJSON *timezone_offset = cJSON_GetObjectItem(server_time, "timezone_offset");
        
        if (timestamp != NULL) {
            // 设置系统时间
            struct timeval tv;
            double ts = timestamp->valuedouble;
            
            // 如果有时区偏移，计算本地时间
            if (timezone_offset != NULL) {
                ts += (timezone_offset->valueint * 60 * 1000); // 转换分钟为毫秒
            }
            
            tv.tv_sec = (time_t)(ts / 1000);  // 转换毫秒为秒
            tv.tv_usec = (suseconds_t)((long long)ts % 1000) * 1000;  // 剩余的毫秒转换为微秒
            settimeofday(&tv, NULL);
            has_server_time_ = true;
        }
    } else {
        ESP_LOGW(TAG, "No server_time section found!");
    }

    has_new_version_ = false;
    /**
     * 解析JSON响应中的固件版本信息
     * 提取version和url字段
     * 比较当前版本和新版本，确定是否有可用更新
     * 如果有force标志且为1，则强制标记为有新版本
     */
    cJSON *firmware = cJSON_GetObjectItem(root, "firmware");
    if (firmware != NULL) {
        cJSON *version = cJSON_GetObjectItem(firmware, "version");
        if (version != NULL) {
            firmware_version_ = version->valuestring;
        }
        cJSON *url = cJSON_GetObjectItem(firmware, "url");
        if (url != NULL) {
            firmware_url_ = url->valuestring;
        }

        if (version != NULL && url != NULL) {
            // Check if the version is newer, for example, 0.1.0 is newer than 0.0.1
            has_new_version_ = IsNewVersionAvailable(current_version_, firmware_version_);
            if (has_new_version_) {
                ESP_LOGI(TAG, "New version available: %s", firmware_version_.c_str());
            } else {
                ESP_LOGI(TAG, "Current is the latest version");
            }
            // If the force flag is set to 1, the given version is forced to be installed
            cJSON *force = cJSON_GetObjectItem(firmware, "force");
            if (force != NULL && force->valueint == 1) {
                has_new_version_ = true;
            }
        }
    } else {
        ESP_LOGW(TAG, "No firmware section found!");
    }

    cJSON_Delete(root);
    return true;
}

void Ota::MarkCurrentVersionValid() {
    auto partition = esp_ota_get_running_partition();
    if (strcmp(partition->label, "factory") == 0) {
        ESP_LOGI(TAG, "Running from factory partition, skipping");
        return;
    }

    ESP_LOGI(TAG, "Running partition: %s", partition->label);
    esp_ota_img_states_t state;
    if (esp_ota_get_state_partition(partition, &state) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get state of partition");
        return;
    }

    if (state == ESP_OTA_IMG_PENDING_VERIFY) {
        ESP_LOGI(TAG, "Marking firmware as valid");
        esp_ota_mark_app_valid_cancel_rollback();
    }
}

void Ota::Upgrade(const std::string& firmware_url) {
    ESP_LOGI(TAG, "Upgrading firmware from %s", firmware_url.c_str());
    esp_ota_handle_t update_handle = 0;
    auto update_partition = esp_ota_get_next_update_partition(NULL);
    if (update_partition == NULL) {
        ESP_LOGE(TAG, "Failed to get update partition");
        return;
    }

    ESP_LOGI(TAG, "Writing to partition %s at offset 0x%lx", update_partition->label, update_partition->address);
    bool image_header_checked = false;
    std::string image_header;

    auto http = Board::GetInstance().CreateHttp();
    if (!http->Open("GET", firmware_url)) {
        ESP_LOGE(TAG, "Failed to open HTTP connection");
        delete http;
        return;
    }

    size_t content_length = http->GetBodyLength();
    if (content_length == 0) {
        ESP_LOGE(TAG, "Failed to get content length");
        delete http;
        return;
    }

    char buffer[512];
    size_t total_read = 0, recent_read = 0;
    auto last_calc_time = esp_timer_get_time();
    while (true) {
        int ret = http->Read(buffer, sizeof(buffer));
        if (ret < 0) {
            ESP_LOGE(TAG, "Failed to read HTTP data: %s", esp_err_to_name(ret));
            delete http;
            return;
        }

        // Calculate speed and progress every second
        recent_read += ret;
        total_read += ret;
        if (esp_timer_get_time() - last_calc_time >= 1000000 || ret == 0) {
            size_t progress = total_read * 100 / content_length;
            ESP_LOGI(TAG, "Progress: %zu%% (%zu/%zu), Speed: %zuB/s", progress, total_read, content_length, recent_read);
            if (upgrade_callback_) {
                upgrade_callback_(progress, recent_read);
            }
            last_calc_time = esp_timer_get_time();
            recent_read = 0;
        }

        if (ret == 0) {
            break;
        }

        if (!image_header_checked) {
            image_header.append(buffer, ret);
            if (image_header.size() >= sizeof(esp_image_header_t) + sizeof(esp_image_segment_header_t) + sizeof(esp_app_desc_t)) {
                esp_app_desc_t new_app_info;
                memcpy(&new_app_info, image_header.data() + sizeof(esp_image_header_t) + sizeof(esp_image_segment_header_t), sizeof(esp_app_desc_t));
                ESP_LOGI(TAG, "New firmware version: %s", new_app_info.version);

                auto current_version = esp_app_get_description()->version;
                if (memcmp(new_app_info.version, current_version, sizeof(new_app_info.version)) == 0) {
                    ESP_LOGE(TAG, "Firmware version is the same, skipping upgrade");
                    delete http;
                    return;
                }

                if (esp_ota_begin(update_partition, OTA_WITH_SEQUENTIAL_WRITES, &update_handle)) {
                    esp_ota_abort(update_handle);
                    delete http;
                    ESP_LOGE(TAG, "Failed to begin OTA");
                    return;
                }

                image_header_checked = true;
                std::string().swap(image_header);
            }
        }
        auto err = esp_ota_write(update_handle, buffer, ret);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to write OTA data: %s", esp_err_to_name(err));
            esp_ota_abort(update_handle);
            delete http;
            return;
        }
    }
    delete http;

    esp_err_t err = esp_ota_end(update_handle);
    if (err != ESP_OK) {
        if (err == ESP_ERR_OTA_VALIDATE_FAILED) {
            ESP_LOGE(TAG, "Image validation failed, image is corrupted");
        } else {
            ESP_LOGE(TAG, "Failed to end OTA: %s", esp_err_to_name(err));
        }
        return;
    }

    err = esp_ota_set_boot_partition(update_partition);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set boot partition: %s", esp_err_to_name(err));
        return;
    }

    ESP_LOGI(TAG, "Firmware upgrade successful, rebooting in 3 seconds...");
    vTaskDelay(pdMS_TO_TICKS(3000));
    esp_restart();
}

void Ota::StartUpgrade(std::function<void(int progress, size_t speed)> callback) {
    upgrade_callback_ = callback;
    Upgrade(firmware_url_);
}

std::vector<int> Ota::ParseVersion(const std::string& version) {
    std::vector<int> versionNumbers;
    std::stringstream ss(version);
    std::string segment;
    
    while (std::getline(ss, segment, '.')) {
        versionNumbers.push_back(std::stoi(segment));
    }
    
    return versionNumbers;
}

bool Ota::IsNewVersionAvailable(const std::string& currentVersion, const std::string& newVersion) {
    std::vector<int> current = ParseVersion(currentVersion);
    std::vector<int> newer = ParseVersion(newVersion);
    
    for (size_t i = 0; i < std::min(current.size(), newer.size()); ++i) {
        if (newer[i] > current[i]) {
            return true;
        } else if (newer[i] < current[i]) {
            return false;
        }
    }
    
    return newer.size() > current.size();
}

std::string Ota::GetActivationPayload() {
    if (!has_serial_number_) {
        return "{}";
    }

    std::string hmac_hex;
#ifdef SOC_HMAC_SUPPORTED
    uint8_t hmac_result[32]; // SHA-256 输出为32字节
    
    // 使用Key0计算HMAC
    esp_err_t ret = esp_hmac_calculate(HMAC_KEY0, (uint8_t*)activation_challenge_.data(), activation_challenge_.size(), hmac_result);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "HMAC calculation failed: %s", esp_err_to_name(ret));
        return "{}";
    }

    for (size_t i = 0; i < sizeof(hmac_result); i++) {
        char buffer[3];
        sprintf(buffer, "%02x", hmac_result[i]);
        hmac_hex += buffer;
    }
#endif

    cJSON *payload = cJSON_CreateObject();
    cJSON_AddStringToObject(payload, "algorithm", "hmac-sha256");
    cJSON_AddStringToObject(payload, "serial_number", serial_number_.c_str());
    cJSON_AddStringToObject(payload, "challenge", activation_challenge_.c_str());
    cJSON_AddStringToObject(payload, "hmac", hmac_hex.c_str());
    std::string json = cJSON_Print(payload);
    cJSON_Delete(payload);

    ESP_LOGI(TAG, "Activation payload: %s", json.c_str());
    return json;
}

esp_err_t Ota::Activate() {
    if (!has_activation_challenge_) {
        ESP_LOGW(TAG, "No activation challenge found");
        return ESP_FAIL;
    }

    std::string url = check_version_url_;
    if (url.back() != '/') {
        url += "/activate";
    } else {
        url += "activate";
    }

    auto http = SetupHttp();

    std::string data = GetActivationPayload();
    if (!http->Open("POST", url, data)) {
        ESP_LOGE(TAG, "Failed to open HTTP connection");
        delete http;
        return ESP_FAIL;
    }
    
    auto status_code = http->GetStatusCode();
    data = http->GetBody();
    http->Close();
    delete http;

    if (status_code == 202) {
        return ESP_ERR_TIMEOUT;
    }
    if (status_code != 200) {
        ESP_LOGE(TAG, "Failed to activate, code: %d, body: %s", status_code, data.c_str());
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Activation successful");
    return ESP_OK;
}
