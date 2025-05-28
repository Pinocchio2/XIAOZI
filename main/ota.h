#ifndef _OTA_H
#define _OTA_H

#include <functional>
#include <string>
#include <map>

#include <esp_err.h>
#include "board.h"


/**
 * @brief OTA（Over-The-Air）升级管理类。
 *
 * 该类用于管理设备的固件OTA升级流程，包括检查新版本、下载固件、激活等操作。
 */
class Ota {
public:
    /**
     * @brief 构造函数，初始化OTA对象。
     */
    Ota();

    /**
     * @brief 析构函数，释放OTA对象占用的资源。
     */
    ~Ota();

    /**
     * @brief 设置HTTP请求头字段。
     *
     * @param key HTTP头字段的键。
     * @param value HTTP头字段的值。
     */
    void SetHeader(const std::string& key, const std::string& value);

    /**
     * @brief 检查是否有可用的新版本固件。
     *
     * @return 如果有新版本则返回true，否则返回false。
     */
    bool CheckVersion();

    /**
     * @brief 激活当前下载的固件。
     *
     * @return ESP_OK表示成功，其他值表示错误。
     */
    esp_err_t Activate();

    /**
     * @brief 判断是否存在激活挑战。
     *
     * @return 如果存在激活挑战则返回true，否则返回false。
     */
    bool HasActivationChallenge() { return has_activation_challenge_; }

    /**
     * @brief 检查是否有新版本固件可用。
     *
     * @return 如果有新版本则返回true，否则返回false。
     */
    bool HasNewVersion() { return has_new_version_; }

    /**
     * @brief 检查是否已经配置了MQTT。
     *
     * @return 如果已配置MQTT则返回true，否则返回false。
     */
    bool HasMqttConfig() { return has_mqtt_config_; }

    /**
     * @brief 检查是否有WebSocket配置。
     *
     * @return 如果有WebSocket配置则返回true，否则返回false。
     */
    bool HasWebsocketConfig() { return has_websocket_config_; }

    /**
     * @brief 检查是否有激活码。
     *
     * @return 如果有激活码则返回true，否则返回false。
     */
    bool HasActivationCode() { return has_activation_code_; }

    /**
     * @brief 检查是否有服务器时间。
     *
     * @return 如果有服务器时间则返回true，否则返回false。
     */
    bool HasServerTime() { return has_server_time_; }

    /**
     * @brief 开始固件升级过程。
     *
     * @param callback 升级进度回调函数，参数为进度百分比和下载速度（字节/秒）。
     */
    void StartUpgrade(std::function<void(int progress, size_t speed)> callback);

    /**
     * @brief 标记当前固件版本为有效。
     */
    void MarkCurrentVersionValid();

    /**
     * @brief 获取固件版本号。
     *
     * @return 当前固件版本字符串。
     */
    const std::string& GetFirmwareVersion() const { return firmware_version_; }

    /**
     * @brief 获取当前运行的固件版本。
     *
     * @return 当前固件版本字符串。
     */
    const std::string& GetCurrentVersion() const { return current_version_; }

    /**
     * @brief 获取激活消息内容。
     *
     * @return 激活消息字符串。
     */
    const std::string& GetActivationMessage() const { return activation_message_; }

    /**
     * @brief 获取激活码。
     *
     * @return 激活码字符串。
     */
    const std::string& GetActivationCode() const { return activation_code_; }

    /**
     * @brief 获取检查版本的URL。
     *
     * @return 版本检查URL字符串。
     */
    const std::string& GetCheckVersionUrl() const { return check_version_url_; }

private:
    std::string check_version_url_;           ///< 存储检查版本的URL。
    std::string activation_message_;          ///< 存储激活消息。
    std::string activation_code_;             ///< 存储激活码。
    bool has_new_version_ = false;            ///< 是否有新版本标志。
    bool has_mqtt_config_ = false;            ///< 是否已配置MQTT标志。
    bool has_websocket_config_ = false;       ///< 是否有WebSocket配置标志。
    bool has_server_time_ = false;            ///< 是否有服务器时间标志。
    bool has_activation_code_ = false;        ///< 是否有激活码标志。
    bool has_serial_number_ = false;          ///< 是否有串号标志。
    bool has_activation_challenge_ = false;   ///< 是否有激活挑战标志。
    std::string current_version_;             ///< 当前运行的固件版本。
    std::string firmware_version_;            ///< 新固件版本。
    std::string firmware_url_;                ///< 固件下载URL。
    std::string activation_challenge_;        ///< 激活挑战信息。
    std::string serial_number_;               ///< 设备序列号。
    int activation_timeout_ms_ = 30000;       ///< 激活超时时间（毫秒）。
    std::map<std::string, std::string> headers_; ///< HTTP请求头。

    /**
     * @brief 执行固件升级。
     *
     * @param firmware_url 固件文件的URL。
     */
    void Upgrade(const std::string& firmware_url);

    /**
     * @brief 升级进度回调函数。
     */
    std::function<void(int progress, size_t speed)> upgrade_callback_;

    /**
     * @brief 解析版本号字符串为整数数组。
     *
     * @param version 版本号字符串。
     * @return 包含版本号各部分的整数数组。
     */
    std::vector<int> ParseVersion(const std::string& version);

    /**
     * @brief 比较两个版本号，判断是否有新版本可用。
     *
     * @param currentVersion 当前版本号。
     * @param newVersion 新版本号。
     * @return 如果新版本更新则返回true，否则返回false。
     */
    bool IsNewVersionAvailable(const std::string& currentVersion, const std::string& newVersion);

    /**
     * @brief 生成激活负载数据。
     *
     * @return 激活负载JSON字符串。
     */
    std::string GetActivationPayload();

    /**
     * @brief 设置HTTP客户端实例。
     *
     * @return 配置好的Http指针。
     */
    Http* SetupHttp();
};
#endif // _OTA_H
