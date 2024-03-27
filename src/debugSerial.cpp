#include "debugSerial.hpp"

DebugSerial debugSerial;

String* webSerialBuffer = nullptr;
bool webSerialConnection = false;

#define WEB_SERIAL_BUFFER_MAX_SIZE 2000

DebugSerial::DebugSerial()
{
    WebSerial.onConnect([](AsyncWebSocketClient *client)
    {
        webSerialConnection = true;
    });
}

size_t DebugSerial::write(const uint8_t *buffer, size_t size)
{
#if defined(ARDUINO_M5Stick_C) || defined(ARDUINO_M5Stack_Tough)
    if (M5.Lcd.getCursorY()+13>M5.Lcd.height()) {
        M5.Lcd.fillScreen(TFT_BLACK);
        M5.Lcd.setCursor(0,0);
    }
    M5.Lcd.print((const char*) buffer);
#endif

    if(!webSerialConnection) {
        if(webSerialBuffer == nullptr) {
            webSerialBuffer = new String();
        }

        webSerialBuffer->concat(reinterpret_cast<const char*>(buffer), size);

        if(webSerialBuffer->length() > WEB_SERIAL_BUFFER_MAX_SIZE) {
            *webSerialBuffer = webSerialBuffer->substring(webSerialBuffer->length() - WEB_SERIAL_BUFFER_MAX_SIZE);
        }
    } else if(webSerialConnection && webSerialBuffer != nullptr) {
        webSerialBuffer->concat(reinterpret_cast<const char*>(buffer), size);
        WebSerial.print(*webSerialBuffer);
        *webSerialBuffer = "";
        delete webSerialBuffer;
        webSerialBuffer = nullptr;
    } else {
        WebSerial.write(buffer, size);
    }

    Serial.write(buffer, size);

    return size;
}

DebugSerial::~DebugSerial()
{
}