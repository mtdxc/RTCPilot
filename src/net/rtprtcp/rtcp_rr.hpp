#ifndef RTCP_RR_HPP
#define RTCP_RR_HPP

#include "rtprtcp_pub.hpp"
#include "byte_stream.hpp"
#include "stringex.hpp"
#include <vector>
#include <stdio.h>
#include <iostream>

namespace cpp_streamer
{

/*
        0                   1                   2                   3
        0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
header |V=2|P|    RC   |   PT=RR=201   |             length            |
       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
       |                 SSRC_1 (reporter ssrc)                        |
       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
report |                 SSRC_2 (reportee ssrc)                        |
block  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  1    | fraction lost |       cumulative number of packets lost       |
       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
       |           extended highest sequence number received           |
       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
       |                      interarrival jitter                      |
       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
       |                         last SR (LSR)                         |
       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
       |                   delay since last SR (DLSR)                  |
       +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
*/

typedef struct {
    //uint32_t reporter_ssrc; //
    uint32_t reportee_ssrc;
    uint32_t loss_fraction : 8;
    uint32_t cumulative_lost : 24;
    uint32_t highest_seq;
    uint32_t jitter;
    uint32_t lsr;//Last sender report timestamp
    uint32_t dlsr;//delay since last sender report
} RtcpRrBlock;

class RtcpRrBlockInfo
{
private:
    RtcpRrBlock* block_ = nullptr;
    bool need_delete_   = false;
    void freeBlock() {
        if (need_delete_) {
            if (block_) {
                free((void*)block_);
                block_ = nullptr;
            }
            need_delete_ = false;
        }
        else {
            block_ = nullptr;
        }
    }
public:
    RtcpRrBlockInfo()
    {
        need_delete_ = true;
        block_ = (RtcpRrBlock*)malloc(sizeof(RtcpRrBlock));
    }
    RtcpRrBlockInfo(RtcpRrBlock* block):block_(block)
    {
        need_delete_ = false;
    }
    ~RtcpRrBlockInfo()
    {
        freeBlock();
    }

public:
    void SetBlock(RtcpRrBlock* block) {
        block_ = block;
    }

    RtcpRrBlock* GetBlock() const {
        return block_;
    }

public:
    uint32_t GetReporteeSsrc() const {
        return ntohl(block_->reportee_ssrc);
    }

    void SetReporteeSsrc(uint32_t ssrc) {
        block_->reportee_ssrc = (uint32_t)htonl(ssrc);
    }

    uint32_t GetHighestSeq() const {
        return ntohl(block_->highest_seq);
    }

    void SetHighestSeq(uint32_t highest_seq) {
        block_->highest_seq = (uint32_t)htonl(highest_seq);
    }

    uint8_t GetFracLost() const {
        uint8_t* p = (uint8_t*)block_;
        return p[4];
    }

    void SetFracLost(uint8_t fraclost) {
        uint8_t* p = (uint8_t*)block_;
        p[4] = fraclost;
    }

    uint32_t GetCumulativeLost() const {
        uint8_t* p = (uint8_t*)block_;
        return ByteStream::Read3Bytes(p + 5);
    }

    void SetCumulativeLost(uint32_t cumulative_lost) {
        uint8_t* p = (uint8_t*)block_;
        ByteStream::Write3Bytes(p + 5, cumulative_lost);
    }

    uint32_t GetJitter() const {
        return (uint32_t)ntohl(block_->jitter);
    }

    void SetJitter(uint32_t jitter) {
        block_->jitter = (uint32_t)htonl(jitter);
    }

    uint32_t GetLsr() const {
        return (uint32_t)ntohl(block_->lsr);
    }

    void SetLsr(uint32_t lsr) {
        block_->lsr = (uint32_t)htonl(lsr);
    }

    uint32_t GetDlsr() const {
        return (uint32_t)ntohl(block_->dlsr);
    }

    void SetDlsr(uint32_t dlsr) {
        block_->dlsr = (uint32_t)htonl(dlsr);
    }
};

class RtcpRrPacket
{
public:
    RtcpRrPacket() {
        memset(this->data, 0, sizeof(this->data));
        this->rtcp_header_ = (RtcpCommonHeader*)(this->data);
        this->reporter_ssrc_p_ = (uint32_t*)(this->rtcp_header_ + 1);
    }

    RtcpRrPacket(uint8_t* header, size_t len) {
        memset(this->data, 0, sizeof(this->data));
        memcpy(this->data, header, len);
        this->rtcp_header_ = (RtcpCommonHeader*)this->data;
        this->reporter_ssrc_p_ = (uint32_t*)(this->rtcp_header_ + 1);
        len_ = len;
    }

