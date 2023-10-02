// convert read registry value to the expected format based on convID
#include <string.h>
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
    void readRegistryValues(unsigned char *data, unsigned char protocol)
    {
        if (protocol == 'S')
        {
            // Registry ID is first byte
            readRegistryValues(data[0], data, 1);
        }
        else
        {
            readRegistryValues(data[1], data, 3);
        }
    }

    void readRegistryValues(char registryID, unsigned char *data, unsigned int offset)
    {
        // Serial.printf("For registry %d, we have these labels:\n", registryID);
        int num = 0;
        LabelDef *labels[128];
        getLabels(registryID, labels, num);

        for (int i = 0; i < num; i++)
        {
            unsigned char *input = data;
            input += labels[i]->offset + offset;
            convert(labels[i], input);
        }
    }

    double convertPress2Temp(double data){//assuming R32 gaz
        	double num = -2.6989493795556E-07 * data * data * data * data * data * data;
			double num2 = 4.26383417104661E-05 * data * data * data * data * data;
			double num3 = -0.00262978346547749 * data * data * data * data;
			double num4 = 0.0805858127503585 * data * data * data;
			double num5 = -1.31924457284073 * data * data;
			double num6 = 13.4157368435437 * data;
			double num7 = -51.1813342993155;
			return num + num2 + num3 + num4 + num5 + num6 + num7;
    }



    void convert(LabelDef *def, unsigned char *data)
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
            strlcat(def->asString, (char*)data, num);
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
        case 161:
            dblData = (double)getUnsignedValue(data, num, 1) * 0.5;
            break;
        case 162:
            dblData = (double)(getUnsignedValue(data, num, 1) - 64) * 0.5;
            break;
        case 163:
            dblData = (double)getUnsignedValue(data, num, 1) * 0.25;
            break;
        case 164:
            dblData = (double)getUnsignedValue(data, num, 1) * 5;
            break;
        case 165:
            dblData = (double)(getUnsignedValue(data, num, 0) & 16383);
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

        case 215:
        case 216:
		{
			int num = data[0] >> 4;
			int num2 = (int)(data[0] & 15);
            sprintf(def->asString,"{0:X}{1:X}", num, num2);
            return;
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
        case 312:
            dblData = convertTable312(data);
            break;
        case 315:
            convertTable315(data, def->asString);
            return;
        case 316:
            convertTable316(data, def->asString);
            return;

        // pressure to temp
        case 401:
            dblData = (double)getSignedValue(data, num, 0);
            dblData = convertPress2Temp(dblData);
        case 402:
            dblData = (double)getSignedValue(data, num, 1);
            dblData = convertPress2Temp(dblData);
        case 403:
            dblData = (double)getSignedValue(data, num, 0) / 256.0;
            dblData = convertPress2Temp(dblData);
        case 404:
            dblData = (double)getSignedValue(data, num, 1) / 256.0;
            dblData = convertPress2Temp(dblData);
        case 405:
            dblData = (double)getSignedValue(data, num, 0) * 0.1;
            dblData = convertPress2Temp(dblData);
        case 406:
            dblData = (double)getSignedValue(data, num, 1) * 0.1;
            dblData = convertPress2Temp(dblData);

        default:
            // conversion is not available
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
    void convertTable300(unsigned char *data, int tableID, char *ret)
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

    void convertTable203(unsigned char *data, char *ret)
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
            strcat(ret, "-");
            ;
        }
    }

    void convertTable204(unsigned char *data, char *ret)
    {
        char array[] = " ACEHFJLPU987654";
        char array2[] = "0123456789AHCJEF";
        int num = data[0] >> 4 & 15;
        int num2 = (int)(data[0] & 15);
        ret[0] = array[num];
        ret[1] = array2[num2];
        ret[2] = 0;
    }

    double convertTable312(unsigned char *data)
    {
        double dblData = ((unsigned char) (7 & data[0] >> 4) + (unsigned char) (15U & data[0])) / 16.0;
        if ((128 & data[0]) > 0)
        {
            dblData *= -1.0;
        }
        // Serial.printf("convertTable312 %02x -> %f \n", data[0], dblData);
        return dblData;
    }
    
    void convertTable315(unsigned char *data, char *ret)
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
            strcat(ret, "-");
        }
    }

    void convertTable316(unsigned char *data, char *ret)
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

    void convertTable200(unsigned char *data, char *ret)
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
    // 201
    void convertTable217(unsigned char *data, char *ret)
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

    unsigned short getUnsignedValue(unsigned char *data, int dataSize, int cnvflg)
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
    short getSignedValue(unsigned char *data, int datasize, int cnvflg)
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
