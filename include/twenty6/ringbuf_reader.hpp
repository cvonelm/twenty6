#pragma once

#include <lib26er/ringbuf.hpp>

namespace twenty6
{

class RingbufferReader
{
public:
        RingbufferReader(void* addr) : hdr_(reinterpret_cast<struct ringbuf_header*>(addr)), data_(reinterpret_cast<struct ringbuf_header*>(addr + PAGE_SIZE)
        bool readable() 
        {

        }
    private:
        struct ringbuf_header* hdr_;
        void *data_;
};
} // namespace twenty6
