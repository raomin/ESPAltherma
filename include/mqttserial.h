#ifndef mqttSerial_h
#define mqttSerial_h
#include "Stream.h"
#include <PubSubClient.h>
#ifdef ARDUINO_M5Stick_C
#include <M5StickC.h>
#endif
class MQTTSerial: public Stream
{
private:
    /* data */
    PubSubClient* _client;
    char _topic[64];
public:
    inline void begin(PubSubClient* client,const char* topic){
    _client=client;
    strcpy(_topic,topic);
    };

    inline size_t write(uint8_t){return 0;};
    size_t write(const uint8_t *buffer, size_t size);
    inline int available(void){return _client->connected();};
    inline int availableForWrite(void){return 0;};
    inline int peek(void){return 0;};
    inline int read(void){return 0;};
    inline void flush(void){};
    inline size_t write(const char * s)
    {
        return write((uint8_t*) s, strlen(s));
    }
    inline size_t write(unsigned long n)
    {
        return write((uint8_t) n);
    }
    inline size_t write(long n)
    {
        return write((uint8_t) n);
    }
    inline size_t write(unsigned int n)
    {
        return write((uint8_t) n);
    }
    inline size_t write(int n)
    {
        return write((uint8_t) n);
    }


    MQTTSerial();
    ~MQTTSerial();
};

MQTTSerial mqttSerial;

MQTTSerial::MQTTSerial()
{
}
size_t MQTTSerial::write(const uint8_t *buffer, size_t size)
{
#ifdef ARDUINO_M5Stick_C
    M5.Lcd.println((const char*) buffer);
#endif
    if (WiFi.status() == WL_CONNECTED && _client->connected()){
        _client->publish(_topic,buffer,size);
    }
    Serial.write(buffer,size);
    return size;
}

MQTTSerial::~MQTTSerial()
{
}

#endif