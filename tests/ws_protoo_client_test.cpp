// Simple integration-style unit test: start a local WebSocketServer,
// accept protoo requests and respond to a "join" request from WsProtooClient.

#include <cassert>
#include <uv.h>
#include <iostream>
#include "net/http/websocket/websocket_server.hpp"
#include "net/http/websocket/websocket_session.hpp"
#include "ws_message/ws_protoo_client.hpp"
#include "utils/logger.hpp"
#include "utils/timer.hpp"
#include "utils/json.hpp"

using namespace cpp_streamer;
using json = nlohmann::json;

// Server-side session callback: responds to protoo "join" requests.
class ServerSessionHandler : public WebSocketSessionCallBackI {
public:
    ServerSessionHandler(WebSocketSession* s) : session_(s) {}

    void OnReadData(int code, const uint8_t* data, size_t len) override {
        (void)code; (void)data; (void)len;
    }

    void OnReadText(int code, const std::string& text) override {
        if (code < 0) return;
        try {
            json j = json::parse(text);
            // handle protoo request
            if (j.value("request", false)) {
                int id = j.value("id", 0);
                std::string method = j.value("method", std::string());
                if (method == "join") {
                    // send a successful response
                    json resp;
                    resp["id"] = id;
                    resp["response"] = true;
                    resp["ok"] = true;
                    resp["data"] = json::object();
                    resp["data"]["result"] = "joined";
                    session_->AsyncWriteText(resp.dump());
                }
            }
        } catch (const std::exception& e) {
            std::cerr << "ServerSessionHandler parse error: " << e.what() << std::endl;
        }
    }

    void OnClose(int code, const std::string& desc) override {
        (void)code; (void)desc;
    }

private:
    WebSocketSession* session_ = nullptr;
};

// free function handler to register with WebSocketServer
static void OnTestWSHandle(const std::string& uri, WebSocketSession* session) {
    (void)uri;
    // advertise protoo protocol
    session->AddHeader("Sec-WebSocket-Protocol", "protoo");
    // attach our callback
    ServerSessionHandler* h = new ServerSessionHandler(session);
    session->SetSessionCallback(h);
}

// Client callback: sends join when connected, stops loop on response
class TestClientCb : public WsProtooClientCallbackI {
public:
    TestClientCb(uv_loop_t* loop) : loop_(loop), got_response(false), client(nullptr) {}
    void OnConnected() override {
        // send join request
        if (client) {
            json data;
            data["roomId"] = "test_room";
            data["userId"] = "u1";
            data["userName"] = "User1";
            client->SendRequest(1001, "join", data.dump());
        }
    }
    void OnResponse(const std::string& text) override {
        try {
            json j = json::parse(text);
            if (j.value("response", false) && j.value("id", 0) == 1001) {
                bool ok = j.value("ok", false);
                if (ok) {
                    got_response = true;
                    uv_stop(loop_);
                }
            }
        } catch (const std::exception& e) {
            std::cerr << "TestClientCb parse error: " << e.what() << std::endl;
        }
    }
    void OnNotification(const std::string& text) override {
        (void)text;
    }
    void OnClosed(int code, const std::string& reason) override {
        (void)code; (void)reason; uv_stop(loop_);
    }

    uv_loop_t* loop_;
    bool got_response;
    WsProtooClient* client;
};

int main() {
    uv_loop_t* loop = new uv_loop_t;
    uv_loop_init(loop);

    // initialize internal timer system used by server/client
    StreamerTimerInitialize(loop, 10);

    Logger logger("", LOGGER_DEBUG_LEVEL);

    const uint16_t port = 9002;
    // start server
    WebSocketServer server("127.0.0.1", port, loop, "certificate.crt", "private.key", &logger);
    server.AddHandle("/webrtc", OnTestWSHandle);

    // create client
    TestClientCb* cb = new TestClientCb(loop);
    WsProtooClient* client = new WsProtooClient(loop, "127.0.0.1", port, "/webrtc", true, &logger, cb);
    cb->client = client;

    // connect and run
    client->AsyncConnect();
    uv_run(loop, UV_RUN_DEFAULT);

    // verify
    assert(cb->got_response && "Did not receive join response from server");

    // cleanup
    delete client;
    delete cb;
    TimerInner::GetInstance()->Deinitialize();
    uv_loop_close(loop);
    delete loop;

    std::puts("ws_protoo_client_test: ALL PASSED");
    return 0;
}
