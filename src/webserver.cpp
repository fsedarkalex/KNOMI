#include <ESPAsyncWebServer.h>
#include <AsyncElegantOTA.h>
#include <ESPmDNS.h>
#include "esp_camera.h"
#include "camstate.h"
#include "knomi.h"

void initCamera(void);

static AsyncWebServer server(SERVER_PORT);

typedef struct {
    camera_fb_t * fb;
    size_t index;
} camera_frame_t;

#define PART_BOUNDARY "123456789000000000000987654321"
static const char* STREAM_CONTENT_TYPE = "multipart/x-mixed-replace; boundary=" PART_BOUNDARY;
static const char* STREAM_BOUNDARY = "\r\n--" PART_BOUNDARY "\r\n";
static const char* STREAM_BOUNDARY_FINAL = "\r\n--" PART_BOUNDARY "--\r\n";
static const char* STREAM_PART = "Content-Type: %s\r\nContent-Length: %u\r\n\r\n";
static const char * JPG_CONTENT_TYPE = "image/jpeg";
static const char * BMP_CONTENT_TYPE = "image/x-windows-bmp";

const char captive_html[] PROGMEM = R"rawliteral(<html>
<head>
<meta http-equiv="refresh" content="2;url=/" />
<title>For makers! By makers!</title>
</head>
<body>
You've successfully connected to the BTT KNOMI Screen. Click <a href="/">here</a> to go to the homepage.
</body>
</html>)rawliteral";

// This is a wrapper for a normal Async handler that allows the captive portal to intercept
// all requests regardless of their destination.
class CaptiveRequestHandler : public AsyncWebHandler {
public:
  CaptiveRequestHandler() {}
  virtual ~CaptiveRequestHandler() {}

  bool canHandle(AsyncWebServerRequest *request){
    //request->addInterestingHeader("ANY");
    return true;
  }

  void handleRequest(AsyncWebServerRequest *request) {
    request->send_P(200, "text/html", captive_html);
  }
};

class AsyncJpegStreamResponse: public AsyncAbstractResponse {
    private:
        camera_frame_t _frame;
        size_t _index;
        size_t _jpg_buf_len;
        uint8_t * _jpg_buf;
        uint64_t lastAsyncRequest;
        bool _mjpeg;
        bool _finished;
    public:
        AsyncJpegStreamResponse(bool returnMJPEG){
            _callback = nullptr;
            _code = 200;
            _contentLength = 0;
            _sendContentLength = false;
            _chunked = true;
            _index = 0;
            _jpg_buf_len = 0;
            _jpg_buf = NULL;
            _mjpeg = returnMJPEG;
            _finished = false;
            lastAsyncRequest = 0;
            memset(&_frame, 0, sizeof(camera_frame_t));

            if (_mjpeg) {
                _contentType = "multipart/x-mixed-replace; boundary=" PART_BOUNDARY;
            } else {
                _contentType = JPG_CONTENT_TYPE;
            }
        }
        ~AsyncJpegStreamResponse(){
            if(_frame.fb){
                if(_frame.fb->format != PIXFORMAT_JPEG){
                    free(_jpg_buf);
                }
                esp_camera_fb_return(_frame.fb);
            }
        }
        bool _sourceValid() const {
            return true;
        }
        virtual size_t _fillBuffer(uint8_t *buf, size_t maxLen) override {
            if (_finished) return 0;
            size_t ret = _content(buf, maxLen, _index);
            if(ret != RESPONSE_TRY_AGAIN){
                _index += ret;
            }
            return ret;
        }
        size_t _content(uint8_t *buffer, size_t maxLen, size_t index){
            if(!_frame.fb || _frame.index == _jpg_buf_len){
                if(index && _frame.fb){
                    uint64_t end = (uint64_t)micros();
                    lastAsyncRequest = end;
                    if(_frame.fb->format != PIXFORMAT_JPEG){
                        free(_jpg_buf);
                    }
                    esp_camera_fb_return(_frame.fb);
                    _frame.fb = NULL;
                    _jpg_buf_len = 0;
                    _jpg_buf = NULL;

                    if (!_mjpeg) {
                        _finished = true;
                        return 0;
                    }
                }

                if(_mjpeg && maxLen < (strlen(STREAM_BOUNDARY) + strlen(STREAM_PART) + strlen(JPG_CONTENT_TYPE) + 8)){
                    return RESPONSE_TRY_AGAIN;
                }

                _frame.index = 0;
                _frame.fb = esp_camera_fb_get();
                if (_frame.fb == NULL) {
                    log_e("Camera frame failed");
                    if (_mjpeg) {
                        size_t flen = strlen(STREAM_BOUNDARY_FINAL);
                        memcpy(buffer, STREAM_BOUNDARY_FINAL, flen);
                        _finished = true;
                        return flen;
                    }
                    return 0;
                }

                if(_frame.fb->format != PIXFORMAT_JPEG){
                    bool jpeg_converted = frame2jpg(_frame.fb, 100, &_jpg_buf, &_jpg_buf_len);
                    if(!jpeg_converted){
                        log_e("JPEG compression failed");
                        esp_camera_fb_return(_frame.fb);
                        _frame.fb = NULL;
                        _jpg_buf_len = 0;
                        _jpg_buf = NULL;
                        return 0;
                    }
                } else {
                    _jpg_buf_len = _frame.fb->len;
                    _jpg_buf = _frame.fb->buf;
                }

                if (_mjpeg) {
                    // ----- MJPEG header + frame -----
                    size_t blen = strlen(STREAM_BOUNDARY);
                    memcpy(buffer, STREAM_BOUNDARY, blen);
                    buffer += blen;

                    size_t hlen = sprintf((char *)buffer, STREAM_PART, JPG_CONTENT_TYPE, _jpg_buf_len);
                    buffer += hlen;

                    size_t copyLen = maxLen - hlen - blen;
                    if(copyLen > _jpg_buf_len) copyLen = _jpg_buf_len;
                    memcpy(buffer, _jpg_buf, copyLen);

                    _frame.index += copyLen;
                    return blen + hlen + copyLen;
                }
            }

            // ----- Frame-Data -----
            size_t available = _jpg_buf_len - _frame.index;
            if(maxLen > available){
                maxLen = available;
            }
            memcpy(buffer, _jpg_buf+_frame.index, maxLen);
            _frame.index += maxLen;

            return maxLen;
        }
};



