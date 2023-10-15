//
// Author: Alexander Sholohov <ra9yer@yahoo.com>
//
// License: MIT
//
#pragma once

#include <stdlib.h>

#include <vector>

// Ring buffer
class CBuffer
{
  public:
    CBuffer(size_t bufferSize);
    ~CBuffer() = default;
    void put(const short *buf, size_t len);
    size_t elementsAvailable() const;
    void peek(short *buf, size_t len) const;
    void peek(std::vector<short> &buffer, size_t len) const;
    void consume(size_t len);
    void reset();

  private:
    std::vector<short> m_buffer;
    size_t m_head;
    size_t m_tail;
};
