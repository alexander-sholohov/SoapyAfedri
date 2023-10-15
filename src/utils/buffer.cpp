//
// Author: Alexander Sholohov <ra9yer@yahoo.com>
//
// License: MIT
//

#include "buffer.hpp"

#include <memory.h>

//---------------------------------------------------------------------------------------------------
CBuffer::CBuffer(size_t bufferSize)
{
    m_buffer.resize(bufferSize);
    m_head = 0;
    m_tail = 0;
}

//---------------------------------------------------------------------------------------------------
void CBuffer::consume(size_t len)
{
    if (elementsAvailable() < len)
    {
        // should never happen, but for some reason
        m_head = 0;
        m_tail = 0;
        return;
    }

    m_tail += len;
    if (m_tail >= m_buffer.size())
    {
        m_tail -= m_buffer.size();
    }
}

//---------------------------------------------------------------------------------------------------
void CBuffer::peek(short *buf, size_t len) const
{
    if (m_tail + len < m_buffer.size())
    {
        memcpy(buf, &m_buffer[m_tail], len * sizeof(short));
    }
    else
    {
        size_t len1 = m_buffer.size() - m_tail;
        size_t len2 = len - len1;
        memcpy(buf, &m_buffer[m_tail], len1 * sizeof(short));
        memcpy(buf + len1, &m_buffer[0], len2 * sizeof(short));
    }
}

//---------------------------------------------------------------------------------------------------
void CBuffer::peek(std::vector<short> &buffer, size_t len) const
{
    size_t len2 = (len > buffer.size()) ? buffer.size() : len; // limit length for some reason
    peek(&buffer[0], len2);
}

//---------------------------------------------------------------------------------------------------
void CBuffer::put(const short *buf, size_t len)
{
    // ignore huge data (should never happen)
    if (len >= m_buffer.size())
        return;

    size_t elementsAvailableBefore = elementsAvailable();

    if (m_head + len < m_buffer.size())
    {
        memcpy(&m_buffer[m_head], buf, len * sizeof(short));
        m_head += len;
    }
    else
    {
        size_t len1 = m_buffer.size() - m_head;
        size_t len2 = len - len1;
        memcpy(&m_buffer[m_head], buf, len1 * sizeof(short));
        memcpy(&m_buffer[0], buf + len1, len2 * sizeof(short));
        m_head = len2;
    }

    // check buffer overflow
    if (elementsAvailableBefore + len >= m_buffer.size())
    {
        // When overflow occurs we move tail next of head for some number of bytes (cut old unreaded data)
        // Why 32? Well, it can be at least 2 elements, bit 32 is a good value in terms of memory alignment.
        m_tail = m_head + 32;
        if (m_tail >= m_buffer.size())
        {
            m_tail -= m_buffer.size();
        }
    }
}

//---------------------------------------------------------------------------------------------------
size_t CBuffer::elementsAvailable() const
{
    int s = static_cast<int>(m_head) - static_cast<int>(m_tail);
    if (s < 0)
    {
        s += static_cast<int>(m_buffer.size());
    }
    return s;
}

//---------------------------------------------------------------------------------------------------
void CBuffer::reset()
{
    m_head = 0;
    m_tail = 0;
}
