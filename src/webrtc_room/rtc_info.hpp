#ifndef RTC_INFO_HPP
#define RTC_INFO_HPP

#include "utils/json.hpp"
#include "utils/av/av.hpp"
#include "utils/stringex.hpp"
#include <stdint.h>
#include <string>
#include <vector>
#include <map>

namespace cpp_streamer {

using json = nlohmann::json;

typedef enum
{
    UNKNOWN_USER_TYPE = 0,
    LOCAL_RTC_USER = 1,
    REMOTE_RTC_USER = 2
} RTC_USER_TYPE;

inline bool FmtpParamContain(const std::string& param_a, const std::string& param_b) {
    //profile-level-id=42e01f;level-asymmetry-allowed=1;packetization-mode=1
    std::vector<std::string> param_a_vec;
    std::vector<std::string> param_b_vec;
    std::map<std::string, std::string> param_a_map;
    std::map<std::string, std::string> param_b_map;
    
    int n = StringSplit(param_a, ";", param_a_vec);
    if (n <= 0) {
        return false;
    }
    n = StringSplit(param_b, ";", param_b_vec);
    if (n <= 0) {
        return false;
    }
    for (auto item : param_a_vec) {
        std::vector<std::string> param_vec;
        n = StringSplit(item, "=", param_vec);
        if (n != 2) {
            return false;
        }
        param_a_map[param_vec[0]] = param_vec[1];
    }
    for (auto item : param_b_vec) {
        std::vector<std::string> param_vec;
        n = StringSplit(item, "=", param_vec);
        if (n != 2) {
            return false;
        }
        param_b_map[param_vec[0]] = param_vec[1];
    }
    for (auto b_item : param_b_map) {
        auto a_iter = param_a_map.find(b_item.first);
        if (a_iter == param_a_map.end()) {
            return false;
        }
        if (a_iter->second != b_item.second) {
            return false;
        }
    }
    return true;
}

class RtpSessionParam
{
public:
    RtpSessionParam() = default;
    ~RtpSessionParam() = default;

public:
    void FromJson(const json& j) {
        av_type_ = avtype_fromstring(j["av_type"].get<std::string>());
        codec_name_ = j["codec"].get<std::string>();
        fmtp_param_ = j["fmtp_param"].get<std::string>();
        rtcp_features_.clear();
        for (const auto& feature : j["rtcp_features"]) {
            rtcp_features_.push_back(feature.get<std::string>());
        }
        if (j.find("channel") != j.end()) {
            channel_ = j["channel"].get<int>();
        }
        ssrc_ = j["ssrc"].get<uint32_t>();
        payload_type_ = j["payload_type"].get<uint8_t>();
        clock_rate_ = j["clock_rate"].get<uint32_t>();
        rtx_ssrc_ = j["rtx_ssrc"].get<uint32_t>();
        rtx_payload_type_ = j["rtx_payload_type"].get<uint8_t>();
        use_nack_ = j["use_nack"].get<bool>();
        if (j.find("key_request") != j.end()) {
            key_request_ = j["key_request"].get<bool>();
        }
        if (j.find("mid_ext_id") != j.end()) {
            mid_ext_id_ = j["mid_ext_id"].get<int>();
        }
        if (j.find("tcc_ext_id") != j.end()) {
            tcc_ext_id_ = j["tcc_ext_id"].get<int>();
        }
        if (j.find("abs_send_time_ext_id") != j.end()) {
            abs_send_time_ext_id_ = j["abs_send_time_ext_id"].get<int>();
        }
    }
public:
    void Dump(json& ret_json) const {
        /*
        video json exampe:
        {
            "av_type": 1,
            "codec": "H264",
            "fmtp_param": "profile-level-id=42e01f;level-asymmetry-allowed=1;packetization-mode=1;sprop-parameter-sets=Z0LgKdoBQBbpUgAAAwABAAADADAAAM1gAAAwAQAAADAEAAAMx8eJEV,aM4xUg==",
            "rtcp_features": [
                "nack",
                "pli"
            ],
            "channel": 2,
            "ssrc": 12345678,
            "payload_type": 96,
            "clock_rate": 90000,
            "rtx_ssrc": 87654321,
            "rtx_payload_type": 97,
            "use_nack": true,
            "key_request": true,
            "mid_ext_id": 1,
            "tcc_ext_id": 3
        }
        audio json example:
        {
            "av_type": 2,
            "codec": "opus",
            "fmtp_param": "minptime=10;useinbandfec=1",
            "rtcp_features": [
                "nack"
            ],
            "ssrc": 23456789,
            "payload_type": 111,
            "clock_rate": 48000,
            "use_nack": true
        }
        */

        ret_json["av_type"] = avtype_tostring(av_type_);
        ret_json["mid"] = mid_;
        ret_json["codec"] = codec_name_;
        ret_json["fmtp_param"] = fmtp_param_;
        ret_json["rtcp_features"] = json::array();
        for (const auto& feature : rtcp_features_) {
            ret_json["rtcp_features"].push_back(feature);
        }
        if (channel_ > 0) {
            ret_json["channel"] = channel_;
        }
        ret_json["ssrc"] = ssrc_;
        ret_json["payload_type"] = payload_type_;
        ret_json["clock_rate"] = clock_rate_;
        ret_json["rtx_ssrc"] = rtx_ssrc_;
        ret_json["rtx_payload_type"] = rtx_payload_type_;
        ret_json["use_nack"] = use_nack_;

        if (key_request_) {
            ret_json["key_request"] = key_request_;
        }
        if (mid_ext_id_ > 0) {
            ret_json["mid_ext_id"] = mid_ext_id_;
        }
        if (tcc_ext_id_ > 0) {
            ret_json["tcc_ext_id"] = tcc_ext_id_;
        }
        if (abs_send_time_ext_id_ > 0) {
            ret_json["abs_send_time_ext_id"] = abs_send_time_ext_id_;
        }
        return;
    }
    