// This function will populate variables on the main html index page.
// Variables include WiFi SSIDs/RSSIs and the current mode.
String knomi_html_processor(const String& var){
    String value = "";
    if (var == "wifi_list") {
        for (uint8_t i = 0; i < wifi_scan.count; i++) {
            value += R"rawliteral(<tr class="showpop" data-modal="modalOne"> <td class="ssid" >)rawliteral";
            value += wifi_scan.ssid[i];
            value += "</td> <td>";
            value += wifi_scan.rssi[i];
            value += "</td> <td>";
            value += wifi_scan.connected[i] ? "connected" : "";
            value += "</td> </tr>";
        }
    } else if (var == "ip") {
        String moonraker_ip = knomi_config.moonraker_ip;
        value = (moonraker_ip.isEmpty() ? "" : "value=") + moonraker_ip;
    } else if (var == "port") {
        String port = knomi_config.moonraker_port;
        value = port.isEmpty() ? "" : "value=" + port;
    } else if (var == "tool") {
        String tool = knomi_config.moonraker_tool;
        value = tool.isEmpty() ? "" : "value=" + tool;
    } else if (var == "ap_ssid") {
        String ap_ssid = knomi_config.ap_ssid;
        value = (ap_ssid.isEmpty() ? "" : "value=") + ap_ssid;
    } else if (var == "ap_password") {
        String ap_pwd = knomi_config.ap_pwd;
        value = (ap_pwd.isEmpty() ? "" : "value=") + ap_pwd;
    } else if (var == "hostname") {
        String hostname = knomi_config.hostname;
        value = hostname.isEmpty() ? "" : "value=" + hostname;
    } else if (var == "camstate") {
        char buf[16];
        sprintf(buf, "Error 0x%x", cam_error);
        value = (cam_error == ESP_OK ? "OK" : (cam_error == ESP_ERR_NOT_FOUND ? "Not found" : buf));
    } else if (var == "camavailable") {
        value = (cam_error == ESP_OK ? "true" : "false");
    } else  if (var == knomi_config.mode) {
        value = "selected";
    } else  if (var == "camres") {
        value = knomi_config.cam_res;
    } else  if (var.length() == 8 && var.substring(0, 7) == "cr_sel_" && var.substring(7, 8) == knomi_config.cam_res) {
        value = "selected";
    } else  if (var.length() == 8 && var.substring(0, 7) == "cq_sel_" && var.substring(7, 8) == knomi_config.cam_quality) {
        value = "selected";
    } else  if (var.length() == 9 && var.substring(0, 7) == "cq_sel_" && var.substring(7, 9) == knomi_config.cam_quality) {
        value = "selected";
    }
    return value;    // Could just be something between two normal $ signs in the HTML...
}

