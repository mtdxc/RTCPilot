#include "ice_server.hpp"
#include "utils/uuid.hpp"
#include "utils/ipaddress.hpp"

namespace cpp_streamer {

IceServer::IceServer(IceOnDataWriteCallbackI* cb, Logger* logger) : cb_(cb), logger_(logger)
{
    ice_ufrag_ = UUID::MakeNumString(16);
    ice_pwd_ = UUID::MakeNumString(32);

    LogInfof(logger_, "IceServer construct, ice_ufrag:%s, ice_pwd:%s",
        ice_ufrag_.c_str(), ice_pwd_.c_str());
}

IceServer::~IceServer() {
    LogInfof(logger_, "IceServer destruct, ice_ufrag:%s, ice_pwd:%s",
        ice_ufrag_.c_str(), ice_pwd_.c_str());
}

int IceServer::HandleStunPacket(StunPacket* stun_pkt, UdpTuple addr) {
    if (stun_pkt->stun_method_ != STUN_METHOD_ENUM::BINDING ||
        stun_pkt->stun_class_ != STUN_CLASS_ENUM::STUN_REQUEST) {
        LogErrorf(logger_, "IceServer HandleStunPacket, not binding request");
        return -1;
    }
    // USERNAME, MESSAGE-INTEGRITY and PRIORITY are required.
    if (stun_pkt->username_.empty() ||
        stun_pkt->priority_ == 0 ||
        stun_pkt->message_integrity_ == nullptr) {
        LogErrorf(logger_, "IceServer HandleStunPacket, missing required attributes");
        return -1;
    }

    STUN_AUTHENTICATION ret = stun_pkt->CheckAuthentication(ice_ufrag_, ice_pwd_);
    if (ret != STUN_AUTHENTICATION::OK) {
        LogErrorf(logger_, "IceServer HandleStunPacket, authentication failed");
        return -1;
    }
    if (stun_pkt->ice_controlled_ != 0) {
        LogErrorf(logger_, "IceServer HandleStunPacket, ice controlled:%llu is forbiden",
            stun_pkt->ice_controlled_);
        return -2;
    }
    
    StunPacket* resp_pkt = stun_pkt->CreateSuccessResponse();
    struct sockaddr remote_sock_addr;

    GetIpv4Sockaddr(addr.ip_address, addr.port, &remote_sock_addr);
    resp_pkt->password_ = this->ice_pwd_;
	resp_pkt->xor_address_ = &remote_sock_addr;
    resp_pkt->Serialize();

    cb_->OnIceWrite(resp_pkt->data_, resp_pkt->data_len_, addr);
    delete resp_pkt;
    return 0;
}

} // namespace cpp_streamer
