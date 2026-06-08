# AS Number ↔ Raw Byte Conversion Reference

Quick reference for converting between AS numbers and the 6 raw bytes
returned by register 0x63 (offsets 2–7).

## Byte layout

```
Byte   Offset   High nibble   Low nibble
─────  ──────   ───────────   ──────────
[0]    2        A  (d1)       (unused)
[1]    3        D  (d4)       C  (d3)
[2]    4        E  (d5)       B  (d2)
[3]    5        G  (d7)       F  (d6)
[4]    6        J  (letter)   I  (s2)
[5]    7        K  (letter)   H  (s1)
```

## AS number: `AS d1 d2 d3 d4 d5 d6 d7 - s1 s2 L`

### Digits → bytes (encoding)

```
byte[0] = (d1 << 4)
byte[1] = (d4 << 4) | d3
byte[2] = (d5 << 4) | d2
byte[3] = (d7 << 4) | d6
byte[4] = (J  << 4) | s2       where J,K encode letter L
byte[5] = (K  << 4) | s1
```

### Letter encoding

```
Letter  →  J  K
──────     ─  ──
A          0  1
B          0  2
C          0  3
D          0  4
E          0  5
F          0  6
G          0  7
H          0  8
J          0  9
K          0  10
L          0  11
M          0  12
N          0  13
P          0  14
(none)     0  0
R          1  0
S          1  1
T          1  2
U          1  3
V          1  4
W          1  5
X          1  6
Y          1  7
Z          1  8
```

Note: Letters I and O are not used in Daikin AS numbers.

### Bytes → digits (decoding)

```
d1 = byte[0] >> 4
d2 = byte[2] & 0x0F
d3 = byte[1] & 0x0F
d4 = byte[1] >> 4
d5 = byte[2] >> 4
d6 = byte[3] & 0x0F
d7 = byte[3] >> 4
s1 = byte[5] & 0x0F
s2 = byte[4] & 0x0F
J  = byte[4] >> 4
K  = byte[5] >> 4
```

## Worked example

**Model**: EAVH16S18DA6V7  
**AS Number**: AS1709217-30A  
**Eeprom_Id**: `AS1709217-30 A 4P675168-30 A EAVH16S18DA6V7`

```
d1=1  d2=7  d3=0  d4=9  d5=2  d6=1  d7=7
s1=3  s2=0  L=A → J=0, K=1

byte[0] = (1<<4) | 0 = 0x10
byte[1] = (9<<4) | 0 = 0x90
byte[2] = (2<<4) | 7 = 0x27
byte[3] = (7<<4) | 1 = 0x71
byte[4] = (0<<4) | 0 = 0x00
byte[5] = (1<<4) | 3 = 0x13

Register 0x63 offsets 2–7: 10 90 27 71 00 13
```

Verify decoding:
```
AS = d1 d2 d3 d4 d5 d6 d7 - s1 s2 L
   = 1  7  0  9  2  1  7  - 3  0  A
   = AS1709217-30A  ✓
```
