#pragma once
#ifdef ARDUINO
#include <pgmspace.h>
#endif

class LabelDef
{
public:
    int convid;
    int offset;
    int registryID;
    int dataSize;
    int dataType;
    const char *label;
    const char *name = nullptr; // display name in the chosen language (catalog.h); label when not set
    char *data;
    char asString[30];
    bool valid = false; // occupies existing alignment padding on ESP32; no converted bytes yet
    LabelDef(){};
    LabelDef(int registryIDp, int offsetp, int convidp, int dataSizep, int dataTypep, const char *labelp) : convid(convidp), offset(offsetp), registryID(registryIDp), dataSize(dataSizep), dataType(dataTypep), label(labelp){};

    static unsigned int bytesRequired(int conv, int size)
    {
        if (conv >= 800 && conv <= 805)
            return 0; // refrigerant metadata, no input bytes
        unsigned int needed = size > 0 ? (unsigned int)size : 1;
        bool word = conv == 114 || conv == 119 || conv == 314 ||
                    (((conv >= 101 && conv <= 118) || (conv >= 151 && conv <= 165) ||
                      conv == 218 || (conv >= 401 && conv <= 406)) && size != 1);
        return word && needed < 2 ? 2 : needed;
    }

    void invalidate() { valid = false; asString[0] = 0; }
};
