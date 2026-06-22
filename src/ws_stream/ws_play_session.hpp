#ifndef WS_PLAY_SESSION_HPP
#define WS_PLAY_SESSION_HPP

#include "net/http/websocket/websocket_session.hpp"
#include "format/flv/flv_demux.hpp"
#include "utils/logger.hpp"
#include <string>
#include <memory>

namespace cpp_streamer {

class WsPlaySession : public WebSocketSessionCallBackI, public AvWriterInterface
{
public:
    WsPlaySession(WebSocketSession* session, Logger* logger);
    virtual ~WsPlaySession();

public:
    bool IsAlive();
    void Clear();
    
public:
    virtual void OnReadData(int code, const uint8_t* data, size_t len) override;
    virtual void OnReadText(int code, const std::string& text) override;
    virtual void OnClose(int code, const std::string& desc) override;

public:
    virtual int WritePacket(Media_Packet_Ptr) override;
    virtual std::string GetKey() override;
    virtual std::string GetWriterId() override;
    virtual void CloseWriter() override;
    virtual bool IsInited() override;
    virtual void SetInitFlag(bool flag) override;

private:
    int SendFlvHeader();
    void WsSendData(const uint8_t* data, size_t len);

private:
    WebSocketSession* session_ = nullptr;
    Logger* logger_ = nullptr;
    std::string stream_name_;

private:
    std::string app_;
    std::string stream_;
    std::string key_;

private:
    bool init_flag_ = false;
    bool flv_header_ready_ = false;
    bool has_audio_ = true;
    bool has_video_ = true;

private:
    int64_t alive_ms_ = -1;
    bool closed_ = false;
};

}
#endif // WS_PLAY_SESSION_HPP