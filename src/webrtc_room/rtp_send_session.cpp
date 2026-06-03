#include "rtp_send_session.hpp"
#include "net/rtprtcp/rtcp_sr.hpp"
#include "net/rtprtcp/rtcp_rr.hpp"
#include "utils/timeex.hpp"

namespace cpp_streamer {

#define SEND_RTX_CACHE_SIZE 1000

RtpSendSession::RtpSendSession(const RtpSessionParam& param,
    const std::string& room_id,
    const std::string& puller_user_id,
    const std::string& pusher_user_id,
    TransportSendCallbackI* send_cb,
    uv_loop_t* loop, Logger* logger) :
    RtpSession(param, room_id, puller_user_id, nullptr, loop, logger),
    send_cb_(send_cb)
{
    rtx_packet_cache_.resize(SEND_RTX_CACHE_SIZE);
    for (size_t i = 0; i < SEND_RTX_CACHE_SIZE; ++i) {
        rtx_packet_cache_[i] = nullptr;
    }
    puller_user_id_ = puller_user_id;
    pusher_user_id_ = pusher_user_id;

    LogInfof(logger_, "RtpSendSession construct, room_id:%s, puller_user_id:%s, pusher_user_id:%s, ssrc:%u, payload_type:%u",
        room_id_.c_str(), puller_user_id.c_str(), pusher_user_id.c_str(),
        param_.ssrc_, param_.payload_type_);
}

RtpSendSession::~RtpSendSession() {
    for (size_t i = 0; i < SEND_RTX_CACHE_SIZE; ++i) {
        if (rtx_packet_cache_[i] != nullptr) {
            delete rtx_packet_cache_[i];
            rtx_packet_cache_[i] = nullptr;
        }
    }
    LogInfof(logger_, "RtpSendSession destruct, room_id:%s, puller_user_id:%s, pusher_user_id:%s, ssrc:%u",
        room_id_.c_str(), puller_user_id_.c_str(), pusher_user_id_.c_str(), param_.ssrc_);
}

bool RtpSendSession::SendRtpPacket(RtpPacket* rtp_pkt) {
    const uint16_t seq = rtp_pkt->GetSeq();
    last_seq_ = seq;

    if (first_pkt_) {
        first_pkt_ = false;
        InitSeq(seq);
        last_pkt_ms_ = rtp_pkt->GetLocalMs();
        last_rtp_ts_ = rtp_pkt->GetTimestamp();

        StoreRtxPacket(rtp_pkt);
        LogInfof(logger_, "RtpSendSession first packet received, room_id:%s, user_id:%s, ssrc:%u, seq:%u",
            room_id_.c_str(), user_id_.c_str(), param_.ssrc_, seq);
        return true;
    }

    if (!UpdateSeq(rtp_pkt)) {
        LogInfof(logger_, "RtpSendSession packet out of order, room_id:%s, user_id:%s, ssrc:%u, seq:%u",
            room_id_.c_str(), user_id_.c_str(), param_.ssrc_, seq);
        return false;
    }
    
    last_pkt_ms_ = rtp_pkt->GetLocalMs();
    last_rtp_ts_ = rtp_pkt->GetTimestamp();

    send_statics_.Update(rtp_pkt->GetDataLength(), rtp_pkt->GetLocalMs());
    StoreRtxPacket(rtp_pkt);

    return true;
}

void RtpSendSession::StoreRtxPacket(RtpPacket* in_pkt) {
    if (param_.use_nack_ == false || param_.rtx_ssrc_ == 0 || param_.rtx_payload_type_ == 0) {
        return;
    }
    auto rtp_pkt = in_pkt->Clone();
    rtp_pkt->SetLogger(logger_);
    uint16_t seq = rtp_pkt->GetSeq();
    size_t index = seq % SEND_RTX_CACHE_SIZE;
    if (rtx_packet_cache_[index] != nullptr) {
        delete rtx_packet_cache_[index];
        rtx_packet_cache_[index] = nullptr;
    }

    rtx_packet_cache_[index] = rtp_pkt;
}

int RtpSendSession::RecvRtcpFbNack(RtcpFbNack* nack_pkt) {
    try {
        auto lost_seqs = nack_pkt->GetLostSeqs();
        for (const auto& seq : lost_seqs) {
            size_t index = seq % SEND_RTX_CACHE_SIZE;
            RtpPacket* rtx_pkt = rtx_packet_cache_[index];

            if (rtx_pkt != nullptr && rtx_pkt->GetSeq() == seq) {
                RetransmitRtxPackets(rtx_pkt);
            } else {
                if (rtx_pkt == nullptr) {
                    //it's possible that no rtx packet cached for the seq
                    //the uplink may not receive the resend rtp packet
                    LogDebugf(logger_, "[Error]: RtpSendSession no RTX packet cached for NACK, \
room_id:%s, puller_user_id:%s, pusher_user_id:%s, ssrc:%u, lost_seq:%u, last send seq:%d",
                        room_id_.c_str(), puller_user_id_.c_str(), pusher_user_id_.c_str(),
                        nack_pkt->GetMediaSsrc(), seq, last_seq_);
                } else {
                    LogErrorf(logger_, "[Error]: RtpSendSession RTX packet seq mismatch for NACK, \
room_id:%s, puller_user_id:%s, pusher_user_id:%s, ssrc:%u, lost_seq:%u, cached_seq:%u, index:%zu, last send seq:%d",
                        room_id_.c_str(), puller_user_id_.c_str(), pusher_user_id_.c_str(),
                        nack_pkt->GetMediaSsrc(), seq, rtx_pkt->GetSeq(), index, last_seq_);
                }
            }
        }
    } catch(const std::exception& e) {
        LogErrorf(logger_, "RtpSendSession RecvRtcpFbNack exception: %s", e.what());
        return -1;
    }
    
    return 0;
}

void RtpSendSession::RetransmitRtxPackets(RtpPacket* rtp_pkt) {
    if (param_.rtx_ssrc_ > 0 && param_.rtx_payload_type_ > 0 && send_cb_ != nullptr) {
        // save origin ssrc and payload
        uint32_t origin_ssrc = rtp_pkt->GetSsrc();
        uint8_t origin_payload = rtp_pkt->GetPayloadType();

        rtx_seq_++;
        // replace ssrc and payload with rtx ssrc and rtx payload
        rtp_pkt->RtxMux(param_.rtx_payload_type_, param_.rtx_ssrc_, rtx_seq_);

        send_cb_->OnTransportSendRtp(rtp_pkt->GetData(), rtp_pkt->GetDataLength());
        
        // restore origin ssrc and payload
        rtp_pkt->RtxDemux(origin_ssrc, origin_payload);
    }
}

void RtpSendSession::OnTimer(int64_t now_ms) {
    OnSendRtcpSr(now_ms);
}

void RtpSendSession::OnSendRtcpSr(int64_t now_ms) {
    if (last_rtcp_sr_ms_ < 0) {
        last_rtcp_sr_ms_ = now_ms;
        return;
    }
    if (now_ms - last_rtcp_sr_ms_ < 1000) {
        return;
    }
    if (send_statics_.GetCount() == 0) {
        return;
    }
    last_rtcp_sr_ms_ = now_ms;

    NTP_TIMESTAMP ntp = millisec_to_ntp(now_ms);
    int64_t diff_ms = now_ms - last_pkt_ms_;
    int64_t diff_ts = (param_.clock_rate_ * diff_ms) / 1000;
    uint32_t rtp_ts = (uint32_t)last_rtp_ts_ + (uint32_t)diff_ts;

    last_rtcp_sr_rtp_ts_ = rtp_ts;
    // send rtcp sr
    RtcpSrPacket sr_pkt;
    sr_pkt.SetSsrc(param_.ssrc_);
    sr_pkt.SetNtp(ntp.ntp_sec, ntp.ntp_frac);
    sr_pkt.SetRtpTimestamp(rtp_ts);
    sr_pkt.SetPktCount((uint32_t)send_statics_.GetCount());
    sr_pkt.SetBytesCount((uint32_t)send_statics_.GetBytes());

    size_t sr_len = 0;
    uint8_t* sr_data = sr_pkt.Serial(sr_len);
    send_cb_->OnTransportSendRtcp(sr_data, sr_len);
}

int RtpSendSession::RecvRtcpRrBlock(RtcpRrBlockInfo& rr_block) {
    // get rtt from rr block
    uint32_t lsr = rr_block.GetLsr();
    uint32_t dlsr = rr_block.GetDlsr();
    if (lsr == 0 || dlsr == 0) {
        return 0;
    }
    int64_t now_ms = now_millisec();
    NTP_TIMESTAMP ntp = millisec_to_ntp(now_ms);
	
    uint32_t compactNtp = (ntp.ntp_sec & 0x0000FFFF) << 16;
	compactNtp |= (ntp.ntp_frac & 0xFFFF0000) >> 16;

    uint32_t rtt_ms = 0;
    if (compactNtp > dlsr + lsr) {
        // RTT = current_time - (DLSR + LSR) in 1/65536 seconds fraction
        uint32_t rtt_ntp_frac = compactNtp - dlsr - lsr;
        rtt_ms = (rtt_ntp_frac >> 16) * 1000;
        rtt_ms += ((rtt_ntp_frac & 0x0000FFFF) * 1000) >> 16;
    } else {
        rtt_ms = 5;
    }

    if (rtt_ms > 200) {
        LogInfof(logger_, "RtpSendSession RTT too large, cap to 200ms, room_id:%s, puller_user_id:%s, \
pusher_user_id:%s, ssrc:%u, rtt_ms:%u, lsr:%u, dlsr:%u, compactNtp:%u, diff:%u",
            room_id_.c_str(), puller_user_id_.c_str(), pusher_user_id_.c_str(),
            param_.ssrc_, rtt_ms, lsr, dlsr, compactNtp, compactNtp - dlsr - lsr);
        rtt_ms = 200;
    }

    avg_rtt_ms_ = (avg_rtt_ms_ * 15 + rtt_ms) / 16;

    lost_total_ = rr_block.GetCumulativeLost();
    lost_fract_ = rr_block.GetFracLost();
    lost_rate_ = (float)lost_fract_ / 256.0f;

    if (rtcp_rr_dbg_ms_ < 0) {
        rtcp_rr_dbg_ms_ = now_ms;
    } else if (now_ms - rtcp_rr_dbg_ms_ > 5000) {
        rtcp_rr_dbg_ms_ = now_ms;
        LogInfof(logger_, "RtpSendSession received RTCP RR, type:%s, room_id:%s, puller_user_id:%s, \
ssrc:%u, avg_rtt:%lld, lost_total:%lld, lost_fract:%lld, lost_rate:%.2f%%",
            avtype_tostring(param_.av_type_).c_str(), room_id_.c_str(), puller_user_id_.c_str(),
            param_.ssrc_, avg_rtt_ms_, lost_total_, lost_fract_, lost_rate_ * 100.0f);
    }
    return 0;
}

}