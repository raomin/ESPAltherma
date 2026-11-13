#pragma once
#include <stdint.h>
#include <string>
#include <math.h>
#include "arduino_to_native.h"
using String=std::string;
class IPAddress {
    uint32_t value=0;
public:
    IPAddress(){} explicit IPAddress(uint32_t v):value(v){}
    operator uint32_t() const{return value;}
    String toString() const {char s[16];snprintf(s,sizeof(s),"%u.%u.%u.%u",value&255,(value>>8)&255,(value>>16)&255,value>>24);return s;}
    bool fromString(const char *s){unsigned a,b,c,d;if(sscanf(s,"%u.%u.%u.%u",&a,&b,&c,&d)!=4 || a>255 || b>255 || c>255 || d>255)return false;value=a|(b<<8)|(c<<16)|(d<<24);return true;}
};
#define BOARD_DEFAULT_RX_PIN 16
#define BOARD_DEFAULT_TX_PIN 17
#define ESPALTHERMA_CONFIG_NATIVE_TEST
#include "config.h"
