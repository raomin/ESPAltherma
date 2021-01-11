#include <pgmspace.h>
#define LABELDEF

class LabelDef
{
public:
    int convid;
    int offset;
    int registryID;
    int dataSize;
    int dataType;
    const char *label;
    char *data;
    char asString[30];
    LabelDef(){};
    LabelDef(int registryIDp, int offsetp, int convidp, int dataSizep, int dataTypep, const char *labelp) : convid(convidp), offset(offsetp), registryID(registryIDp), dataSize(dataSizep), dataType(dataTypep), label(labelp){};
};
