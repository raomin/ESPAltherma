Important::: I comes from A:   public class CommProtocolI : CommProtocolA

			this.BaudRate = 9600;
			this.DataBits = 8;
			this.Parity = Parity.Even;
			this.StopBits = StopBits.One;
			this.RecvTextTimeout = 500;
			this.RecvRetryMax = 3;
			this.RecvRetryInterval = 200;
			this.DetectCommand = "0, 0x03 @ 0x60 0x5C, 0, 0, 0";
			this.DetectResponse = "@ 0x60 0x* 0x* 0x* 0x* 0x* 0x* 0x* 0x* 0x* 0x* 0x* 0x* 0x* 0x* 0x* 0x*, 0, 18";
			this.SampCommands = new string[]
			{
				"0, 0x03 @ 0x00, 1, 0, 0",
				"0, 0x03 @ 0x10, 1, 0, 0",
				"0, 0x03 @ 0x11, 1, 0, 0",
				"1, 0x03 @ 0x20, 1, 0, 0",
				"1, 0x03 @ 0x30, 1, 0, 0",
				"0, 0x03 @ 0x60, 1, 0, 0",
				"0, 0x03 @ 0x61, 1, 0, 0",
				"0, 0x03 @ 0x62, 1, 0, 0",
				"0, 0x03 @ 0x63, 1, 0, 0",
				"0, 0x03 @ 0x64, 1, 0, 0",
				"0, 0x03 @ 0xA0, 1, 0, 0",
				"0, 0x03 @ 0xA1, 1, 0, 0"
				CmdKind (1: Continuous Response), Command, AddCRC?, RespLen, IgnErr
			};

			Key = Command value/Register *256

### Command format
Eg: 03-40-60-5C
	0  1  2  3
Pos 0: Length of the command (-1 for the checksum)
Pos 1: 40, always.
Pos 2: Register
Pos 3: CRC - SumAndInvert

### Response Message format
40-60-13-80-00-18-00-00-00-00-C2-01-C1-01-E0-02-23-91-82-00-17
 0  1  2  3  4  5 ...
0: Protocol always 0x40
1: Register
2: Length of the response
3-end: Content of the register

### Response description
Each register contains several values. Name, format and offset of each value in each register is described in file.

Then goes the data:

From the FILEXXX the register 0x61 contains the following values:
offset,dataid,datakind,datatype,labelId,labelStr
convid,convertedval,offset,dataid,datakind,datatype,labelId,labelStr
217		0	0x10		-1	521	Operation Mode
307		1	0x10	D	-1	1443	Thermostat ON/OFF
306		1	0x10	D	-1	934	Restart standby
305		1	0x10	D	-1	1372	Startup Control
304		1	0x10	D	-1	291	Defrost Operation
303		1	0x10	D	-1	519	Oil Return Operation
302		1	0x10	D	-1	909	Pressure equalizing operation
301		1	0x10	D	-1	294	Demand Signal
300		1	0x10	D	-1	447	Low noise control
203		4	0x10		-1	464	Error type
204		5	0x10		-1	461	Error Code
114		6	0x10	A	1	1425	Target Evap. Temp.
114		8	0x10	A	1	1422	Target Cond. Temp.
307		10	0x10	D	-1	303	Discharge Temp. Drop
310		10	0x10		-1	304	Discharge Temp. Protection Retry Qty
303		10	0x10	D	-1	250	Comp. INV Current Drop
311		10	0x10		-1	251	Comp. INV Current Protection Retry Qty
307		11	0x10	D	-1	379	HP Drop Control
310		11	0x10		-1	380	HP Protection Retry Qty
303		11	0x10	D	-1	452	LP Drop Control
311		11	0x10		-1	453	LP Protection Retry Qty
307		12	0x10	D	-1	354	Fin Temp. Drop Control
310		12	0x10		-1	355	Fin Temp. Protection Retry Qty
303		12	0x10	D	-1	541	Other Drop Control
311		12	0x10		-1	506	Not in use

Offset: position of the value in the register (starting at byte 4)

Datatype: 1: temperature in (C), 2: pressure in (kgcm2), -1: everything else
Datakind: A is analog, D is digital

convid tells you what conversion to do to get the actual value:
105: little endian uint16 to be divided by 10 (from int to float with 1 decimal)
152: One byte integer unsigned
200-220: specific conversion
30X: Boolean value at bit X of the byte (little endian)



#### Example: analysing an response message
40-21-12-F9-00-95-00-E6-00-A8-CE-FF-67-01-1A-00-C4-FF-00-5E

40 - Protocol
21 - RegisterID is 0x21
12 - Length is 0x12

F9-00 - The registers description tells us that for register 0x21 we have uint16 at offset 0 an **A**nalog value
`105,,0,0x21,A,-1,437,INV primary current (A)`
So F9-00 codes for the *INV primary current (A)*. F9-00 little endian is 249. 
ConvID 105 tells us to divide by 10, we get 24.9 Amp.



### CRC
CommProtocolA adds a check-sum byte at the end of the COMMAND (nothing for the response crc). It goes like this:
```c++
uint8_t
SumAndInvert(uint8_t const message[], int nBytes)
{
    uint8_t  sum = 0;
  

    while (nBytes-- > 0)
    {
        sum += *(message++);
    }
  
    return (~sum);

}   /* SumAndInvert() */
```
