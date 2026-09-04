// convert read registry value to the expected format based on convID
#include <string.h>
char buff[64];
class Converter
{
public:
    int RType = 802; // refrigerant for pressure->temp: 801=R410A, 802=R32, 803=R22

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

    double press2tempR410A(double d){
        return -8.4448460086362E-07*d*d*d*d*d*d + 0.000112833751855216*d*d*d*d*d - 0.00584955138495273*d*d*d*d + 0.149130933664499*d*d*d - 1.99373679674187*d*d + 16.0504901396224*d - 53.2854030662239;
    }
    double press2tempR22(double d){
        return -5.0232146925414E-06*d*d*d*d*d*d + 0.000475561916201275*d*d*d*d*d - 0.0175087997648461*d*d*d*d + 0.318620413943417*d*d*d - 3.0844373741869*d*d + 19.0217687284902*d - 42.3282361912494;
    }
    double convertPress2Temp(double data){// R32 default; R410A/R22 selected via RType (conv 800-803)
        if (RType == 801) return press2tempR410A(data);
        if (RType == 803) return press2tempR22(data);
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
            // Serial.printf("%f\n", dblData);
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
        case 120:
        {
            if (data[0] == 0 && data[1] == 128)
            {
                strcat(def->asString, "---");
                return;
            }
            // Interpret as signed 16-bit fixed point (value / 256.0)
            dblData = (double)getSignedValue(data, num, 0) / 256.0;
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

        case 213:
        {
            // O/U MPU / software ID nibble: 10-15 -> A-F, else decimal
            int v = data[0];
            if (v >= 10 && v <= 15)
                sprintf(def->asString, "%c", 'A' + (v - 10));
            else
                sprintf(def->asString, "%d", v);
            return;
        }

        case 214:
        {
            // EEPROM revision code: 0 -> blank, 1-24 -> A-X, else decimal
            int v = data[0];
            if (v == 0)
                strcat(def->asString, " ");
            else if (v >= 1 && v <= 24)
                sprintf(def->asString, "%c", 'A' + (v - 1));
            else
                sprintf(def->asString, "%d", v);
            return;
        }

        case 215:
        case 216:
		{
			int num = data[0] >> 4;
			int num2 = (int)(data[0] & 15);
            sprintf(def->asString,"{0:%d}{1:%d}", num, num2);
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
            break;
        case 402:
            dblData = (double)getSignedValue(data, num, 1);
            dblData = convertPress2Temp(dblData);
            break;
        case 403:
            dblData = (double)getSignedValue(data, num, 0) / 256.0;
            dblData = convertPress2Temp(dblData);
            break;
        case 404:
            dblData = (double)getSignedValue(data, num, 1) / 256.0;
            dblData = convertPress2Temp(dblData);
            break;
        case 405:
            dblData = (double)getSignedValue(data, num, 0) * 0.1;
            dblData = convertPress2Temp(dblData);
            break;
        case 406:
            dblData = (double)getSignedValue(data, num, 1) * 0.1;
            dblData = convertPress2Temp(dblData);
            break;

        case 212:
        {
            // capacity code -> printed value
            int v = data[0];
            switch (v) {
            case 0:   strcat(def->asString, "---"); break;
            case 251: strcat(def->asString, "280"); break;
            case 252: strcat(def->asString, "290"); break;
            case 253: strcat(def->asString, "300"); break;
            case 254: strcat(def->asString, "310"); break;
            case 255: strcat(def->asString, "320"); break;
            default:  sprintf(def->asString, "%d", v);
            }
            return;
        }

        case 219:
        {
            // capacity (kW class) code lookup
            int v = data[0];
            switch (v) {
            case 0: case 50: case 63: strcat(def->asString, "---"); break;
            case 17:  strcat(def->asString, "15");  break;
            case 22:  strcat(def->asString, "20");  break;
            case 28:  strcat(def->asString, "25");  break;
            case 36:  strcat(def->asString, "32");  break;
            case 45:  strcat(def->asString, "40");  break;
            case 56:  strcat(def->asString, "50");  break;
            case 71:  strcat(def->asString, "63");  break;
            case 80:  strcat(def->asString, "71");  break;
            case 90:  strcat(def->asString, "80");  break;
            case 112: strcat(def->asString, "100"); break;
            case 125: strcat(def->asString, "112"); break;
            case 140: strcat(def->asString, "125"); break;
            case 160: strcat(def->asString, "140"); break;
            case 180: strcat(def->asString, "160"); break;
            case 224: strcat(def->asString, "200"); break;
            case 251: strcat(def->asString, "250"); break;
            case 252: strcat(def->asString, "290"); break;
            case 253: strcat(def->asString, "300"); break;
            case 254: strcat(def->asString, "310"); break;
            case 255: strcat(def->asString, "320"); break;
            default:  sprintf(def->asString, "%d", v);
            }
            return;
        }

        case 310:
            dblData = (double)((data[0] & 0x70) >> 4); // bits 4-6
            break;
        case 311:
            dblData = (double)(data[0] & 0x07);        // low 3 bits
            break;

        case 314:
        {
            // 2-byte model code -> 3-char string via Conv51 table
            static const char conv51[32] = {
                '-','1','2','3','4','5','6','7','8','9','A','C','E','F','*','H',
                'J','E','L','U','*','P','P','P','U','2','A','E','F','*','*','*'};
            if (num < 2) { strcat(def->asString, "---"); return; }
            unsigned int val = ((unsigned int)data[1] << 8) | data[0];
            int i2 = (val >> 11) & 0x1F;
            int i3 = (val >> 6) & 0x1F;
            int i4 = val & 0x3F;
            char s[4];
            s[0] = conv51[i2];
            s[1] = conv51[i3];
            s[2] = (i4 > 32) ? conv51[0] : ((i4 < 32) ? conv51[i4] : '?');
            s[3] = 0;
            strcat(def->asString, s);
            return;
        }

        // refrigerant type: sets RType (used by convertPress2Temp) and prints the gas
        case 800:
            RType = 800; strcat(def->asString, "---");   return;
        case 801:
            RType = 801; strcat(def->asString, "R410A"); return;
        case 802:
            RType = 802; strcat(def->asString, "R32");   return;
        case 803:
            RType = 803; strcat(def->asString, "R22");   return;

        // enum-state convs not present; the def label
        // enumerates the integer states, so expose the raw value
        case 317:
        case 323:
        case 336:
            dblData = (double)data[0];
            break;

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
