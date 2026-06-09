#ifndef SRTP_SESSION_HPP
#define SRTP_SESSION_HPP
#include "utils/logger.hpp"
#ifdef _WIN32
#include "srtp.h"
#else
#include <srtp2/srtp.h>
#endif
#include <cstdint>
#include <cstddef>

namespace cpp_streamer {

typedef enum
{
    SRTP_SESSION_CRYPTO_SUITE_INVALID = -1,
    AEAD_AES_256_GCM = 0,
    AEAD_AES_128_GCM,
    AES_CM_128_HMAC_SHA1_80,
    AES_CM_128_HMAC_SHA1_32
} SRtpSessionCryptoSuite;

typedef enum
{
    SRTP_SESSION_TYPE_INVALID = -1,
    SRTP_SESSION_TYPE_SEND = 0,
    SRTP_SESSION_TYPE_RECV
} SRtpType;

class SRtpSession
{
public:
    SRtpSession(SRtpType type, 
        SRtpSessionCryptoSuite cryptoSuite, 
        uint8_t* key, size_t keyLen, 
        Logger* logger);
    ~SRtpSession();

    // 初始化 libsrtp（静态方法，只需调用一次）
    static int GlobalInit();
    static void GlobalCleanup();

    // RTP 加密/解密
    bool EncryptRtp(uint8_t*& data, int* len);
    bool DecryptRtp(uint8_t* data, int* len);

    // RTCP 加密/解密
    bool EncryptRtcp(uint8_t* data, int* len);
    bool DecryptRtcp(uint8_t* data, int* len);

private:
    bool InitSrtpSession();
    srtp_profile_t GetSrtpProfile(SRtpSessionCryptoSuite cryptoSuite);

private:
    SRtpType type_;
    SRtpSessionCryptoSuite crypto_suite_;
    uint8_t* key_ = nullptr;
    size_t key_len_ = 0;
    Logger* logger_ = nullptr;
    srtp_t srtp_session_ = nullptr;
    static bool global_initialized_;

private:
    uint8_t data_buffer_[2048] = {0};
    int data_buffer_len_ = 0;
};

} // namespace cpp_streamer

#endif // SRTP_SESSION_HPP