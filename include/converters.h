//convert read registry value to the expected format based on convID
// #include <registrys.h>
#include <Arduino.h>
char buff[64];
class Converter
{
public:
    void getLabels(char registryID, LabelDef *ret[], int &num)
    {
        num = 0;
        for (auto &&label : labelDefs)
        {
            if (label.registryID == registryID)
            {
                ret[num++] = &label;
            }
        }
    }

    // Extract all values from the registry data response
    void readRegistryValues(char *data)
    {
        readRegistryValues(data[1], data);
    }

    void readRegistryValues(char registryID, char *data)
    {

        // Serial.printf("For registry %d, we have these labels:\n", registryID);
        int num = 0;
        LabelDef *labels[128];
        getLabels(registryID, labels, num);

        for (int i = 0; i < num; i++)
        {
            char *input = data;
            input += labels[i]->offset + 3;
            convert(labels[i], input);
        }
    }

    void convert(LabelDef *def, char *data)
    {
        def->asString[0] = {0};
        int convId = def->convid;
        int num = def->dataSize;
        double dblData = NAN;
        Serial.print("Converting from:");
        for (int i = 0; i < num; i++)
        {
            Serial.printf(" 0x%02x ", data[i]);
        }

        switch (convId)
        {
        case 100:
            strlcat(def->asString, data, num);
            return;
        case 101:
            dblData = (double)getSignedValue(data, num, 0);
            break;
        case 102:
            dblData = (double)getSignedValue(data, num, 1);
            break;
        case 103:
            dblData = (double)getSignedValue(data, num, 0) / 256.0;
            break;
        case 104:
            dblData = (double)getSignedValue(data, num, 1) / 256.0;
            break;
        case 105:
            dblData = (double)getSignedValue(data, num, 0) * 0.1;
            Serial.printf("%f\n", dblData);
            break;
        case 106:
            dblData = (double)getSignedValue(data, num, 1) * 0.1;
            break;
        case 107:
            dblData = (double)getSignedValue(data, num, 0) * 0.1;
            if (dblData == -3276.8)
            {
                strcat(def->asString, "---");
                return;
            }
            break;
        case 108:
            dblData = (double)getSignedValue(data, num, 1) * 0.1;
            if (dblData == -3276.8)
            {
                strcat(def->asString, "---");
                return;
            }
            break;
        case 109:
            dblData = (double)getSignedValue(data, num, 0) / 256.0 * 2.0;
            break;
        case 110:
            dblData = (double)getSignedValue(data, num, 1) / 256.0 * 2.0;
            break;
        case 111:
            dblData = (double)getSignedValue(data, num, 1) * 0.5;
            break;
        case 112:
            dblData = (double)(getSignedValue(data, num, 1) - 64) * 0.5;
            break;
        case 113:
            dblData = (double)getSignedValue(data, num, 1) * 0.25;
            break;
        case 114:
        {
            if (data[0] == 0 && data[1] == 128)
            {
                strcat(def->asString, "---");
                return;
            }
            unsigned short num2 = (unsigned short)((int)data[1] * 256);
            num2 |= (unsigned short)data[0];
            if ((data[1] & 128) != 0)
            {
                num2 = ~(num2 - 1);
            }
            dblData = (double)((num2 & 65280) / 256);
            dblData += (double)(num2 & 255) / 256.0;
            dblData *= 10.0;
            if ((data[1] & 128) != 0)
            {
                dblData *= -1.0;
            }
            break;
        }
        case 115:
            dblData = (double)getSignedValue(data, num, 0) / 2560.0;
            break;
        case 116:
            dblData = (double)getSignedValue(data, num, 1) / 2560.0;
            break;
        case 117:
            dblData = (double)getSignedValue(data, num, 0) * 0.01;
            break;
        case 118:
            dblData = (double)getSignedValue(data, num, 1) * 0.01;
            break;
        case 119:
        {
            if (data[0] == 0 && data[1] == 128)
            {
                strcat(def->asString, "---");
                return;
            }
            unsigned short num3 = (unsigned short)((int)data[1] * 256);
            num3 |= (unsigned short)(data[0] & 127);
            dblData = (double)((num3 & 65280) / 256);
            dblData += (double)(num3 & 255) / 256.0;
            break;
        }
        case 151:
            dblData = (double)getUnsignedValue(data, num, 0);
            break;
        case 152:
            dblData = (double)getUnsignedValue(data, num, 1);
            break;
        case 153:
            dblData = (double)getUnsignedValue(data, num, 0) / 256.0;
            break;
        case 154:
            dblData = (double)getUnsignedValue(data, num, 1) / 256.0;
            break;
        case 155:
            dblData = (double)getUnsignedValue(data, num, 0) * 0.1;
            break;
        case 156:
            dblData = (double)getUnsignedValue(data, num, 1) * 0.1;
            break;
        case 157:
            dblData = (double)getUnsignedValue(data, num, 0) / 256.0 * 2.0;
            break;
        case 158:
            dblData = (double)getUnsignedValue(data, num, 1) / 256.0 * 2.0;
            break;
        case 200:
            convertTable200(data, def->asString);
            return;
        case 203:
            convertTable203(data, def->asString);
            return;
        case 204:
            convertTable204(data, def->asString);
            return;
        case 211:
            if (data == 0)
            {
                strcat(def->asString, "OFF");
                return;
            }
            else
            {
                dblData = (uint)(double)data[0];
                break;
            }

        case 201:
        case 217:
            convertTable217(data, def->asString);
            return;
        case 300:
        case 301:
        case 302:
        case 303:
        case 304:
        case 305:
        case 306:
        case 307:
            convertTable300(data, def->convid, def->asString);
            return;
        case 315:
            convertTable315(data, def->asString);
            return;
        case 316:
            convertTable316(data, def->asString);
            return;
        default:
            //conversion is not available
            sprintf(def->asString, "Conv %d not avail.", convId);
            return;
        }
        if (dblData != NAN)
        {
            sprintf(def->asString, "%g", dblData);
        }
        Serial.printf("-> %s\n", def->asString);
    }

private:
    void convertTable300(char *data, int tableID, char *ret)
    {
        Serial.printf("Bin Conv %02x with tableID %d \n", data[0], tableID);
        char b = 1;
        b = (char)(b << tableID % 10);
        if ((data[0] & b) > 0)
        {
            strcat(ret, "ON");
        }
        else
        {
            strcat(ret, "OFF");
        }
        return;
    }

