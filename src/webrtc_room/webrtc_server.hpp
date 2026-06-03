#ifndef WEBRTC_SERVER_HPP
#define WEBRTC_SERVER_HPP
#include "utils/logger.hpp"
#include "utils/timer.hpp"
#include "config/config.hpp"
#include "net/udp/udp_server.hpp"
#include "udp_transport.hpp"

#include <map>
#include <vector>

namespace cpp_streamer {

class WebRtcSession;
class WebRtcServer : public UdpSessionCallbackI, public TimerInterface, public UdpTransportI
{
public:
    WebRtcServer(uv_loop_t* loop, Logger* logger, const RtcCandidate& config);
    virtual ~WebRtcServer();

public:
    virtual void OnWriteUdpData(const uint8_t* data, size_t sent_size, UdpTuple address) override;
    
public:
    static void SetUserName2Session(const std::string& username, std::shared_ptr<WebRtcSession> session);
    static void SetAddr2Session(uint64_t addr_u64, std::shared_ptr<WebRtcSession> session);
    static void RemoveSessionByRoomId(const std::string& room_id);
protected:
    virtual void OnWrite(size_t sent_size, UdpTuple address) override;
    virtual void OnRead(const char* data, size_t data_size, UdpTuple address) override;
protected:
    virtual bool OnTimer() override;
    
private:
    void HandleStunPacket(const uint8_t* data, size_t len, UdpTuple addr);
    void HandleNoneStunPacket(const uint8_t* data, size_t len, UdpTuple addr);
    std::string GetKeyByUsername(const std::string& username);
private:
    uv_loop_t* loop_ = nullptr;
    Logger* logger_ = nullptr;
    RtcCandidate rtc_candidate_;
    std::unique_ptr<UdpServer> udp_server_;

private:
    static std::unordered_map<std::string, std::shared_ptr<WebRtcSession>> username2sessions_;//ice_username ->session
    static std::unordered_map<uint64_t, std::shared_ptr<WebRtcSession>> addr2sessions_;//address ->session
};

} // namespace cpp_streamer

#endif // WEBRTC_SERVER_HPP