    ~RtcpRrPacket() {

    }

public:
    static RtcpRrPacket* Parse(uint8_t* data, size_t len) {
        if (len < (sizeof(RtcpCommonHeader) + sizeof(uint32_t))) {
            CSM_THROW_ERROR("rtcp rr len(%zu) rr normal len(%zu) error", len, sizeof(RtcpCommonHeader) + sizeof(RtcpRrBlock));
        }
        RtcpRrPacket* pkt = new RtcpRrPacket(data, len);
        RtcpCommonHeader* hdr = (RtcpCommonHeader*)data;
        if (GetRtcpLength(hdr) <= (sizeof(RtcpCommonHeader) + sizeof(uint32_t))) {
            //no rr block inside
            return pkt;
        }
        RtcpRrBlock* block = (RtcpRrBlock*)(pkt->reporter_ssrc_p_ + 1);

        while ((uint8_t*)block < pkt->data + len) {
            RtcpRrBlockInfo info(block);
            pkt->rr_blocks_.push_back(info);
            block++;
        }
        return pkt;
    }

    uint8_t* GetData(size_t& data_len) {
        if (rr_blocks_.empty()) {
            data_len = 0;
            return 0;
        }
        data_len = sizeof(RtcpCommonHeader) + sizeof(uint32_t) + sizeof(RtcpRrBlock) * rr_blocks_.size();
        RtcpCommonHeader* rtcp_header = (RtcpCommonHeader*)this->data;
        rtcp_header->version     = 2;
        rtcp_header->padding     = 0;
        rtcp_header->count       = this->rr_blocks_.size();
        rtcp_header->packet_type = RTCP_RR;
        rtcp_header->length      = htons(((uint16_t)data_len) / 4 - 1);
        
        uint32_t* ssrc_p = (uint32_t*)(rtcp_header + 1);
        *ssrc_p = 1;
        
        RtcpRrBlock* block = (RtcpRrBlock*)(ssrc_p + 1);
        for (RtcpRrBlockInfo& info : this->rr_blocks_) {
            memcpy(block, info.GetBlock(), sizeof(RtcpRrBlock));
            block++;
        }
        return (uint8_t*)this->data;
    }
    size_t GetLen() const { return len_;}
    std::string Dump() const {
        std::stringstream ss;

        ss << "rtcp receive reporter ssrc:" << this->GetReporterSsrc()
            << ", block count:" << this->rr_blocks_.size() 
            << "\r\n";

        for(auto& info : this->rr_blocks_) {
            ss << ", reportee ssrc:" << info.GetReporteeSsrc()
                << ", frac lost:" << (int)info.GetFracLost()
                << "(" << (float)info.GetFracLost()/256.0 << ")"
                << ", cumulative lost:" << (int)info.GetCumulativeLost() << ", highest sequence:" << info.GetHighestSeq()
                << ", jitter:" << info.GetJitter() << ", lsr:" << info.GetLsr() << ", dlsr:" << info.GetDlsr() << "\r\n";
        }
        std::string data_str = DataToString(this->data, this->len_);
        ss << "data bytes:" << "\r\n";
        ss << data_str << "\r\n";

        return ss.str();
    }

    std::vector<RtcpRrBlockInfo> GetRrBlocks() const {
        return rr_blocks_;
    }
    
    void AddRrBlock(RtcpRrBlock* block) {
        if (rr_blocks_.empty()) {
            uint8_t* p = (uint8_t*)(reporter_ssrc_p_ + 1);
            memcpy(p, block, sizeof(RtcpRrBlock));
            RtcpRrBlockInfo info((RtcpRrBlock*)p);
            rr_blocks_.push_back(info);
            len_ = sizeof(RtcpCommonHeader) + sizeof(uint32_t) + sizeof(RtcpRrBlock);
            return;
        }
        uint8_t* p = (uint8_t*)(rr_blocks_[rr_blocks_.size() - 1].GetBlock() + 1);
        memcpy(p, block, sizeof(RtcpRrBlock));
        RtcpRrBlockInfo info((RtcpRrBlock*)p);
        rr_blocks_.push_back(info);
        len_ += sizeof(RtcpRrBlock);
        return;
    }

    uint32_t GetReporterSsrc() const {
        return ntohl(*reporter_ssrc_p_);
    }

private:
    RtcpCommonHeader* rtcp_header_ = nullptr;
    uint32_t* reporter_ssrc_p_ = nullptr;
    std::vector<RtcpRrBlockInfo> rr_blocks_;
    uint8_t data[1500];
    size_t len_ = 0;
};

}

#endif