/*
 * mDNS
 * OTA
 */
#include "favicon.h"
#include "index_html.h"
typedef struct {
    const char * name;
    char * value;
    uint8_t v_len;
    uint16_t require;
} web_post_info_t;

web_post_info_t web_post_info[] = {
    {
        .name = "ssid",
        .value = knomi_config.sta_ssid,
        .v_len = sizeof(knomi_config.sta_ssid),
        .require = WEB_POST_WIFI_CONFIG_STA,
    },
    {
        .name = "password",
        .value = knomi_config.sta_pwd,
        .v_len = sizeof(knomi_config.sta_pwd),
        .require = WEB_POST_WIFI_CONFIG_STA,
    },
    {
        .name = "mode",
        .value = knomi_config.mode,
        .v_len = sizeof(knomi_config.mode),
        .require = WEB_POST_WIFI_CONFIG_MODE,
    },
    {
        .name = "ap_ssid",
        .value = knomi_config.ap_ssid,
        .v_len = sizeof(knomi_config.ap_ssid),
        .require = WEB_POST_WIFI_CONFIG_AP,
    },
    {
        .name = "ap_password",
        .value = knomi_config.ap_pwd,
        .v_len = sizeof(knomi_config.ap_pwd),
        .require = WEB_POST_WIFI_CONFIG_AP,
    },
    {
        .name = "hostname",
        .value = knomi_config.hostname,
        .v_len = sizeof(knomi_config.hostname),
        .require = WEB_POST_LOCAL_HOSTNAME,
    },
    {
        .name = "ip",
        .value = knomi_config.moonraker_ip,
        .v_len = sizeof(knomi_config.moonraker_ip),
        .require = WEB_POST_MOONRAKER,
    },
    {
        .name = "port",
        .value = knomi_config.moonraker_port,
        .v_len = sizeof(knomi_config.moonraker_port),
        .require = WEB_POST_MOONRAKER,
    },
    {
        .name = "tool",
        .value = knomi_config.moonraker_tool,
        .v_len = sizeof(knomi_config.moonraker_tool),
        .require = WEB_POST_MOONRAKER,
    },
    {
        .name = "camres",
        .value = knomi_config.cam_res,
        .v_len = sizeof(knomi_config.cam_res),
        .require = WEB_POST_CAMERA,
    },
    {
        .name = "camqual",
        .value = knomi_config.cam_quality,
        .v_len = sizeof(knomi_config.cam_quality),
        .require = WEB_POST_CAMERA,
    },
    {
        .name = "refresh",
        .value = NULL,
        .v_len = 0,
        .require = WEB_POST_WIFI_REFRESH,
    },
    {
        .name = "restart",
        .value = NULL,
        .v_len = 0,
        .require = WEB_POST_RESTART,
    },
    {
        .name = "factoryReset",
        .value = NULL,
        .v_len = 0,
        .require = WEB_POST_RESET,
    },
};

static AsyncWebServerRequest * wifi_refresh_request = NULL;
void webserver_wifi_refresh_callback(void) {
    if (wifi_refresh_request == NULL) return;
    wifi_refresh_request->send_P(200, "text/html", index_html, knomi_html_processor);
    wifi_refresh_request = NULL;
}

void streamJpg(AsyncWebServerRequest *request){
    if (cam_error != ESP_OK) {
        char buf[70];
        sprintf(buf, "Camera module is not connected or defective. Error Code: 0x%x", cam_error);
        request->send(503, "text/plain", buf);
    } else {
        AsyncJpegStreamResponse *response = new AsyncJpegStreamResponse(true);
        if(!response){
            request->send(501);
            return;
        }
        response->addHeader("Access-Control-Allow-Origin", "*");
        request->send(response);
    }
}

