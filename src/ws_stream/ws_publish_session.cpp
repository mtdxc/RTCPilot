#include "ws_publish_session.hpp"
#include "utils/av/media_stream_manager.hpp"
#include "utils/byte_stream.hpp"
#include "utils/uuid.hpp"
#include "utils/timeex.hpp"

namespace cpp_streamer {

WsPublishSession::WsPublishSession(WebSocketSession* session, Logger* logger)
    :session_(session)
    ,logger_(logger)
{
    session_->SetSessionCallback(this);

    app_ = session_->GetQueryMap().at("app");
    stream_ = session_->GetQueryMap().at("stream");
    key_ = app_ + "/" + stream_;
    
    flv_demuxer_ptr_.reset(new FlvDemuxer(false, logger_));
    flv_demuxer_ptr_->SetLogger(logger_);
    flv_demuxer_ptr_->AddSinker(this);
    flv_demuxer_ptr_->SetReporter(this);
    flv_demuxer_ptr_->SetSupportFlvMediaHeader(true);

    auto r = UUID::GetRandomUint(10000, 99999);

    alive_ms_ = now_millisec();
    stream_name_ = "WsStreamSession_" + app_ + "_" + stream_ + "_" + std::to_string(r);
    LogInfof(logger_, "WsPublishSession construct, app:%s, stream:%s, key:%s",
		app_.c_str(), stream_.c_str(), key_.c_str());
}

WsPublishSession::~WsPublishSession()
{
    session_->SetSessionCallback(nullptr);
    session_ = nullptr;//don't own the session
    logger_ = nullptr;
	LogInfof(logger_, "WsPublishSession destruct, app:%s, stream:%s, key:%s", 
		app_.c_str(), stream_.c_str(), key_.c_str());
}

void WsPublishSession::OnReadData(int code, const uint8_t* data, size_t len)
{
    if (code < 0) {
        LogErrorf(logger_, "WsPublishSession::OnReadData error code:%d", code);
        closed_ = true;
        return;
    }
    if (app_.empty() || stream_.empty()) {
        LogErrorf(logger_, "WsPublishSession::OnReadData app or stream is empty, app:%s, stream:%s",
                  app_.c_str(), stream_.c_str());
        return;
    }
    alive_ms_ = now_millisec();
    LogDebugf(logger_, "WsPublishSession::OnReadData, app:%s, stream:%s, data len:%zu", 
		app_.c_str(), stream_.c_str(), len);

    // size_t debug_len = len > 80 ? 80 : len;
    // LogInfoData(logger_, data, debug_len, "flv data");
#if FILE_DUMP_DEBUG    
    // write to a flv file in binary format
    std::string filename = stream_name_ + ".flv";
    FILE* fp = nullptr;
    fopen_s(&fp, filename.c_str(), "ab+");
    if (fp) {
        fwrite(data, len, 1, fp);
        fclose(fp);
    }
#endif
    try {
        Media_Packet_Ptr pkt_ptr = std::make_shared<Media_Packet>(len);
        pkt_ptr->buffer_ptr_->AppendData((const char*)data, len);
        pkt_ptr->key_ = key_;
        flv_demuxer_ptr_->SourceData(pkt_ptr);
    } catch(const std::exception& e) {
        LogErrorf(logger_, "WsPublishSession::OnReadData exception:%s, app:%s, stream:%s", e.what(), app_.c_str(), stream_.c_str());
        return;
    }
}

void WsPublishSession::OnReadText(int code, const std::string& text) {
    if (code < 0) {
        LogErrorf(logger_, "WsPublishSession::OnReadText error code:%d", code);
        closed_ = true;
        return;
    }
    alive_ms_ = now_millisec();
    LogErrorf(logger_, "WsPublishSession::OnReadText shouldn't be called, text:%s", text.c_str());
}

void WsPublishSession::OnClose(int code, const std::string& desc) {
    LogInfof(logger_, "WsPublishSession::OnClose, code:%d, desc:%s", code, desc.c_str());
    closed_ = true;
}

int WsPublishSession::SourceData(Media_Packet_Ptr pkt_ptr) {
    if (closed_) {
        return -1;
    }
    alive_ms_ = now_millisec();
    int ret = MediaStreamManager::WriterMediaPacket(pkt_ptr);
    if (ret < 0) {
        LogErrorf(logger_, "WsPublishSession::SourceData failed to writer media packet, key:%s", pkt_ptr->key_.c_str());
        return ret;
    }
    return 0;
}

void WsPublishSession::OnReport(const std::string& name,
        const std::string& type,
        const std::string& value) {
    LogInfof(logger_, "WsPublishSession report, name:%s, type:%s, value:%s",
             name.c_str(), type.c_str(), value.c_str());
}


bool WsPublishSession::IsAlive() {
    if (closed_) {
        return false;
    }
    int64_t now_ms = now_millisec();
    if (alive_ms_ < 0) {
        alive_ms_ = now_ms;
        return true;
    }
    if (now_ms - alive_ms_ > 15000) {//15s no data
        LogInfof(logger_, "WsPublishSession::IsAlive false for no data timeout, app:%s, stream:%s",
            app_.c_str(), stream_.c_str());
        return false;
    }
    return true;
}

} // namespace cpp_streamer