#ifndef FAST_JITTERBUFFER_HPP
#define FAST_JITTERBUFFER_HPP
#include "net/rtprtcp/rtp_packet.hpp"
#include "utils/logger.hpp"
#include "utils/timer.hpp"
#include <uv.h>
#include <vector>
#include <map>
#include <queue>

namespace cpp_streamer
{
class JitterBufferCallbackI
{
public:
    virtual void OnJitterBufferRtpPacket(std::vector<RtpPacket*> rtp_packets, bool discard) = 0;
};

class FastJitterBuffer : public TimerInterface
{
public:
    FastJitterBuffer(JitterBufferCallbackI* callback, 
        uv_loop_t* loop,
        Logger* logger,
        int max_delay_ms = 200, 
        int clock_rate = 90000);
    ~FastJitterBuffer();

    void PushRtpPacket(RtpPacket* rtp_packet);
    void Close();

protected:// TimerInterface implementation
    virtual bool OnTimer() override;

private:
    Logger* logger_;
    JitterBufferCallbackI* callback_;
    int max_delay_ms_ = 600;
    int clock_rate_ = 90000;

    // Jitter buffer state
    uint16_t next_expected_seq_ = 0;
    bool has_received_first_packet_ = false;
    std::map<uint16_t, RtpPacket*> packet_buffer_;
    bool closed_ = false;
};
}

#endif