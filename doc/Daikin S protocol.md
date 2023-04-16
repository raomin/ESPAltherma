Some HP (older Daikin and Rotex?) use a slightly simpler protocol: Daikin S protocol.

It seems to be the case for [DAIKIN EKHBH016BA6WN year 2009](https://github.com/raomin/ESPAltherma/issues/46) and is confirmed on my 2009 Rotex RDLQ014AA6V3.

## Serial protocol
- BaudRate = 9600
- DataBits = 8
- Parity = Even
- StopBits = One

## Available registries

Dchecker supports registries 0x50, 0x53, 0x54 and 0x55. My RDLQ014AA6V3 also replies for 0x56 (still unknown values).

## Command format
Eg:
```
02-53-AA
 0  1  2
```
- Pos 0: Length of the command (-1 for the checksum)
- Pos 1: Registry ID
- Pos 2: CRC - SumAndInvert

## Response Message format

Reply length is hard-coded, depending on the queried registry:
- reg 0x53, 0x54, 0x55 -> 18 bytes
- reg 0x50, 0x56 -> 6 bytes

Eg:
```
53-01-00-00-00-00-00-00-00-00-00-00-00-00-00-00-00-ab
 0  1  2  3  4  5 ...
```

- 0: Registry ID
- 1..4/16: Content of the registry
- 5/17

## Response description

The response payload follows the same logic as [Daikin I protocol](Daikin%20I%20protocol.md).
