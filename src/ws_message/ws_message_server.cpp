#include "ws_message_server.hpp"
#include "ws_message_session.hpp"
#include "net/http/websocket/websocket_session.hpp"
#include "webrtc_room/room_mgr.hpp"
#include "utils/uuid.hpp"
#include <map>

namespace cpp_streamer {

std::map<std::string, std::shared_ptr<WsMessageSession>> WsMessageServer::ws_message_sessions;

void OnWSMessageSessionHandle(const std::string& uri, WebSocketSession* session) {
    auto session_ptr = std::make_shared<WsMessageSession>(session,
        &RoomMgr::Instance(session->UvLoop(), session->GetLogger()),
        session->GetLogger());

    //need to add protocol in http header to support protoo
    session->AddHeader("Sec-WebSocket-Protocol", "protoo");
    WsMessageServer::ws_message_sessions[session->GetRemoteAddress()] = session_ptr;
}

WsMessageServer::WsMessageServer(const std::string& ip,
    uint16_t port, uv_loop_t* loop, Logger* logger) : TimerInterface(15*1000)
{
    logger_ = logger;
    ws_server_ptr_.reset(new WebSocketServer(ip, port, loop, logger));
    ws_server_ptr_->AddHandle("/webrtc", OnWSMessageSessionHandle);

    StartTimer();
}

WsMessageServer::WsMessageServer(const std::string& ip, uint16_t port, uv_loop_t* loop,
    const std::string& key_file, const std::string& cert_file, Logger* logger) : TimerInterface(500)
{
    logger_ = logger;
    ws_server_ptr_.reset(new WebSocketServer(ip, port, loop, key_file, cert_file, logger));
    ws_server_ptr_->AddHandle("/webrtc", OnWSMessageSessionHandle);

    StartTimer();
}

WsMessageServer::~WsMessageServer()
{
}

bool WsMessageServer::OnTimer() {
    //clean up closed sessions
    for (auto iter = ws_message_sessions.begin(); iter != ws_message_sessions.end(); ) {
        std::shared_ptr<WsMessageSession> session_ptr = iter->second;
        if (!session_ptr->IsAlive()) {
            LogInfof(logger_, "remove closed ws message session, addr:%s", iter->first.c_str());
            iter->second->Clear();
            iter = ws_message_sessions.erase(iter);
            continue;
        }
        ++iter;
    }
    return timer_running_;
}

} // namespace cpp_streamer

