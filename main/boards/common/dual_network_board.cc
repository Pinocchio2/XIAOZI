#include "dual_network_board.h"
#include "application.h"
#include "display.h"
#include "assets/lang_config.h"
#include "settings.h"
#include <esp_log.h>

static const char *TAG = "DualNetworkBoard";

// 构造函数，初始化ml307_tx_pin_、ml307_rx_pin_和ml307_rx_buffer_size_，并从Settings加载网络类型，初始化当前网络类型对应的板卡
DualNetworkBoard::DualNetworkBoard(gpio_num_t ml307_tx_pin, gpio_num_t ml307_rx_pin, size_t ml307_rx_buffer_size) 
    : Board(), 
      ml307_tx_pin_(ml307_tx_pin), 
      ml307_rx_pin_(ml307_rx_pin), 
      ml307_rx_buffer_size_(ml307_rx_buffer_size) {
    
    // 从Settings加载网络类型
    network_type_ = LoadNetworkTypeFromSettings();
    
    // 只初始化当前网络类型对应的板卡
    InitializeCurrentBoard();
}

NetworkType DualNetworkBoard::LoadNetworkTypeFromSettings() {
    // 从设置中加载网络类型
    Settings settings("network", true);
    int network_type = settings.GetInt("type", 1); // 默认使用ML307 (1)
    // 如果网络类型为1，则返回ML307，否则返回WIFI
    return network_type == 1 ? NetworkType::ML307 : NetworkType::WIFI;
}

// 将网络类型保存到设置中
void DualNetworkBoard::SaveNetworkTypeToSettings(NetworkType type) {
    // 创建一个设置对象，参数为设置名称和是否创建新设置
    Settings settings("network", true);
    // 根据网络类型设置网络类型值，ML307为1，其他为0
    int network_type = (type == NetworkType::ML307) ? 1 : 0;
    // 将网络类型值保存到设置中
    settings.SetInt("type", network_type);
}

void DualNetworkBoard::InitializeCurrentBoard() {
    // 判断网络类型
    if (network_type_ == NetworkType::ML307) {
        // 如果网络类型为ML307，则初始化ML307板
        ESP_LOGI(TAG, "Initialize ML307 board");
        current_board_ = std::make_unique<Ml307Board>(ml307_tx_pin_, ml307_rx_pin_, ml307_rx_buffer_size_);
    } else {
        // 否则，初始化WiFi板
        ESP_LOGI(TAG, "Initialize WiFi board");
        current_board_ = std::make_unique<WifiBoard>();
    }
}

void DualNetworkBoard::SwitchNetworkType() {
    // 获取显示对象
    auto display = GetDisplay();
    // 如果当前网络类型为WIFI
    if (network_type_ == NetworkType::WIFI) {    
        // 将网络类型保存到设置中，改为ML307
        SaveNetworkTypeToSettings(NetworkType::ML307);
        // 显示通知，切换到4G网络
        display->ShowNotification(Lang::Strings::SWITCH_TO_4G_NETWORK);
    } else {
        // 将网络类型保存到设置中，改为WIFI
        SaveNetworkTypeToSettings(NetworkType::WIFI);
        // 显示通知，切换到WIFI网络
        display->ShowNotification(Lang::Strings::SWITCH_TO_WIFI_NETWORK);
    }
    // 延时1秒
    vTaskDelay(pdMS_TO_TICKS(1000));
    // 获取应用程序实例
    auto& app = Application::GetInstance();
    // 重启应用程序
    app.Reboot();
}

 
// 获取当前棋盘类型
std::string DualNetworkBoard::GetBoardType() {
    // 调用当前棋盘对象的GetBoardType()方法，获取棋盘类型
    return current_board_->GetBoardType();
}

void DualNetworkBoard::StartNetwork() {
    auto display = Board::GetInstance().GetDisplay();
    
    if (network_type_ == NetworkType::WIFI) {
        display->SetStatus(Lang::Strings::CONNECTING);
    } else {
        display->SetStatus(Lang::Strings::DETECTING_MODULE);
    }
    current_board_->StartNetwork();
}

Http* DualNetworkBoard::CreateHttp() {
    return current_board_->CreateHttp();
}

WebSocket* DualNetworkBoard::CreateWebSocket() {
    return current_board_->CreateWebSocket();
}

Mqtt* DualNetworkBoard::CreateMqtt() {
    return current_board_->CreateMqtt();
}

Udp* DualNetworkBoard::CreateUdp() {
    return current_board_->CreateUdp();
}

const char* DualNetworkBoard::GetNetworkStateIcon() {
    return current_board_->GetNetworkStateIcon();
}

void DualNetworkBoard::SetPowerSaveMode(bool enabled) {
    current_board_->SetPowerSaveMode(enabled);
}

// 获取当前棋盘的JSON表示
std::string DualNetworkBoard::GetBoardJson() {   
    // 调用current_board_的GetBoardJson()方法，获取当前棋盘的JSON表示
    return current_board_->GetBoardJson();
} 