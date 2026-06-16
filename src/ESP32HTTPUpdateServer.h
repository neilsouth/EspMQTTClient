#ifndef ESP32_HTTP_UPDATE_SERVER_H
#define ESP32_HTTP_UPDATE_SERVER_H

/*
  Based on the HTTP update exemple of ESP32 core
*/

#include <WebServer.h>
#include <Update.h>

// Show hostname (client name) on the update page using client-side JS
#define ESP32_WEB_UPDATE_HTML "<html><body><h1 id='title'></h1><script>document.getElementById('title').textContent = 'Update ' + location.hostname;</script><form method='POST' action='' enctype='multipart/form-data'><input type='file' name='update'><input type='submit' value='Update'></form></body></html>"
#define ESP32_WEB_UPDATE_SUCCESS_RESPONSE "<META http-equiv=\"refresh\" content=\"10;URL=/\">Update Success! Rebooting...\n"

class ESP32HTTPUpdateServer
{
private:
  WebServer* _server;

  String _username;
  String _password;
  bool _serialDebugging;
  String _deviceName;

public:
  ESP32HTTPUpdateServer(bool serialDebugging = false)
  {
    _server = NULL;
    _username = "";
    _password = "";
    _deviceName = "";
    _serialDebugging = serialDebugging;
  }
  // Add optional deviceName so the update page can show a friendly name server-side
  void setup(WebServer* server, const char* path = "/", const char* username = "", const char* password = "", const char* deviceName = "")
  {
    _server = server;
    _username = username;
    _password = password;
    if (deviceName != NULL) _deviceName = String(deviceName);

    // Get of the index handling
    _server->on(path, HTTP_GET, [&]() {
      // Force authentication if a user and password are defined
      if (_username.length() > 0 && _password.length() > 0 && !_server->authenticate(_username.c_str(), _password.c_str()))
        return _server->requestAuthentication();

      _server->sendHeader("Connection", "close");
      // If a device name was provided, render it server-side; otherwise fall back to hostname JS
      if (_deviceName.length() > 0) {
        String html = "<html><body><h1>Update ";
        html += _deviceName;
        html += "</h1><form method='POST' action='' enctype='multipart/form-data'><input type='file' name='update'><input type='submit' value='Update'></form></body></html>";
        _server->send(200, "text/html", html);
      } else {
        _server->send(200, "text/html", ESP32_WEB_UPDATE_HTML);
      }
    });

    // Post of the file handling
    _server->on(path, HTTP_POST, [&]() {
      _server->client().setNoDelay(true);
      _server->send_P(200, "text/html", (Update.hasError()) ? "FAIL" : ESP32_WEB_UPDATE_SUCCESS_RESPONSE);
      delay(100);
      _server->client().stop();
      ESP.restart();
    }, [&]() {
      HTTPUpload& upload = _server->upload();

      if (upload.status == UPLOAD_FILE_START) 
      {
        // Check if we are authenticated
        if (!(_username.length() == 0 || _password.length() == 0 || _server->authenticate(_username.c_str(), _password.c_str())))
        {
          if (_serialDebugging)
            Serial.printf("Unauthenticated Update\n");

          return;
        }

        // Debugging message for upload start
        if (_serialDebugging) 
        {
          Serial.setDebugOutput(true);
          Serial.printf("Update: %s\n", upload.filename.c_str());
        }

        // Starting update
        bool error = Update.begin(UPDATE_SIZE_UNKNOWN);
        if (_serialDebugging && error)
          Update.printError(Serial);
      }
      else if (upload.status == UPLOAD_FILE_WRITE) 
      {
        if (Update.write(upload.buf, upload.currentSize) != upload.currentSize && _serialDebugging) 
          Update.printError(Serial);
      }
      else if (upload.status == UPLOAD_FILE_END) 
      {
        if (Update.end(true) && _serialDebugging)
          Serial.printf("Update Success: %u\nRebooting...\n", upload.totalSize);
        else if(_serialDebugging)
          Update.printError(Serial);

        if(_serialDebugging)
          Serial.setDebugOutput(false);
      }
      else if(_serialDebugging)
        Serial.printf("Update Failed Unexpectedly (likely broken connection): status=%d\n", upload.status);
    });

    _server->begin();
  }
};

#endif