    std::string Dump() const {
        json ret_json = json::object();
        Dump(ret_json);
        return ret_json.dump();
    }

public:
    MEDIA_PKT_TYPE av_type_ = MEDIA_UNKNOWN_TYPE;
    int mid_ = -1;
    uint32_t ssrc_ = 0;
    uint8_t payload_type_ = 0;
    int channel_ = 0;
    uint32_t clock_rate_ = 90000;
    uint32_t rtx_ssrc_ = 0;
    uint8_t rtx_payload_type_ = 0;
    bool use_nack_ = false;
	bool key_request_ = false;
    int mid_ext_id_ = -1;
    int tcc_ext_id_ = -1;
    int abs_send_time_ext_id_ = -1;
    std::string codec_name_;
    std::string fmtp_param_;
    std::vector<std::string> rtcp_features_;
};

class PushInfo
{
public:
    std::string pusher_id_;
    RtpSessionParam param_;

public:
    void DumpJson(json& j) const {
        j["pusherId"] = pusher_id_;
        auto rtp_param_json = json::object();
        param_.Dump(rtp_param_json);
        j["rtpParam"] = rtp_param_json;
    }
    std::string Dump() const {
        json j = json::object();
        DumpJson(j);
        return j.dump();
    }
};

class PullRequestInfo
{
public:
    PullRequestInfo() = default;
    ~PullRequestInfo() = default;

public:
    void Dump(json& ret_json) const {
        ret_json["target_user_id"] = target_user_id_;
        ret_json["src_user_id"] = src_user_id_;
        ret_json["room_id"] = room_id_;
        json pushers_json = json::array();
        for (const auto& push_info : pushers_) {
            json pusher_json = json::object();
            pusher_json["pusher_id"] = push_info.pusher_id_;
            pusher_json["type"] = avtype_tostring(push_info.param_.av_type_);
            pushers_json.push_back(pusher_json);
        }
        ret_json["pushers"] = pushers_json;
    }
    std::string Dump() const {
        json ret_json = json::object();
        Dump(ret_json);
        return ret_json.dump();
    }
public:
    std::string target_user_id_;
    std::string src_user_id_;
    std::string room_id_;
    std::vector<PushInfo> pushers_;
};

class MediaPushPullEventI
{
public:
    virtual void OnPushClose(const std::string& pusher_id) = 0;
    virtual void OnPullClose(const std::string& puller_id) = 0;
    virtual void OnKeyFrameRequest(const std::string& pusher_id, 
        const std::string& puller_user_id, 
        const std::string& pusher_user_id,
        uint32_t ssrc) = 0;
};

class AsyncRequestCallbackI
{
public:
    virtual void OnAsyncRequestResponse(int id, const std::string& method, json& resp_json) = 0;
};

class AsyncNotificationCallbackI
{
public:
    virtual void OnAsyncNotification(const std::string& method, json& data_json) = 0;
};

class PilotClientI
{
public:
    virtual void AsyncConnect() = 0;
    virtual int AsyncRequest(const std::string& method, json& data_json, AsyncRequestCallbackI* cb) = 0;
    virtual void AsyncNotification(const std::string& method, json& data_json) = 0;
};


} // namespace cpp_streamer

#endif // RTC_INFO_HPP