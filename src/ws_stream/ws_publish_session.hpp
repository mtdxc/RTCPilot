#ifndef WS_STREAM_SESSION_HPP
#define WS_STREAM_SESSION_HPP

#include "net/http/websocket/websocket_session.hpp"
#include "format/flv/flv_demux.hpp"
#include "utils/logger.hpp"
#include <string>
#include <memory>

namespace cpp_streamer {

class WsPublishSession : public WebSocketSessionCallBackI, public CppStreamerInterface, public StreamerReport
{
public:
    WsPublishSession(WebSocketSession* session, Logger* logger);
    virtual ~WsPublishSession();

public:
    bool IsAlive();

public:
    virtual void OnReadData(int code, const uint8_t* data, size_t len) override;
    virtual void OnReadText(int code, const std::string& text) override;
    virtual void OnClose(int code, const std::string& desc) override;

protected:
    virtual std::string StreamerName() override {
        return stream_name_;
    }
    virtual void SetLogger(Logger* logger) override {
        logger_ = logger;
    }
    virtual int AddSinker(CppStreamerInterface* sinker) override { return 0; }
    virtual int RemoveSinker(const std::string& name) override { return 0; }
    virtual int SourceData(Media_Packet_Ptr pkt_ptr) override;
    virtual void StartNetwork(const std::string& url, void* loop_handle) override {}
    virtual void AddOption(const std::string& key, const std::string& value) override {}
    virtual void SetReporter(StreamerReport* reporter) override {}

protected:
    virtual void OnReport(const std::string& name,
            const std::string& type,
            const std::string& value) override;
private:
    WebSocketSession* session_ = nullptr;
    Logger* logger_ = nullptr;
private:
    std::string stream_name_;
    std::string app_;
    std::string stream_;
    std::string key_;

private:
    std::unique_ptr<FlvDemuxer> flv_demuxer_ptr_;
    bool closed_ = false;

private:
    int64_t alive_ms_ = -1;
};

}
#endif // WS_STREAM_SESSION_HPP