#ifndef BOARD_H
#define BOARD_H

#include <http.h>
#include <web_socket.h>
#include <mqtt.h>
#include <udp.h>
#include <string>

#include "led/led.h"
#include "backlight.h"

void* create_board();
class AudioCodec;
class Display;
class Board {
private:
    Board(const Board&) = delete; // 禁用拷贝构造函数
    Board& operator=(const Board&) = delete; // 禁用赋值操作

protected:
    Board();
    std::string GenerateUuid();

    // 软件生成的设备唯一标识
    std::string uuid_;

public:
    // 获取单例实例
    static Board& GetInstance() {
        // 静态局部变量，在第一次调用时初始化
        static Board* instance = static_cast<Board*>(create_board());
        // 返回单例实例
        return *instance;
    }

    // 析构函数，虚函数，默认实现
    virtual ~Board() = default;
    // 获取板子类型，纯虚函数
    virtual std::string GetBoardType() = 0;
    // 获取UUID，虚函数
    virtual std::string GetUuid() { return uuid_; }
    // 获取背光，虚函数，默认实现
    virtual Backlight* GetBacklight() { return nullptr; }
    // 获取LED，虚函数
    virtual Led* GetLed();
    // 获取音频编解码器，纯虚函数
    virtual AudioCodec* GetAudioCodec() = 0;
    // 获取温度，虚函数
    virtual bool GetTemperature(float& esp32temp);
    // 获取显示，虚函数
    virtual Display* GetDisplay();
    // 创建HTTP，纯虚函数
    virtual Http* CreateHttp() = 0;
    // 创建WebSocket，纯虚函数
    virtual WebSocket* CreateWebSocket() = 0;
    // 创建Mqtt，纯虚函数
    virtual Mqtt* CreateMqtt() = 0;
    // 创建Udp，纯虚函数
    virtual Udp* CreateUdp() = 0;
    // 启动网络，纯虚函数
    virtual void StartNetwork() = 0;
    // 获取网络状态图标，纯虚函数
    virtual const char* GetNetworkStateIcon() = 0;
    // 获取电池电量，虚函数
    virtual bool GetBatteryLevel(int &level, bool& charging, bool& discharging);
    // 获取Json，虚函数
    virtual std::string GetJson();
    // 设置省电模式，纯虚函数
    virtual void SetPowerSaveMode(bool enabled) = 0;
    // 获取板子Json，虚函数
    virtual std::string GetBoardJson() = 0;
};

#define DECLARE_BOARD(BOARD_CLASS_NAME) \
void* create_board() { \
    return new BOARD_CLASS_NAME(); \
}

#endif // BOARD_H
