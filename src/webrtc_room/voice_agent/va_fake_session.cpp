#include "va_fake_session.hpp"
#include "utils/logger.hpp"
#include "utils/uuid.hpp"

namespace cpp_streamer
{

VaFakeSession::VaFakeSession(const std::string& room_id, const std::string& user_id, Logger* logger)
    : room_id_(room_id), user_id_(user_id), logger_(logger)
{
    id_ = UUID::MakeUUID2();
    LogInfof(logger_, "VaFakeSession constructed for room_id:%s, user_id:%s, session_id:%s", 
        room_id_.c_str(), user_id_.c_str(), id_.c_str());
}

VaFakeSession::~VaFakeSession()
{
    LogInfof(logger_, "VaFakeSession destructed for room_id:%s, user_id:%s, session_id:%s", 
        room_id_.c_str(), user_id_.c_str(), id_.c_str());
}

bool VaFakeSession::IsConnected()
{
    return true;
}

void VaFakeSession::OnTransportSendRtp(uint8_t* data, size_t sent_size) {

}

void VaFakeSession::OnTransportSendRtcp(uint8_t* data, size_t sent_size) {

}

} // namespace cpp_streamer
