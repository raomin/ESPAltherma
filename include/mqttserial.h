#ifndef mqttSerial_h
#define mqttSerial_h

#include "Stream.h"
#include <PubSubClient.h>

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
    inline int available(void){return 0;};
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
    if (!_client->connected()){
        return 0;
    }
    _client->publish(_topic,buffer,size);
    return size;
}

MQTTSerial::~MQTTSerial()
{
}

#endif