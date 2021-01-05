### Serial protocol
 BaudRate = 9600
 DataBits = 8
 Parity = Even
 StopBits = One

### Command format
Eg: 03-40-60-5C
	0  1  2  3
Pos 0: Length of the command (-1 for the checksum)
Pos 1: 40, always.
Pos 2: Registry
Pos 3: CRC - SumAndInvert

### Response Message format
Eg:

40-60-13-80-00-18-00-00-00-00-C2-01-C1-01-E0-02-23-91-82-00-17

 0  1  2  3  4  5 ...

0: Protocol always 0x40

1: Registry

2: Length of the bytes after this one (ie registry value, ie total size -3)

3..end-1: Content of the registry

end: CRC - SumAndInvert

### Response description
Each registry contains several values. Name, format and offset of each value in each registry is described in file.

Then goes the data:

From the /include/ALTHERMAxxx.h the registry 0x61 contains the following labels:
{0x61,0,307,1,-1,"Data Enable/Disable"},
{0x61,1,152,1,-1,"Indoor Unit Address"},
{0x61,2,105,2,1,"Leaving water temp. before BUH (R1T)"},
{0x61,4,105,2,1,"Leaving water temp. after BUH (R2T)"},
{0x61,6,105,2,1,"Refrig. Temp. liquid side (R3T)"},
{0x61,8,105,2,1,"Inlet water temp.(R4T)"},
{0x61,10,105,2,1,"DHW tank temp. (R5T)"},
{0x61,12,105,2,1,"Indoor ambient temp. (R1T)"},
{0x61,14,105,2,1,"Ext. indoor ambient sensor (R6T)"},

Label format: {int registryIDp, int offsetp, int convidp, int dataSizep, int dataTypep, const char *labelp} 
Offset: position of the value in the registry (starting at byte 4)

Datatype: 1: temperature in (C), 2: pressure in (kgcm2), -1: everything else
Datasize: number of byte for value

convid tells you what conversion to do to get the actual value: See include/converters.h
eg:
105: little endian uint16 to be divided by 10 (from int to float with 1 decimal)
152: One byte integer unsigned
200-220: specific conversion
30X: Boolean value at bit X of the byte (little endian)

#### Example: analysing an response message
40-21-12-F9-00-95-00-E6-00-A8-CE-FF-67-01-1A-00-C4-FF-00-5E

40 - Protocol
21 - RegistryID is 0x21
12 - Length is 0x12

F9-00 - The registrys description tells us that for registry 0x21 we have uint16 at offset 0 an **A**nalog value
`105,,0,0x21,A,-1,437,INV primary current (A)`
So F9-00 codes for the *INV primary current (A)*. F9-00 little endian is 249. 
ConvID 105 tells us to divide by 10, we get 24.9 Amp.

### CRC
CommProtocolA adds a check-sum byte at the end of the COMMAND and Response messages. It goes like this:
```c++
uint8_t SumAndInvert(uint8_t const message[], int nBytes)
{
    uint8_t  sum = 0;
    while (nBytes-- > 0)
    {
        sum += *(message++);
    }
    return (~sum);
}   /* SumAndInvert() */
```


### Writing in registers:

read
LEN  33  73  0  OPCODE=1 OPERAND=1 PAGEN SETTINGN CRC
eg

char buf[] = {8,33,73,0,1,1,5,5,0x81};


write
LEN  33  70  0  OPCODE=1 OPERAND=1 PAGEN SETTINGN DATADATA CRC
receive 8 

```c#
private long r_Comm_Fcom_Opcode()
{
    checked
    {
        Mod_CommDefine.s_comm_data.sbuf = new byte[(int)Mod_CommDefine.s_comm_cmd.Fcmd.byten + 1];
        long num = 0L;
        Mod_CommDefine.s_comm_data.sbuf[(int)num] = (byte)Mod_CommDefine.s_comm_cmd.Fcmd.byten;
        num += 1L;
        Mod_CommDefine.s_comm_data.sbuf[(int)num] = Mod_CommDefine.s_comm_cmd.Fcmd.cmd;
        num += 1L;
        Mod_CommDefine.s_comm_data.sbuf[(int)num] = Mod_CommDefine.s_comm_cmd.Fcmd.subcmd;
        num += 1L;
        Mod_CommDefine.s_comm_data.sbuf[(int)num] = 0;
        num += 1L;
        Mod_CommDefine.s_comm_data.sbuf[(int)num] = Mod_CommDefine.s_comm_cmd.Fcmd.opcode;
        num += 1L;
        Mod_CommDefine.s_comm_data.sbuf[(int)num] = Mod_CommDefine.s_comm_cmd.Fcmd.operand;
        num += 1L;
        byte opcode = Mod_CommDefine.s_comm_cmd.Fcmd.opcode;
        if (opcode == 1)
        {
            byte operand = Mod_CommDefine.s_comm_cmd.Fcmd.operand;
            if (operand == 1)
            {
                Mod_CommDefine.s_comm_data.sbuf[(int)num] = Mod_CommDefine.s_comm_cmd.Fcmd.pageN;
                num += 1L;
                Mod_CommDefine.s_comm_data.sbuf[(int)num] = Mod_CommDefine.s_comm_cmd.Fcmd.settingN;
                num += 1L;
                long num2 = unchecked((long)(checked(Mod_CommDefine.s_comm_cmd.Fcmd.dataN - 1)));
                for (long num3 = 0L; num3 <= num2; num3 += 1L)
                {
                    Mod_CommDefine.s_comm_data.sbuf[(int)num] = Mod_CommDefine.s_comm_cmd.Fcmd.f_data[(int)num3];
                    num += 1L;
                }
                Mod_CommDefine.s_comm_data.Int_rcvN = 8L;
            }
        }
        long num4 = this.r_Comm_MakeSum(ref Mod_CommDefine.s_comm_data.sbuf, (int)num);
        Mod_CommDefine.s_comm_data.sbuf[(int)num] = (byte)((num4 & 255L) ^ 255L);
        num += 1L;
        Mod_CommDefine.s_comm_data.sndN = num;
        return num;
    }
}

```