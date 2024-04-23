#include "X10ASerial.hpp"

DEFINE_SerialX10A;

void X10ASerial::begin(ulong baud, int8_t rxPin, int8_t txPin)
{
    SerialX10A.begin(baud, SERIAL_CONFIG, rxPin, txPin);
}

void X10ASerial::end()
{
    SerialX10A.end();
}

void X10ASerial::flush()
{
    SerialX10A.flush(SERIAL_FLUSH_TX_ONLY);
}

void X10ASerial::write(const uint8_t * data, size_t length)
{
    SerialX10A.write(data, length);
}

bool X10ASerial::available()
{
    return SerialX10A.available();
}

int X10ASerial::read()
{
    return SerialX10A.read();
}