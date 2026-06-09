#include "fast_jitterbuffer.hpp"
#include "utils/timeex.hpp"
#include <chrono>

namespace cpp_streamer
{
FastJitterBuffer::FastJitterBuffer(JitterBufferCallbackI* callback,
    uv_loop_t* loop,
    Logger* logger,
    int max_delay_ms,
    int clock_rate)
    : TimerInterface(20),
      logger_(logger),
      callback_(callback)
{
    max_delay_ms_ = max_delay_ms;
    clock_rate_ = clock_rate;
}

FastJitterBuffer::~FastJitterBuffer() {
    Close();
}

void FastJitterBuffer::Close() {
    if (closed_) {
        return;
    }
    closed_ = true;
    StopTimer();

    // 清理缓存中的包
    for (auto it = packet_buffer_.begin(); it != packet_buffer_.end(); ) {
        RtpPacket* pkt = it->second;
        delete pkt;
        it = packet_buffer_.erase(it);
    }
}

bool FastJitterBuffer::OnTimer()
{
    uint64_t current_time_ms = now_millisec();
    
    // 检查缓存中的包是否超时（延时大于 max_delay_ms_）
    std::vector<RtpPacket*> timeout_packets;
    for (auto it = packet_buffer_.begin(); it != packet_buffer_.end(); ) {
        RtpPacket* pkt = it->second;
        int64_t pkt_time_ms = pkt->GetLocalMs();
        if (current_time_ms - pkt_time_ms >= (uint64_t)max_delay_ms_) {
            timeout_packets.push_back(pkt);
            it = packet_buffer_.erase(it);
            next_expected_seq_ = pkt->GetSeq() + 1;
        } else {
            break;
        }
    }
    
    // 通知上层超时的包
    if (!timeout_packets.empty()) {
        LogInfof(logger_, "FastJitterBuffer::OnTimer: {} packets timeout, exceed {}ms",
                        timeout_packets.size(), max_delay_ms_);
        if (callback_) {
            callback_->OnJitterBufferRtpPacket(timeout_packets, true);
        }
    }
    return true;
}

void FastJitterBuffer::PushRtpPacket(RtpPacket* rtp_packet) {
    if (!rtp_packet || !callback_) {
        LogError(logger_,"FastJitterBuffer::PushRtpPacket: invalid packet or callback");
        return;
    }

    uint16_t seq = rtp_packet->GetSeq();
    
    // 第一个包：初始化期望序列号
    if (!has_received_first_packet_) {
        has_received_first_packet_ = true;
        next_expected_seq_ = seq + 1;
        
        // 直接送出第一个包
        std::vector<RtpPacket*> packets;
        packets.push_back(rtp_packet);
        callback_->OnJitterBufferRtpPacket(packets, false);
        
        LogInfof(logger_, "FastJitterBuffer: received first packet with seq=%d", seq);
        return;
    }
    
    auto ret = CompareSeq(next_expected_seq_, seq);

    if (ret == REPEAT_SEQ) {
        LogInfof(logger_, "FastJitterBuffer: duplicate packet seq=%d, ignored", seq);
        return;
    } else if (ret == REVERSE_SEQ || ret == JUMP_LARGE_SEQ) {
        // pop the packet from buffer
        LogInfof(logger_, "FastJitterBuffer: change type:%d packet seq=%d, expected=%d, flush buffer",
                        ret, seq, next_expected_seq_);
        std::vector<RtpPacket*> pop_packets;
        for (auto it = packet_buffer_.begin(); it != packet_buffer_.end(); ) {
            pop_packets.push_back(it->second);
            it = packet_buffer_.erase(it);
        }
        pop_packets.push_back(rtp_packet);
        if (callback_) {
            callback_->OnJitterBufferRtpPacket(pop_packets, false);
        }
        next_expected_seq_ = seq + 1;
        return;
    } else if (ret == DISCORD_SEQ) {
        //discard the disordered packet
        LogInfof(logger_, "FastJitterBuffer: discord seq packet seq=%d, expected=%d",
                        seq, next_expected_seq_);
        return;
    } else if (ret == NORMAL_SEQ) {
        std::vector<RtpPacket*> packets;
        packets.push_back(rtp_packet);
        next_expected_seq_++;

        // 尝试从缓存中取出连续的包
        auto expected_it = packet_buffer_.find(next_expected_seq_);
        while (expected_it != packet_buffer_.end()) {
            packets.push_back(expected_it->second);
            packet_buffer_.erase(expected_it);
            next_expected_seq_++;
            expected_it = packet_buffer_.find(next_expected_seq_);
        }
        if (callback_) {
            callback_->OnJitterBufferRtpPacket(packets, false);
        }
    } else if (ret == LITTLE_JUMP_SEQ) {
        packet_buffer_[seq] = rtp_packet;
    } else {
        LogErrorf(logger_, "FastJitterBuffer: unknown seq compare result:%d", ret);
    }
}


}