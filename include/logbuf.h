#ifndef ESPALTHERMA_LOGBUF_H
#define ESPALTHERMA_LOGBUF_H

#include <Arduino.h>

// Thread-safe FIFO of log chunks.
// Any task can push; the main loop pops them and forwards them to MQTT (PubSubClient is not thread-safe).
// When full, the oldest chunks are dropped.
class LogRing
{
public:
  static const size_t CAPACITY = 4096;
  static const size_t MAX_CHUNK = 512;

  void push(const uint8_t *data, size_t len)
  {
    if (len == 0)
      return;
    if (len > MAX_CHUNK)
      len = MAX_CHUNK;
    lock();
    while (CAPACITY - _used < len + 2)
    {
      dropOldest();
    }
    putByte(len & 0xff);
    putByte(len >> 8);
    for (size_t i = 0; i < len; i++)
    {
      putByte(data[i]);
    }
    unlock();
  }

  // Copies the oldest chunk into out (NUL terminated) and removes it. Returns its length, 0 when empty.
  size_t pop(char *out, size_t size)
  {
    lock();
    if (_used == 0)
    {
      unlock();
      return 0;
    }
    size_t len = getByte() | (getByte() << 8);
    size_t copied = 0;
    for (size_t i = 0; i < len; i++)
    {
      uint8_t b = getByte();
      if (copied < size - 1)
        out[copied++] = b;
    }
    out[copied] = 0;
    unlock();
    return copied;
  }

  // Copies every chunk, oldest first, into out (NUL terminated) without removing them. Returns the length.
  size_t snapshot(char *out, size_t size)
  {
    lock();
    size_t pos = 0;
    size_t t = _tail;
    size_t remaining = _used;
    while (remaining >= 2)
    {
      size_t len = _buf[t] | (_buf[(t + 1) % CAPACITY] << 8);
      t = (t + 2) % CAPACITY;
      remaining -= 2 + len;
      for (size_t i = 0; i < len; i++)
      {
        if (pos < size - 1)
          out[pos++] = _buf[t];
        t = (t + 1) % CAPACITY;
      }
    }
    out[pos] = 0;
    unlock();
    return pos;
  }

private:
  uint8_t _buf[CAPACITY];
  size_t _head = 0; // next write
  size_t _tail = 0; // next read
  size_t _used = 0;
#ifdef ARDUINO_ARCH_ESP32
  portMUX_TYPE _mux = portMUX_INITIALIZER_UNLOCKED;
  void lock() { portENTER_CRITICAL(&_mux); }
  void unlock() { portEXIT_CRITICAL(&_mux); }
#else
  void lock() {}
  void unlock() {}
#endif

  void putByte(uint8_t b)
  {
    _buf[_head] = b;
    _head = (_head + 1) % CAPACITY;
    _used++;
  }

  uint8_t getByte()
  {
    uint8_t b = _buf[_tail];
    _tail = (_tail + 1) % CAPACITY;
    _used--;
    return b;
  }

  void dropOldest()
  {
    size_t len = getByte() | (getByte() << 8);
    _tail = (_tail + len) % CAPACITY;
    _used -= len;
  }
};

#endif
