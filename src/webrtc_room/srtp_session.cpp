#include "srtp_session.hpp"
#include <cstring>

namespace cpp_streamer {
const char* SRtpTypeStr(SRtpType type){
    switch (type) {
        case SRTP_SESSION_TYPE_SEND:
            return "SEND";
        case SRTP_SESSION_TYPE_RECV:
            return "RECV";
        default:
            return "INVALID";
    }
}
const char* SRtpCryptoSuiteStr(SRtpSessionCryptoSuite suite) {
    switch (suite) {
        case AEAD_AES_256_GCM:
            return "AEAD_AES_256_GCM";
        case AEAD_AES_128_GCM:
            return  "AEAD_AES_128_GCM";
        case AES_CM_128_HMAC_SHA1_80:
            return "AES_CM_128_HMAC_SHA1_80";
        case AES_CM_128_HMAC_SHA1_32:
            return "AES_CM_128_HMAC_SHA1_32";
        default:
            return "UNKNOWN";
    }
}

bool SRtpSession::global_initialized_ = false;

int SRtpSession::GlobalInit() {
    if (global_initialized_) {
        return 0;
    }

    srtp_err_status_t err = srtp_init();
    if (err != srtp_err_status_ok) {
        return -1;
    }

    global_initialized_ = true;
    return 0;
}

void SRtpSession::GlobalCleanup() {
    if (global_initialized_) {
        srtp_shutdown();
        global_initialized_ = false;
    }
}

SRtpSession::SRtpSession(SRtpType type, SRtpSessionCryptoSuite cryptoSuite, 
                         uint8_t* key, size_t keyLen, Logger* logger)
    : type_(type)
    , crypto_suite_(cryptoSuite)
    , key_len_(keyLen)
    , logger_(logger)
{
    if (key && keyLen > 0) {
        key_ = new uint8_t[keyLen];
        std::memcpy(key_, key, keyLen);
    }

    if (!InitSrtpSession()) {
        LogErrorf(logger_, "Failed to initialize SRTP session");
    }
}

SRtpSession::~SRtpSession() {
    if (srtp_session_) {
        srtp_dealloc(srtp_session_);
        srtp_session_ = nullptr;
    }

    if (key_) {
        delete[] key_;
        key_ = nullptr;
    }
}

srtp_profile_t SRtpSession::GetSrtpProfile(SRtpSessionCryptoSuite cryptoSuite) {
    switch (cryptoSuite) {
        case AEAD_AES_256_GCM:
            return srtp_profile_aead_aes_256_gcm;
        case AEAD_AES_128_GCM:
            return srtp_profile_aead_aes_128_gcm;
        case AES_CM_128_HMAC_SHA1_80:
            return srtp_profile_aes128_cm_sha1_80;
        case AES_CM_128_HMAC_SHA1_32:
            return srtp_profile_aes128_cm_sha1_32;
        default:
            return srtp_profile_reserved;
    }
}

bool SRtpSession::InitSrtpSession() {
    // 确保 libsrtp 已初始化
    if (!global_initialized_) {
        LogErrorf(logger_, "libsrtp not initialized, call GlobalInit() first");
        return false;
    }
    
    srtp_policy_t policy;
    std::memset(&policy, 0, sizeof(srtp_policy_t));

    srtp_err_status_t err;
    
    // 根据加密套件类型设置 policy
    // 尝试使用 profile 方法，因为某些 libsrtp 版本可能不支持直接设置 AEAD policy
    switch (crypto_suite_) {
        case AEAD_AES_256_GCM: {
            // 先尝试使用 profile 方法
            srtp_profile_t profile = srtp_profile_aead_aes_256_gcm;
            err = srtp_crypto_policy_set_from_profile_for_rtp(&policy.rtp, profile);
            if (err != srtp_err_status_ok) {
                // 如果 profile 方法失败，尝试直接设置 policy
                LogWarnf(logger_, "Profile method failed for AEAD_AES_256_GCM, trying direct policy setup");
                srtp_crypto_policy_set_aes_gcm_256_8_auth(&policy.rtp);
            }
            err = srtp_crypto_policy_set_from_profile_for_rtcp(&policy.rtcp, profile);
            if (err != srtp_err_status_ok) {
                srtp_crypto_policy_set_aes_gcm_256_8_auth(&policy.rtcp);
            }
            // 验证密钥长度：AEAD_AES_256_GCM 需要 32 bytes key + 12 bytes salt = 44 bytes
            if (key_len_ != 44) {
                LogErrorf(logger_, "Invalid key length for AEAD_AES_256_GCM: expected 44, got %zu", key_len_);
                return false;
            }
            break;
        }
            
        case AEAD_AES_128_GCM: {
            // 先尝试使用 profile 方法
            srtp_profile_t profile = srtp_profile_aead_aes_128_gcm;
            err = srtp_crypto_policy_set_from_profile_for_rtp(&policy.rtp, profile);
            if (err != srtp_err_status_ok) {
                // 如果 profile 方法失败，尝试直接设置 policy
                LogWarnf(logger_, "Profile method failed for AEAD_AES_128_GCM, trying direct policy setup");
                srtp_crypto_policy_set_aes_gcm_128_8_auth(&policy.rtp);
            }
            err = srtp_crypto_policy_set_from_profile_for_rtcp(&policy.rtcp, profile);
            if (err != srtp_err_status_ok) {
                srtp_crypto_policy_set_aes_gcm_128_8_auth(&policy.rtcp);
            }
            // 验证密钥长度：AEAD_AES_128_GCM 需要 16 bytes key + 12 bytes salt = 28 bytes
            if (key_len_ != 28) {
                LogErrorf(logger_, "Invalid key length for AEAD_AES_128_GCM: expected 28, got %zu", key_len_);
                return false;
            }
            break;
        }
            
        case AES_CM_128_HMAC_SHA1_80:
        case AES_CM_128_HMAC_SHA1_32: {
            // 使用 profile 方法设置传统加密套件
            srtp_profile_t profile = GetSrtpProfile(crypto_suite_);
            if (profile == srtp_profile_reserved) {
                LogErrorf(logger_, "Invalid SRTP crypto suite: %d", crypto_suite_);
                return false;
            }
            err = srtp_crypto_policy_set_from_profile_for_rtp(&policy.rtp, profile);
            if (err != srtp_err_status_ok) {
                LogErrorf(logger_, "Failed to set RTP crypto policy: %d", err);
                return false;
            }
            err = srtp_crypto_policy_set_from_profile_for_rtcp(&policy.rtcp, profile);
            if (err != srtp_err_status_ok) {
                LogErrorf(logger_, "Failed to set RTCP crypto policy: %d", err);
                return false;
            }
            // 验证密钥长度：AES_CM_128 需要 16 bytes key + 14 bytes salt = 30 bytes
            if (key_len_ != 30) {
                LogErrorf(logger_, "Invalid key length for AES_CM_128: expected 30, got %zu", key_len_);
                return false;
            }
            break;
        }
            
        default:
            LogErrorf(logger_, "Unsupported SRTP crypto suite: %d", crypto_suite_);
            return false;
    }

    // 设置密钥（master key，包含 key + salt）
    if (!key_ || key_len_ == 0) {
        LogErrorf(logger_, "SRTP key is null or empty");
        return false;
    }
    
    switch (crypto_suite_) {
        case AEAD_AES_256_GCM:
		{
			srtp_crypto_policy_set_aes_gcm_256_16_auth(&policy.rtp);
			srtp_crypto_policy_set_aes_gcm_256_16_auth(&policy.rtcp);
			break;
		}
        case AEAD_AES_128_GCM:
		{
			srtp_crypto_policy_set_aes_gcm_128_16_auth(&policy.rtp);
			srtp_crypto_policy_set_aes_gcm_128_16_auth(&policy.rtcp);
            break;
        }
        case AES_CM_128_HMAC_SHA1_80:
        {
			srtp_crypto_policy_set_aes_cm_128_hmac_sha1_80(&policy.rtp);
			srtp_crypto_policy_set_aes_cm_128_hmac_sha1_80(&policy.rtcp);
            break;
        }
        case AES_CM_128_HMAC_SHA1_32:
		{
			srtp_crypto_policy_set_aes_cm_128_hmac_sha1_32(&policy.rtp);
			srtp_crypto_policy_set_aes_cm_128_hmac_sha1_80(&policy.rtcp);
			break;
		}
        default:
        {
            LogErrorf(logger_, "cipher type:%d is not supported", crypto_suite_);
            return false;
        }
    }
    
    if (key_len_ != policy.rtp.cipher_key_len) {
        LogErrorf(logger_, "Key length mismatch: expected %zu (cipher_key_len=%d), type:%d", 
            key_len_, policy.rtp.cipher_key_len, crypto_suite_);
        return false;
    }
    
    LogInfof(logger_, "SRTP policy: key_len=%zu, cipher_key_len=%d, auth_key_len=%d, cipher_type=%u, auth_type=%u, sec_serv=%d",
             key_len_, policy.rtp.cipher_key_len, policy.rtp.auth_key_len,
             policy.rtp.cipher_type, policy.rtp.auth_type, policy.rtp.sec_serv);
    
    policy.key = key_;
    
    // 设置 SSRC（通配符，匹配所有 SSRC）
    policy.ssrc.type = ssrc_any_inbound;
    if (type_ == SRTP_SESSION_TYPE_SEND) {
        policy.ssrc.type = ssrc_any_outbound;
    }

    policy.ssrc.value = 0;

    // 允许重放检测（接收端）
    policy.allow_repeat_tx = 0;
    policy.window_size = 128;
    
    // 设置下一跳
    policy.next = nullptr;
    
    // 初始化 keys 数组（新版本 libsrtp 可能需要）
    policy.keys = nullptr;
    policy.num_master_keys = 0;

    // 创建 SRTP 会话
    err = srtp_create(&srtp_session_, &policy);
    if (err != srtp_err_status_ok) {
        const char* err_str = "unknown";
        switch (err) {
            case srtp_err_status_fail: err_str = "srtp_err_status_fail"; break;
            case srtp_err_status_bad_param: err_str = "srtp_err_status_bad_param"; break;
            case srtp_err_status_alloc_fail: err_str = "srtp_err_status_alloc_fail"; break;
            case srtp_err_status_init_fail: err_str = "srtp_err_status_init_fail"; break;
            default: break;
        }
        LogErrorf(logger_, "Failed to create SRTP session: %d (%s)", err, err_str);
        LogErrorf(logger_, "Policy details: cipher_type=%u, cipher_key_len=%d, auth_type=%u, auth_key_len=%d, sec_serv=%d, key_len=%zu",
                  policy.rtp.cipher_type, policy.rtp.cipher_key_len, 
                  policy.rtp.auth_type, policy.rtp.auth_key_len, policy.rtp.sec_serv, key_len_);
        LogErrorf(logger_, "SSRC: type=%d, value=%u, window_size=%lu, allow_repeat_tx=%d",
                  policy.ssrc.type, policy.ssrc.value, policy.window_size, policy.allow_repeat_tx);
        
        // 检查是否是 libsrtp 不支持 AEAD 模式
        if (crypto_suite_ == AEAD_AES_256_GCM || crypto_suite_ == AEAD_AES_128_GCM) {
            LogErrorf(logger_, "Possible cause: libsrtp may not be compiled with AEAD support. "
                      "Please check libsrtp compilation options (--enable-openssl, --enable-aes-gcm).");
        }
        return false;
    }

    LogInfof(logger_, "SRTP session created: type=%s, suite=%s, key_len=%zu", 
             SRtpTypeStr(type_), SRtpCryptoSuiteStr(crypto_suite_), key_len_);
    
    return true;
}

bool SRtpSession::EncryptRtp(uint8_t*& data, int* len) {
    if (!srtp_session_ || !data || !len || *len <= 0) {
        LogErrorf(logger_, "Invalid parameters for RTP encryption");
        return false;
    }

    if (type_ != SRTP_SESSION_TYPE_SEND) {
        LogErrorf(logger_, "Cannot encrypt RTP on RECV session");
        return false;
    }

    int original_len = *len;

    if (original_len > (int)sizeof(data_buffer_)) {
        LogErrorf(logger_, "RTP packet too large to encrypt: %d bytes", original_len);
        return false;
    }
    memcpy(data_buffer_, data, original_len);
    data_buffer_len_ = original_len;

    srtp_err_status_t err = srtp_protect(srtp_session_, data_buffer_, &data_buffer_len_);
    if (err != srtp_err_status_ok) {
        LogErrorf(logger_, "Failed to encrypt RTP packet: %d, len=%d", err, original_len);
        return false;
    }
    data = data_buffer_;
    *len = data_buffer_len_;
    LogDebugf(logger_, "RTP encrypted: %d -> %d bytes", original_len, *len);
    return true;
}

bool SRtpSession::DecryptRtp(uint8_t* data, int* len) {
    if (!srtp_session_ || !data || !len || *len <= 0) {
        LogErrorf(logger_, "Invalid parameters for RTP decryption");
        return false;
    }

    if (type_ != SRTP_SESSION_TYPE_RECV) {
        LogErrorf(logger_, "Cannot decrypt RTP on SEND session");
        return false;
    }

    int original_len = *len;
    srtp_err_status_t err = srtp_unprotect(srtp_session_, data, len);
    if (err != srtp_err_status_ok) {
        LogErrorf(logger_, "Failed to decrypt RTP packet: %d, len=%d", err, original_len);
        return false;
    }

    LogDebugf(logger_, "RTP decrypted: %d -> %d bytes", original_len, *len);
    return true;
}

bool SRtpSession::EncryptRtcp(uint8_t* data, int* len) {
    if (!srtp_session_ || !data || !len || *len <= 0) {
        LogErrorf(logger_, "Invalid parameters for RTCP encryption");
        return false;
    }

    if (type_ != SRTP_SESSION_TYPE_SEND) {
        LogErrorf(logger_, "Cannot encrypt RTCP on RECV session");
        return false;
    }

    int original_len = *len;
    srtp_err_status_t err = srtp_protect_rtcp(srtp_session_, data, len);
    
    if (err != srtp_err_status_ok) {
        LogErrorf(logger_, "Failed to encrypt RTCP packet: %d, len=%d", err, original_len);
        return false;
    }

    LogDebugf(logger_, "RTCP encrypted: %d -> %d bytes", original_len, *len);
    return true;
}

bool SRtpSession::DecryptRtcp(uint8_t* data, int* len) {
    if (!srtp_session_ || !data || !len || *len <= 0) {
        LogErrorf(logger_, "Invalid parameters for RTCP decryption");
        return false;
    }

    if (type_ != SRTP_SESSION_TYPE_RECV) {
        LogErrorf(logger_, "Cannot decrypt RTCP on SEND session");
        return false;
    }

    int original_len = *len;
    srtp_err_status_t err = srtp_unprotect_rtcp(srtp_session_, data, len);
    
    if (err != srtp_err_status_ok) {
        LogErrorf(logger_, "Failed to decrypt RTCP packet: %d, len=%d", err, original_len);
        return false;
    }

    LogDebugf(logger_, "RTCP decrypted: %d -> %d bytes", original_len, *len);
    return true;
}

} // namespace cpp_streamer