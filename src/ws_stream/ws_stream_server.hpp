#ifndef WS_STREAM_SERVER_HPP
#define WS_STREAM_SERVER_HPP

#include "net/http/websocket/websocket_server.hpp"
#include "utils/logger.hpp"
#include "utils/timer.hpp"
#include <memory>
#include <string>
#include <stdint.h>
#include <map>

namespace cpp_streamer {

class WsPublishSession;
class WsPlaySession;
class WebSocketSession;

class WsStreamServer : public TimerInterface
{
	friend void OnWSStreamPublish(const std::string& uri, WebSocketSession* session);
    friend void OnWSStreamPlay(const std::string& uri, WebSocketSession* session);

public:
    WsStreamServer(const std::string& ip, uint16_t port, uv_loop_t* loop, Logger* logger);
    WsStreamServer(const std::string& ip, uint16_t port, uv_loop_t* loop, 
        const std::string& key_file, 
        const std::string& cert_file, 
        Logger* logger);
    virtual ~WsStreamServer();

protected:
    virtual bool OnTimer() override;
    
private:
    Logger* logger_ = nullptr;
    std::unique_ptr<WebSocketServer> ws_server_ptr_ = nullptr;

private:
    static std::map<std::string, std::shared_ptr<WsPublishSession>> ws_publish_sessions;
    static std::map<std::string, std::shared_ptr<WsPlaySession>> ws_play_sessions;
};

} // namespace cpp_streamer
#endif // WS_STREAM_SERVER_HPP