void camframeJpg(AsyncWebServerRequest *request){
    if (cam_error != ESP_OK) {
        char buf[70];
        sprintf(buf, "Camera module is not connected or defective. Error Code: 0x%x", cam_error);
        request->send(503, "text/plain", buf);
    } else {
        AsyncJpegStreamResponse *response = new AsyncJpegStreamResponse(false);
        if(!response){
            request->send(501);
            return;
        }
        response->addHeader("Access-Control-Allow-Origin", "*");
        request->send(response);
    }
}

void webserver_setup(void) {

    if(!MDNS.begin(knomi_config.hostname)) {
        Serial.println("Error starting mDNS");
    } else {
        Serial.println("mDNS start ok!");
    }
    AsyncElegantOTA.begin(&server);

    server.on("/favicon.ico", HTTP_GET, [](AsyncWebServerRequest *request){
        AsyncWebServerResponse *response = request->beginResponse_P(200, "image/x-icon", btt_logo_only_ico, sizeof(btt_logo_only_ico));
        request->send(response);
    });

    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send_P(200, "text/html", index_html, knomi_html_processor);
    });

    server.on("/", HTTP_POST, [](AsyncWebServerRequest *request){
        uint16_t post_require = WEB_POST_NULL;
        int paramsNr = request->params();
        for (int i = 0; i < paramsNr; i++) {
            AsyncWebParameter* p = request->getParam(i);
            Serial.printf("name: %s\r\n", p->name().c_str());
            Serial.printf("value: %s\r\n", p->value().c_str());

            for (uint8_t i = 0; i < ACOUNT(web_post_info); i++) {
                if (strcmp(web_post_info[i].name, p->name().c_str()) == 0) {
                    if (web_post_info[i].value == NULL) {
                        post_require |= web_post_info[i].require;
                        break;
                    }
                    if (strcmp(web_post_info[i].value, p->value().c_str()) != 0) {
                        post_require |= web_post_info[i].require;
                        strlcpy(web_post_info[i].value, p->value().c_str(), web_post_info[i].v_len);
                    }
                    break;
                }
            }
        }
        if ((post_require & WEB_POST_WIFI_CONFIG_STA)) {
            if (strcmp(knomi_config.mode, "ap") == 0) {
                strlcpy(knomi_config.mode, "sta", sizeof(knomi_config.mode));
                post_require |= WEB_POST_WIFI_CONFIG_MODE;
            }
            knomi_config.sta_auth = wifi_get_ahth_mode_from_scanned_list();
        }

        knomi_config_require_change(post_require);

        if (post_require & WEB_POST_WIFI_CONFIG_STA) {
            String sta_ssid = knomi_config.sta_ssid;
            String sta_pwd = knomi_config.sta_pwd;
            request->send(200, "text/html", "SSID: " + sta_ssid + "<br>PWD: " + sta_pwd + \
                    "<br>The BTT KNOMI will now attempt to connect to the specified network.<br>If it fails after 15s then this access point will be re-launched and you can connect to it to try again. <br><a href=\"/\">Return to Home Page</a>");
        } else if (post_require & WEB_POST_CAMERA){
            initCamera();
            request->send(200, "text/html", "Camera configuration has been modified. If there are any issues, please restart knomi. <br><a href=\"/\">Return to Home Page</a>");
        } else if (post_require & WEB_POST_LOCAL_HOSTNAME){
            request->send(200, "text/html", "The hostname needs to be restarted before it takes effect.<br>Please return to the home page and manually restart. <br><a href=\"/\">Return to Home Page</a>");
        } else if (post_require & WEB_POST_RESTART){
            request->send(200, "text/html", "KNOMI is restarting, please wait for the restart to complete and re-establish the connection. <br><a href=\"/\">Return to Home Page</a>");
        } else if (post_require & WEB_POST_RESET){
            request->send(200, "text/html", "KNOMI is restored to factory, please wait for the reset to complete and re-establish the connection. <br><a href=\"/\">Return to Home Page</a>");
        } else if (post_require & WEB_POST_WIFI_REFRESH) {
            wifi_refresh_request = request;
            wifi_scan_refresh_set_callback(webserver_wifi_refresh_callback);
        } else {
            request->send_P(200, "text/html", index_html, knomi_html_processor);
        }
    });
    server.on("/cam/stream", HTTP_GET, streamJpg);
    server.on("/cam/snapshot", HTTP_GET, camframeJpg);
    server.addHandler(new CaptiveRequestHandler()).setFilter(ON_AP_FILTER);
    server.begin();
}