    void convertTable203(char *data, char *ret)
    {
        switch (data[0])
        {
        case 0:
            strcat(ret, "Normal");
            break;
        case 1:
            strcat(ret, "Error");
            break;
        case 2:
            strcat(ret, "Warning");
            break;
        case 3:
            strcat(ret, "Caution");
            break;
        default:
            strcat(ret, "");
            ;
        }
    }

    void convertTable204(char *data, char *ret)
    {
        char array[] = " ACEHFJLPU987654";
        char array2[] = "0123456789AHCJEF";
        int num = data[0] >> 4 & 15;
        int num2 = (int)(data[0] & 15);
        ret[0] = array[num];
        ret[1] = array2[num2];
        ret[2] = 0;
    }

    void convertTable315(char *data, char *ret)
    {
        char b = 240 & data[0];
        b = (char)(b >> 4);
        switch (b)
        {
        case 0:
            strcat(ret, "Stop");
            break;
        case 1:
            strcat(ret, "Heating");
            break;
        case 2:
            strcat(ret, "Cooling");
            break;
        case 3:
            strcat(ret, "??");
            break;
        case 4:
            strcat(ret, "DHW");
            break;
        case 5:
            strcat(ret, "Heating + DHW");
            break;
        case 6:
            strcat(ret, "Cooling + DHW");
            break;
        default:
            strcat(ret, "");
        }
    }

    void convertTable316(char *data, char *ret)
    {
        char b = 240 & data[0];
        b = (char)(b >> 4);
        switch (b)
        {
        case 0:
            strcat(ret, "H/P only");
            break;
        case 1:
            strcat(ret, "Hybrid");
            break;
        case 2:
            strcat(ret, "Boiler only");
            break;
        default:
            strcat(ret, "Unknown");
        }
    }

    void convertTable200(char *data, char *ret)
    {
        if (data[0] == 0)
        {
            strcat(ret, "OFF");
        }
        else
        {
            strcat(ret, "ON");
        }
    }
    //201
    void convertTable217(char *data, char *ret)
    {
        char r217[][30] = {"Fan Only",
                           "Heating",
                           "Cooling",
                           "Auto",
                           "Ventilation",
                           "Auto Cool",
                           "Auto Heat",
                           "Dry",
                           "Aux.",
                           "Cooling Storage",
                           "Heating Storage",
                           "UseStrdThrm(cl)1",
                           "UseStrdThrm(cl)2",
                           "UseStrdThrm(cl)3",
                           "UseStrdThrm(cl)4",
                           "UseStrdThrm(ht)1",
                           "UseStrdThrm(ht)2",
                           "UseStrdThrm(ht)3",
                           "UseStrdThrm(ht)4"};
        sprintf(ret, r217[(int)data[0]]);
    }

    unsigned short getUnsignedValue(char *data, int dataSize, int cnvflg)
    {
        unsigned short result;
        if (dataSize == 1)
        {
            result = (unsigned short)data[0];
        }
        else if (cnvflg == 0)
        {
            result = ((unsigned short)(data[1] << 8) | (unsigned short)data[0]);
        }
        else
        {
            result = ((unsigned short)(data[0] << 8) | (unsigned short)data[1]);
        }
        return result;
    }
    short getSignedValue(char *data, int datasize, int cnvflg)
    {
        unsigned short num = getUnsignedValue(data, datasize, cnvflg);
        short result = (short)num;
        if ((num & 32768) != 0)
        {
            num = ~num;
            num += 1;
            result = (short)((int)num * -1);
        }
        return result;
    }
};