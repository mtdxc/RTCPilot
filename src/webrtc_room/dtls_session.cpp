#include "dtls_session.hpp"
#include "utils/stringex.hpp"
#include "utils/timeex.hpp"
#include "srtp_session.hpp"
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/objects.h>
#include <openssl/core_names.h>
#include <assert.h>
#include <vector>
#include <string>

namespace cpp_streamer {

const size_t SslReadBufferSize = 65536;

X509* DtlsSession::certificate_ = nullptr;
EVP_PKEY* DtlsSession::private_key_ = nullptr;
SSL_CTX* DtlsSession::ssl_ctx_ = nullptr;
uint8_t DtlsSession::ssl_read_buffer_[SslReadBufferSize] = { 0 };

// AES-HMAC: http://tools.ietf.org/html/rfc3711
static constexpr size_t SrtpMasterKeyLength{ 16u };
static constexpr size_t SrtpMasterSaltLength{ 14u };
static constexpr size_t SrtpMasterLength{ SrtpMasterKeyLength + SrtpMasterSaltLength };
// AES-GCM: http://tools.ietf.org/html/rfc7714
static constexpr size_t SrtpAesGcm256MasterKeyLength{ 32u };
static constexpr size_t SrtpAesGcm256MasterSaltLength{ 12u };
static constexpr size_t SrtpAesGcm256MasterLength{ SrtpAesGcm256MasterKeyLength + SrtpAesGcm256MasterSaltLength };
static constexpr size_t SrtpAesGcm128MasterKeyLength{ 16u };
static constexpr size_t SrtpAesGcm128MasterSaltLength{ 12u };
static constexpr size_t SrtpAesGcm128MasterLength{ SrtpAesGcm128MasterKeyLength + SrtpAesGcm128MasterSaltLength };
	
std::map<std::string, Role> DtlsSession::string2role_ = {
    {"auto", Role::ROLE_AUTO},
    {"client", Role::ROLE_CLIENT},
    {"server", Role::ROLE_SERVER}
};

std::map<std::string, FingerprintAlgorithm> DtlsSession::string2fingerprint_algorithm_ = {
    {"sha-1", FingerprintAlgorithm::ALGORITHM_SHA1},
    {"sha-224", FingerprintAlgorithm::ALGORITHM_SHA224},
    {"sha-256", FingerprintAlgorithm::ALGORITHM_SHA256},
    {"sha-384", FingerprintAlgorithm::ALGORITHM_SHA384},
    {"sha-512", FingerprintAlgorithm::ALGORITHM_SHA512}
};

std::map<FingerprintAlgorithm, std::string> DtlsSession::fingerprint_algorithm2string_ = {
    {FingerprintAlgorithm::ALGORITHM_SHA1, "sha-1"},
    {FingerprintAlgorithm::ALGORITHM_SHA224, "sha-224"},
    {FingerprintAlgorithm::ALGORITHM_SHA256, "sha-256"},
    {FingerprintAlgorithm::ALGORITHM_SHA384, "sha-384"},
    {FingerprintAlgorithm::ALGORITHM_SHA512, "sha-512"}
};

std::vector<SRtpCryptoSuiteMapEntry> DtlsSession::srtp_crypto_suites_ = {
	{ AEAD_AES_256_GCM,        "SRTP_AEAD_AES_256_GCM"  },
	{ AEAD_AES_128_GCM,        "SRTP_AEAD_AES_128_GCM"  },
	{ AES_CM_128_HMAC_SHA1_80, "SRTP_AES128_CM_SHA1_80" },
	{ AES_CM_128_HMAC_SHA1_32, "SRTP_AES128_CM_SHA1_32" }
};

std::vector<Fingerprint> DtlsSession::local_fingerprints_;

static int onSslCertificateVerify(int , X509_STORE_CTX*)
{
	return 1;
}
static void onSslInfo(const SSL* ssl, int where, int ret)
{
	auto session = static_cast<DtlsSession*>(SSL_get_ex_data(ssl, 0));
	if (session) {
		session->OnSslInfo(where, ret);
	}
}
static unsigned int OnSslDtlsTimer(SSL* /*ssl*/, unsigned int timerUs)
{
	if (timerUs == 0)
		return 100000;
	else if (timerUs >= 4000000)
		return 4000000;
	else
		return 2 * timerUs;
}

void OnSslMsgCallback(int write_p, int version, int content_type,
							 const void* buf, size_t len, SSL* ssl, void* arg)
{
	auto* dtls = static_cast<DtlsSession*>(SSL_get_ex_data(ssl, 0));
	const char* dir = write_p ? "write" : "read";
	// DTLS/TLS content types: 20=ChangeCipherSpec,21=Alert,22=Handshake,23=Application
	LogInfof(dtls->logger_, "SSL msg cb: %s v=%d content_type=%d len=%zu", dir, version, content_type, len);
}

long OnSslBioOut(BIO* bio, int operationType, const char* argp, size_t len, int /*argi*/, long /*argl*/, int ret, size_t* /*processed*/) {
	const long resultOfcallback = (operationType == BIO_CB_RETURN) ? static_cast<long>(ret) : 1;
	auto* dtlsTransport = reinterpret_cast<DtlsSession*>(BIO_get_callback_arg(bio));
	
	if (len > 0)
		LogInfof(dtlsTransport->logger_, "OnSslBioOut operationType:%d, len:%zu", operationType, len);

	if ((operationType == BIO_CB_WRITE) && argp && len > 0)
	{

		dtlsTransport->transport_->OnDtlsTransportSendData(
			reinterpret_cast<const uint8_t*>(argp), len, dtlsTransport->dtls_remote_addr_);
		// Clear the BIO buffer.
		auto ret = BIO_reset(dtlsTransport->ssl_bio_to_network_);

		if (ret != 1) {
			LogErrorf(dtlsTransport->logger_, "BIO_reset() failed [ret:%d]", ret);
		}
	}

	return resultOfcallback;
}

std::string Fingerprint::ToString() const {
    std::string alg_str = DtlsSession::GetFingerprintAlgorithmString(algorithm);
    return alg_str + " " + value;
}

int DtlsSession::Init(const std::string& cert_file, const std::string& key_file) {
	BIO* bio = BIO_new_file(cert_file.c_str(), "r");
	if (!bio) {
		return -1;
	}
	certificate_ = PEM_read_bio_X509(bio, nullptr, nullptr, nullptr);
	BIO_free(bio);
	if (!certificate_) {
		return -1;
	}

	//   ȡ˽Կ
	bio = BIO_new_file(key_file.c_str(), "r");
	if (!bio) {
		return -1;
	}
	private_key_ = PEM_read_bio_PrivateKey(bio, nullptr, nullptr, nullptr);
	BIO_free(bio);
	if (!private_key_) {
		return -1;
	}

    ssl_ctx_ = SSL_CTX_new(DTLS_method());
    if (!ssl_ctx_) {
        return -1;
    }
    int ret = SSL_CTX_use_certificate(ssl_ctx_, certificate_);
    if (ret != 1) {
        return -1;
    }
    ret = SSL_CTX_use_PrivateKey(ssl_ctx_, private_key_);
    if (ret != 1) {
        return -1;
    }
    ret = SSL_CTX_check_private_key(ssl_ctx_);
    if (ret != 1) {
        return -1;
    }
    
    /* Extract certificate curve information and configure supported curves */
    const char* cert_curve_name = nullptr;
    int cert_curve_nid = 0;
    EVP_PKEY* cert_key = X509_get_pubkey(certificate_);
    if (cert_key) {
        int key_type = EVP_PKEY_id(cert_key);
        if (key_type == EVP_PKEY_EC) {
            // 使用 EVP_PKEY API 直接获取曲线名称
            char group_name[64] = {0};
            size_t group_name_len = sizeof(group_name);
            
            if (EVP_PKEY_get_utf8_string_param(cert_key, OSSL_PKEY_PARAM_GROUP_NAME, 
                                               group_name, sizeof(group_name), &group_name_len) == 1) {
                cert_curve_name = OBJ_sn2nid(group_name) != NID_undef ? group_name : nullptr;
                cert_curve_nid = OBJ_sn2nid(group_name);
                if (cert_curve_nid != NID_undef) {
                    LogInfof(nullptr, "Certificate uses EC curve: %s (NID: %d)", 
                        group_name, cert_curve_nid);
                }
            } else {
                LogWarnf(nullptr, "Failed to get EC curve name from certificate");
            }
        } else {
            LogInfof(nullptr, "Certificate key type: %d (not EC)", key_type);
        }
        EVP_PKEY_free(cert_key);
    }
    
    SSL_CTX_set_options(
		  ssl_ctx_,
		  SSL_OP_CIPHER_SERVER_PREFERENCE | SSL_OP_NO_TICKET | SSL_OP_SINGLE_ECDH_USE |
		    SSL_OP_NO_QUERY_MTU);

    SSL_CTX_set_session_cache_mode(ssl_ctx_, SSL_SESS_CACHE_OFF);

    SSL_CTX_set_read_ahead(ssl_ctx_, 1);

	SSL_CTX_set_verify_depth(ssl_ctx_, 4);

/* Configure supported elliptic curves for ECDH key exchange.
 * Priority: certificate curve first, then common curves.
 * OpenSSL 3.x uses groups API, OpenSSL 1.1.1 uses curves API. */
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
	/* OpenSSL 3.0+: Use groups API (recommended) */
	std::string groups_list;
	if (cert_curve_name && cert_curve_nid != 0) {
		groups_list = cert_curve_name;
		groups_list += ":prime256v1:secp384r1:secp521r1";
	} else {
		groups_list = "prime256v1:secp384r1:secp521r1";
	}
	
	if (SSL_CTX_set1_groups_list(ssl_ctx_, groups_list.c_str()) != 1) {
		LogWarnf(nullptr, "SSL_CTX_set1_groups_list failed with '%s', trying prime256v1 only", 
			groups_list.c_str());
		if (SSL_CTX_set1_groups_list(ssl_ctx_, "prime256v1") != 1) {
			LogWarnf(nullptr, "SSL_CTX_set1_groups_list with prime256v1 also failed");
		}
	} else {
		LogInfof(nullptr, "SSL_CTX_set1_groups_list set to: %s", groups_list.c_str());
	}
	/* Note: In OpenSSL 3.x, SSL_CTX_set_ecdh_auto() is not available and not needed.
	 * The groups list configuration is sufficient for ECDH curve selection. */
#elif defined(SSL_CTX_set1_groups_list)
	/* OpenSSL 1.1.1+ with groups API support */
	std::string groups_list;
	if (cert_curve_name && cert_curve_nid != 0) {
		groups_list = cert_curve_name;
		groups_list += ":prime256v1:secp384r1:secp521r1";
	} else {
		groups_list = "prime256v1:secp384r1:secp521r1";
	}
	
	if (SSL_CTX_set1_groups_list(ssl_ctx_, groups_list.c_str()) != 1) {
		LogWarnf(nullptr, "SSL_CTX_set1_groups_list failed with '%s', trying prime256v1 only", 
			groups_list.c_str());
		if (SSL_CTX_set1_groups_list(ssl_ctx_, "prime256v1") != 1) {
			LogWarnf(nullptr, "SSL_CTX_set1_groups_list with prime256v1 also failed");
		}
	} else {
		LogInfof(nullptr, "SSL_CTX_set1_groups_list set to: %s", groups_list.c_str());
	}
	
	/* Try to enable automatic ECDH curve selection if available */
	#if defined(SSL_CTX_set_ecdh_auto)
		SSL_CTX_set_ecdh_auto(ssl_ctx_, 1);
	#endif
#elif defined(SSL_CTX_set1_curves_list)
	/* OpenSSL 1.1.0+: Use curves API */
	std::string curves_list;
	if (cert_curve_name && cert_curve_nid != 0) {
		curves_list = cert_curve_name;
		curves_list += ":prime256v1:secp384r1:secp521r1";
	} else {
		curves_list = "prime256v1:secp384r1:secp521r1";
	}
	
	if (SSL_CTX_set1_curves_list(ssl_ctx_, curves_list.c_str()) != 1) {
		LogWarnf(nullptr, "SSL_CTX_set1_curves_list failed with '%s', trying prime256v1 only", 
			curves_list.c_str());
		if (SSL_CTX_set1_curves_list(ssl_ctx_, "prime256v1") != 1) {
			LogWarnf(nullptr, "SSL_CTX_set1_curves_list with prime256v1 also failed");
		}
	} else {
		LogInfof(nullptr, "SSL_CTX_set1_curves_list set to: %s", curves_list.c_str());
	}
	
	/* Try to enable automatic ECDH curve selection if available */
	#if defined(SSL_CTX_set_ecdh_auto)
		SSL_CTX_set_ecdh_auto(ssl_ctx_, 1);
	#endif
#else
	/* Fallback for older OpenSSL versions: use deprecated SSL_CTX_set_tmp_ecdh() */
	EC_KEY* ecdh = nullptr;
	if (cert_curve_nid != 0) {
		ecdh = EC_KEY_new_by_curve_name(cert_curve_nid);
		if (ecdh) {
			LogInfof(nullptr, "Using certificate curve (NID: %d) for ECDH", cert_curve_nid);
		}
	}
	if (!ecdh) {
		ecdh = EC_KEY_new_by_curve_name(NID_X9_62_prime256v1);
		if (ecdh) {
			LogInfof(nullptr, "Using fallback curve prime256v1 for ECDH");
		}
	}
	if (ecdh) {
		SSL_CTX_set_tmp_ecdh(ssl_ctx_, ecdh);
		EC_KEY_free(ecdh);
	} else {
		LogWarnf(nullptr, "EC_KEY_new_by_curve_name failed; ECDH curves may be unavailable");
	}
#endif


	SSL_CTX_set_verify(
		  ssl_ctx_, SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT, onSslCertificateVerify);

	SSL_CTX_set_info_callback(ssl_ctx_, onSslInfo);
	/* Enable message callback to log handshake records for diagnostics. */
#ifdef SSL_CTX_set_msg_callback
	SSL_CTX_set_msg_callback(ssl_ctx_, OnSslMsgCallback);
#endif

	ret = SSL_CTX_set_cipher_list(
		  ssl_ctx_, "DEFAULT:!NULL:!aNULL:!SHA256:!SHA384:!aECDH:!AESGCM+AES256:!aPSK");
	if (ret == 0){
		return -1;
	}
    ret = SSL_CTX_set_tlsext_use_srtp(ssl_ctx_, 
        "SRTP_AEAD_AES_256_GCM:SRTP_AEAD_AES_128_GCM:SRTP_AES128_CM_SHA1_80:SRTP_AES128_CM_SHA1_32");
    if (ret != 0) {
        LogErrorf(nullptr, "SSL_CTX_set_tlsext_use_srtp() failed");
        return -1;
    }

	for (auto& kv : DtlsSession::string2fingerprint_algorithm_)
	{
		const std::string& algorithmString = kv.first;
		FingerprintAlgorithm algorithm     = kv.second;
		uint8_t binaryFingerprint[EVP_MAX_MD_SIZE];
		unsigned int size{ 0 };
		char hexFingerprint[(EVP_MAX_MD_SIZE * 3) + 1];
		const EVP_MD* hashFunction;
		int ret;

		switch (algorithm)
		{
			case FingerprintAlgorithm::ALGORITHM_SHA1:
				hashFunction = EVP_sha1();
				break;

			case FingerprintAlgorithm::ALGORITHM_SHA224:
				hashFunction = EVP_sha224();
				break;

			case FingerprintAlgorithm::ALGORITHM_SHA256:
				hashFunction = EVP_sha256();
				break;

			case FingerprintAlgorithm::ALGORITHM_SHA384:
				hashFunction = EVP_sha384();
				break;

			case FingerprintAlgorithm::ALGORITHM_SHA512:
				hashFunction = EVP_sha512();
				break;

			default:
				throw("unknown algorithm");
		}

		ret = X509_digest(DtlsSession::certificate_, hashFunction, binaryFingerprint, &size);

		if (ret == 0)
		{
			throw("Fingerprints generation failed");
		}

		// Convert to hexadecimal format in uppercase with colons.
		for (unsigned int i{ 0 }; i < size; ++i)
		{
			std::snprintf(hexFingerprint + (i * 3), 4, "%.2X:", binaryFingerprint[i]);
		}
		hexFingerprint[(size * 3) - 1] = '\0';


		// Store it in the vector.
		Fingerprint fingerprint;

		fingerprint.algorithm = DtlsSession::GetFingerprintAlgorithm(algorithmString);
		fingerprint.value     = hexFingerprint;

		DtlsSession::local_fingerprints_.push_back(fingerprint);
	}
	return 0;
}

void DtlsSession::CleanupGlobal() {
    if (ssl_ctx_) { SSL_CTX_free(ssl_ctx_); ssl_ctx_ = nullptr; }
    if (certificate_) { X509_free(certificate_); certificate_ = nullptr; }
    if (private_key_) { EVP_PKEY_free(private_key_); private_key_ = nullptr; }
}

bool DtlsSession::IsDtlsData(const uint8_t* data, size_t len) {
	return ((len >= 13) && (data[0] > 19 && data[0] < 64));
}

FingerprintAlgorithm DtlsSession::GetFingerprintAlgorithm(const std::string& fingerprint)
{
	auto it = DtlsSession::string2fingerprint_algorithm_.find(fingerprint);
	if (it != DtlsSession::string2fingerprint_algorithm_.end())
		return it->second;
	else
		return FingerprintAlgorithm::ALGORITHM_NONE;
}

std::string& DtlsSession::GetFingerprintAlgorithmString(FingerprintAlgorithm fingerprint)
{
	auto it = DtlsSession::fingerprint_algorithm2string_.find(fingerprint);
	return it->second;
}

Fingerprint DtlsSession::GetLocalFingerprint(FingerprintAlgorithm algorithm) {
    for (const auto& fp : DtlsSession::local_fingerprints_) {
        if (fp.algorithm == algorithm) {
            return fp;
        }
    }
    return Fingerprint();
}

DtlsSession::DtlsSession(DtlsWriteCallbackI* transport, Logger* logger) : TimerInterface(100)
	, transport_(transport)
	, logger_(logger) {
}

DtlsSession::~DtlsSession() {
	StopTimer();
    if (ssl_) {
        SSL_set_ex_data(ssl_, 0, nullptr);

        SSL_shutdown(ssl_);
        SSL_shutdown(ssl_);

        SSL_free(ssl_);
        ssl_ = nullptr;

        ssl_bio_from_network_ = nullptr;
        ssl_bio_to_network_ = nullptr;
    }
}

void DtlsSession::OnSslInfo(int where, int ret) {
	const int w = where & -SSL_ST_MASK;
	const char* role;

	if ((w & SSL_ST_CONNECT) != 0) {
		role = "client";
	} else if ((w & SSL_ST_ACCEPT) != 0) {
		role = "server";
	} else {
		role = "undefined";
	}

	if ((where & SSL_CB_LOOP) != 0) {
		LogInfof(logger_, "[role:%s, action:'%s']", role, SSL_state_string_long(ssl_));
	}
	else if ((where & SSL_CB_ALERT) != 0) {
		const char* alertType;

		switch (*SSL_alert_type_string(ret))
		{
			case 'W':
			{
				alertType = "warning";
				break;
			}

			case 'F':
			{
				alertType = "fatal";
				break;
			}

			default:
			{
				alertType = "undefined";
			}
		}

		if ((where & SSL_CB_READ) != 0) {
			LogWarnf(logger_, "received DTLS %s alert: %s", alertType, SSL_alert_desc_string_long(ret));
		}
		else if ((where & SSL_CB_WRITE) != 0)
		{
			LogDebugf(logger_, "sending DTLS %s alert: %s", alertType, SSL_alert_desc_string_long(ret));
		}
		else
		{
			LogInfof(logger_, "DTLS %s alert: %s", alertType, SSL_alert_desc_string_long(ret));
		}
	}
	else if ((where & SSL_CB_EXIT) != 0) {
		if (ret == 0) {
			LogInfof(logger_, "[role:%s, failed:'%s']", role, SSL_state_string_long(ssl_));
		} else if (ret < 0) {
			LogInfof(logger_, "role: %s, waiting:'%s']", role, SSL_state_string_long(ssl_));
		}
	} else if ((where & SSL_CB_HANDSHAKE_START) != 0) {
		LogInfof(logger_, "DTLS handshake start");
	} else if ((where & SSL_CB_HANDSHAKE_DONE) != 0) {
		LogInfof(logger_, "DTLS handshake done");
		this->handshake_done_ = true;
		if (!ProcessHandshake()) {
			LogErrorf(logger_, "ProcessHandshake failed");
		}
	}
}

int DtlsSession::SetRemoteFingerprint(const std::string& fingerprint) {
    // Implementation here
    // sha-256 77:B0:05:EC:26:1B:9F:85:B7:83:69:0A:57:2F:55:81:9C:60:1A:F7:A6:54:CC:A7:DF:16:61:E1:F8:72:39:F0
    std::vector<std::string> parts;
    int res = StringSplit(fingerprint, " ", parts);
    if (res != 2 || parts.size() != 2) {
        return -1;
    }
    FingerprintAlgorithm algorithm = GetFingerprintAlgorithm(parts[0]);
    if (algorithm == FingerprintAlgorithm::ALGORITHM_NONE) {
        return -1;
    }
    remote_fp_.algorithm = algorithm;
    remote_fp_.value = parts[1];
    LogInfof(logger_, "set remote finger print:%s", fingerprint.c_str());
    return 0;
}

bool DtlsSession::GenRemoteCertByRemoteFingerprint() {
	X509* certificate;
	uint8_t binaryFingerprint[EVP_MAX_MD_SIZE];
	unsigned int size{ 0 };
	char hexFingerprint[(EVP_MAX_MD_SIZE * 3) + 1];
	const EVP_MD* hashFunction;
	int ret;

	certificate = SSL_get_peer_certificate(ssl_);

	if (!certificate) {
		LogErrorf(logger_, "no certificate was provided by the peer");
		return false;
	}

	switch (remote_fp_.algorithm)
	{
		case FingerprintAlgorithm::ALGORITHM_SHA1:
		{
			hashFunction = EVP_sha1();
			break;
		}

		case FingerprintAlgorithm::ALGORITHM_SHA224:
		{
			hashFunction = EVP_sha224();
			break;
		}

		case FingerprintAlgorithm::ALGORITHM_SHA256:
		{
			hashFunction = EVP_sha256();
			break;
		}

		case FingerprintAlgorithm::ALGORITHM_SHA384:
		{
			hashFunction = EVP_sha384();
			break;
		}

		case FingerprintAlgorithm::ALGORITHM_SHA512:
		{
			hashFunction = EVP_sha512();
			break;
		}
		default:
		{
			return false;
		}
	}

	// Compare the remote fingerprint with the value given via signaling.
	ret = X509_digest(certificate, hashFunction, binaryFingerprint, &size);
	if (ret == 0) {
		LogErrorf(logger_, "X509_digest() failed");
		X509_free(certificate);
		return false;
	}

	// Convert to hexadecimal format in uppercase with colons.
	for (unsigned int i{ 0 }; i < size; ++i) {
		std::snprintf(hexFingerprint + (i * 3), 4, "%.2X:", binaryFingerprint[i]);
	}
	hexFingerprint[(size * 3) - 1] = '\0';

	if (remote_fp_.value != hexFingerprint) {
		LogErrorf(logger_,
		  "fingerprint in the remote certificate (%s) does not match the announced one (%s)",
		  hexFingerprint,
		  remote_fp_.value.c_str());
		X509_free(certificate);
		return false;
	}

	LogInfof(logger_, "valid remote fingerprint");

	// Get the remote certificate in PEM format.
	BIO* bio = BIO_new(BIO_s_mem());
	(void)BIO_set_close(bio, BIO_CLOSE);

	ret = PEM_write_bio_X509(bio, certificate);
	if (ret != 1) {
		LogErrorf(logger_, "PEM_write_bio_X509() failed");
		X509_free(certificate);
		BIO_free(bio);
		return false;
	}

	BUF_MEM* mem;

	BIO_get_mem_ptr(bio, &mem);
	if (!mem || !mem->data || mem->length == 0u) {
		LogErrorf(logger_, "BIO_get_mem_ptr() failed");
		X509_free(certificate);
		BIO_free(bio);
		return false;
	}

	remote_cert_ = std::string(mem->data, mem->length);

	X509_free(certificate);
	BIO_free(bio);
	return true;
}

void DtlsSession::SetRole(Role role) {
    // Implementation here
    role_ = role;
}

int DtlsSession::InitSession() {
	const int DTLS_MTU = 1000;
	ssl_ = SSL_new(ssl_ctx_);
	if (!ssl_) {
		return -1;
	}
	SSL_set_ex_data(ssl_, 0, static_cast<void*>(this));
	ssl_bio_from_network_ = BIO_new(BIO_s_mem());
	ssl_bio_to_network_ = BIO_new(BIO_s_mem());

	// Set the MTU so that we don't send packets that are too large with no fragmentation.
	SSL_set_mtu(ssl_, DTLS_MTU);
	DTLS_set_link_mtu(ssl_, DTLS_MTU);

	// Set callback handler for setting DTLS timer interval.
	DTLS_set_timer_cb(ssl_, OnSslDtlsTimer);
	BIO_set_callback_ex(ssl_bio_to_network_, OnSslBioOut);
	BIO_set_callback_arg(ssl_bio_to_network_, reinterpret_cast<char*>(this));
	struct timeval timeout;
    timeout.tv_sec = 1;   // 1 秒
    timeout.tv_usec = 0;
    BIO_ctrl(ssl_bio_from_network_, BIO_CTRL_DGRAM_SET_RECV_TIMEOUT, 0, &timeout);
    
	SSL_set_bio(ssl_, ssl_bio_from_network_, ssl_bio_to_network_);
	return 0;
}

int DtlsSession::Run() {
	LogInfof(logger_, "DtlsSession Run role:%d", static_cast<int>(role_));
	if (role_ == Role::ROLE_CLIENT) {
		StartTimer();
	} else if (role_ == Role::ROLE_SERVER) {
		SSL_set_accept_state(ssl_);
		SSL_do_handshake(ssl_);
	} else {
		return -1;
	}
	return 0;
}

void DtlsSession::SendDtlsMemData() {
	if (BIO_eof(ssl_bio_to_network_))
		return;

	int64_t read;
	char* data{ nullptr };

	read = BIO_get_mem_data(ssl_bio_to_network_, &data); // NOLINT

	if (read <= 0)
		return;

	transport_->OnDtlsTransportSendData(
	  reinterpret_cast<uint8_t*>(data), static_cast<size_t>(read), dtls_remote_addr_);

	(void)BIO_reset(ssl_bio_to_network_);

}

int DtlsSession::OnHandleDtlsData(const uint8_t* data, size_t len, UdpTuple addr) {
	int written;
	int read;

	if (handshake_done_) {
        LogWarnf(logger_, "Received DTLS data after handshake done, len:%zu, is_handshake:%d", 
            len, SSL_is_init_finished(ssl_) ? 0 : 1);
        
        // 检查是否是重传的握手消息
        if (len > 13 && data[0] >= 20 && data[0] <= 23) {
            uint8_t content_type = data[0];
            LogInfof(logger_, "DTLS content type: %d (20=ChangeCipherSpec, 21=Alert, 22=Handshake, 23=Application)", 
                content_type);
        }
		return 0;
	}
	dtls_remote_addr_ = addr;
		
	written = BIO_write(ssl_bio_from_network_, 
		static_cast<const void*>(data), static_cast<int>(len));

	if (written != static_cast<int>(len)) {
		LogWarnf(logger_, "OpenSSL BIO_write() wrote less (%zu bytes) than given data (%zu bytes)",
			static_cast<size_t>(written),len);
	}

	read = SSL_read(ssl_, static_cast<void*>(DtlsSession::ssl_read_buffer_), SslReadBufferSize);

	if (!CheckStatus(read)) {
		LogErrorf(logger_, "DtlsSession SSL_read() failed");
		return -1;
	}
	return 0;
}

bool DtlsSession::CheckStatus(int ret) {
	bool r = false;
	const int err = SSL_get_error(ssl_, ret);

	switch (err)
	{
		case SSL_ERROR_NONE:
			r = true;
			break;

		case SSL_ERROR_SSL:
		{
			LogErrorf(logger_, "SSL status: SSL_ERROR_SSL");
			// Dump OpenSSL error queue for more details.
			unsigned long openssl_err = ERR_get_error();
			if (openssl_err != 0) {
				char err_buf[256] = {0};
				ERR_error_string_n(openssl_err, err_buf, sizeof(err_buf));
				LogErrorf(logger_, "OpenSSL error: %s", err_buf);
			} else {
				LogInfof(logger_, "OpenSSL error queue empty");
			}
			// Log current SSL state for debugging.
			if (ssl_) {
				LogInfof(logger_, "SSL state: %s", SSL_state_string_long(ssl_));
			}
			// Log pending bytes in the outgoing BIO to see if there is data to flush.
			if (ssl_bio_to_network_) {
				size_t pending = BIO_ctrl_pending(ssl_bio_to_network_);
				LogInfof(logger_, "pending bytes in ssl_bio_to_network_: %d", pending);
			}
			r = false;
			break;
		}

		case SSL_ERROR_WANT_READ:
		{
			r = true;
			break;
		}

		case SSL_ERROR_WANT_WRITE:
		{
			// In a mem-BIO / non-blocking setup SSL may return WANT_WRITE to indicate
			// it needs to write data out. Treat this as non-fatal: flush any pending
			// DTLS data from the SSL BIO to the network and allow the caller to
			// continue processing.
			LogInfof(logger_, "SSL status: SSL_ERROR_WANT_WRITE - flushing outgoing DTLS data and treating as non-fatal");
			// Try to push any pending data produced by OpenSSL to the network.
			this->SendDtlsMemData();
			r = true;
			break;
		}

		case SSL_ERROR_WANT_X509_LOOKUP:
		{
			LogErrorf(logger_, "SSL status: SSL_ERROR_WANT_X509_LOOKUP");
			r = false;
			break;
		}

		case SSL_ERROR_SYSCALL:
		{
			LogErrorf(logger_, "SSL status: SSL_ERROR_SYSCALL");
			r = false;
			break;
		}

		case SSL_ERROR_ZERO_RETURN:
		{
			r = true;
			break;
		}

		case SSL_ERROR_WANT_CONNECT:
		{
			LogErrorf(logger_, "SSL status: SSL_ERROR_WANT_CONNECT");
			r = false;
			break;
		}

		case SSL_ERROR_WANT_ACCEPT:
		{
			LogErrorf(logger_, "SSL status: SSL_ERROR_WANT_ACCEPT");
			r = false;
			break;
		}

		default:
		{
			LogErrorf(logger_, "SSL status: unknown error");
			r = false;
		}
	}
	return r;
}

bool DtlsSession::ProcessHandshake() {
	bool r = false;

	r = GenRemoteCertByRemoteFingerprint();
	if (!r) {
		LogErrorf(logger_, "make remote cert error");
		return false;
	}
	auto srtpCryptoSuite = GenSslSrtpCryptoSuite();
	if (srtpCryptoSuite == SRTP_SESSION_CRYPTO_SUITE_INVALID) {
		LogErrorf(logger_, "generate ssl srtp crypto suite error");
		return false;
	}
	// Create SRTP session
	GenSrtpKeys(srtpCryptoSuite);

	return true;
}

SRtpSessionCryptoSuite DtlsSession::GenSslSrtpCryptoSuite() {
	SRtpSessionCryptoSuite crypto_suite = SRTP_SESSION_CRYPTO_SUITE_INVALID;

	// Ensure that the SRTP crypto suite has been negotiated.
	// NOTE: This is a OpenSSL type.
	const SRTP_PROTECTION_PROFILE* sslSrtpCryptoSuite = SSL_get_selected_srtp_profile(ssl_);

	if (!sslSrtpCryptoSuite)
	{
		return crypto_suite;
	}

	// Get the negotiated SRTP crypto suite.
	for (auto& srtpCryptoSuite : srtp_crypto_suites_)
	{
		SRtpCryptoSuiteMapEntry* cryptoSuiteEntry = std::addressof(srtpCryptoSuite);

		if (std::strcmp(sslSrtpCryptoSuite->name, cryptoSuiteEntry->name) == 0)
		{
			LogInfof(logger_, "chosen SRTP crypto suite: %s", cryptoSuiteEntry->name);

			crypto_suite = cryptoSuiteEntry->cryptoSuite;
		}
	}

	return crypto_suite;
}

void DtlsSession::GenSrtpKeys(SRtpSessionCryptoSuite crypto_suite) {
	size_t srtpKeyLength = 0;
	size_t srtpSaltLength = 0;
	size_t srtpMasterLength = 0;

	switch (crypto_suite)
	{
		case AEAD_AES_256_GCM:
		{
			srtpKeyLength    = SrtpAesGcm256MasterKeyLength;
			srtpSaltLength   = SrtpAesGcm256MasterSaltLength;
			srtpMasterLength = SrtpAesGcm256MasterLength;

			break;
		}

		case AEAD_AES_128_GCM:
		{
			srtpKeyLength    = SrtpAesGcm128MasterKeyLength;
			srtpSaltLength   = SrtpAesGcm128MasterSaltLength;
			srtpMasterLength = SrtpAesGcm128MasterLength;

			break;
		}

		case AES_CM_128_HMAC_SHA1_80:
		case AES_CM_128_HMAC_SHA1_32:
		{
			srtpKeyLength    = SrtpMasterKeyLength;
			srtpSaltLength   = SrtpMasterSaltLength;
			srtpMasterLength = SrtpMasterLength;

			break;
		}
		default:
		{
			LogErrorf(logger_, "unknown SRTP crypto suite:%d", crypto_suite);
			return;
		}
	}

	std::vector<uint8_t> srtpMaterial_vec(srtpMasterLength * 2);
	auto* srtpMaterial = srtpMaterial_vec.data();
	uint8_t* srtpLocalKey{ nullptr };
	uint8_t* srtpLocalSalt{ nullptr };
	uint8_t* srtpRemoteKey{ nullptr };
	uint8_t* srtpRemoteSalt{ nullptr };
	std::vector<uint8_t> srtpLocalMasterKey_vec(srtpMasterLength);
	std::vector<uint8_t> srtpRemoteMasterKey_vec(srtpMasterLength);
	auto* srtpLocalMasterKey  = srtpLocalMasterKey_vec.data();
	auto* srtpRemoteMasterKey = srtpRemoteMasterKey_vec.data();
	int ret;

	ret = SSL_export_keying_material(
	  ssl_, srtpMaterial, srtpMasterLength * 2, "EXTRACTOR-dtls_srtp", 19, nullptr, 0, 0);
	if (ret != 1) {
		LogErrorf(logger_, "SSL_export_keying_material() failed: %d", ret);
		return;
	}

	switch (role_)
	{
		case Role::ROLE_SERVER:
		{
			srtpRemoteKey  = srtpMaterial;
			srtpLocalKey   = srtpRemoteKey + srtpKeyLength;
			srtpRemoteSalt = srtpLocalKey + srtpKeyLength;
			srtpLocalSalt  = srtpRemoteSalt + srtpSaltLength;

			break;
		}

		case Role::ROLE_CLIENT:
		{
			srtpLocalKey   = srtpMaterial;
			srtpRemoteKey  = srtpLocalKey + srtpKeyLength;
			srtpLocalSalt  = srtpRemoteKey + srtpKeyLength;
			srtpRemoteSalt = srtpLocalSalt + srtpSaltLength;

			break;
		}

		default:
		{
			LogErrorf(logger_, "unknown DTLS role:%d", (int)role_);
		}
	}

	// Create the SRTP local master key.
	std::memcpy(srtpLocalMasterKey, srtpLocalKey, srtpKeyLength);
	std::memcpy(srtpLocalMasterKey + srtpKeyLength, srtpLocalSalt, srtpSaltLength);
	// Create the SRTP remote master key.
	std::memcpy(srtpRemoteMasterKey, srtpRemoteKey, srtpKeyLength);
	std::memcpy(srtpRemoteMasterKey + srtpKeyLength, srtpRemoteSalt, srtpSaltLength);

	dtls_connected_ = true;
	this->transport_->OnDtlsTransportConnected(
	  this,
	  crypto_suite,
	  srtpLocalMasterKey,
	  srtpMasterLength,
	  srtpRemoteMasterKey,
	  srtpMasterLength,
	  remote_cert_);
}

//implementation of timer interface
bool DtlsSession::OnTimer() {
	const int64_t kDtlsSendIntervalMs = 5*1000;
	if (!GetIceConnected()) {
		return true;
	}
	if (dtls_connected_) {
		LogInfof(logger_, "DtlsSession OnTimer dtls connected, stop timer that no need to send handshake data");
		return false;
	}
	int64_t now_ms = now_millisec();
	if (now_ms - last_dtls_send_ms_ >= kDtlsSendIntervalMs) {
		last_dtls_send_ms_ = now_ms;
		LogInfof(logger_, "DtlsSession OnTimer send DTLS handshake data");
		SSL_set_connect_state(ssl_);
		SSL_do_handshake(ssl_);
		SendDtlsMemData();
	}
    return true;
}

} // namespace cpp_streamer