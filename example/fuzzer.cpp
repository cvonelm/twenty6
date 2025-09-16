// SPDX-License-Identifier: MIT
//
// Simple two-threaded ringbuffer fuzzer
//
// Copyright (C) 2025 Technische Universität Dresden
// Christian von Elm <christian.von_elm@tu-dresden.de>

#include <twenty6/ringbuf.hpp>

#include <chrono>
#include <random>
#include <thread>

#include <cstdint>
#include <cstring>

extern "C"
{
#include <sys/types.h>
#include <unistd.h>
}

char* buf;

enum class RingbufReadOps : uint64_t
{
    CONSUME = 0,
    READ = 1,
    PEEK = 2,
};

enum class RingbufWriteOps : uint64_t
{
    PUBLISH = 0,
    RESERVE = 1
};

void read_thread(int fd)
{
    auto res = twenty6::Ringbuf::attach_ringbuf(fd);

    int pagesz = getpagesize();
    if (!res.has_value())
    {
        std::print("Could not initialize read size of ringbuf!\n");
        std::exit(1);
    }

    std::mt19937_64 rng(std::chrono::steady_clock::now().time_since_epoch().count());
    std::uniform_int_distribution<int> cmd_distrib(0, 2);
    std::uniform_int_distribution<int> msg_size_distrib(0, pagesz * 1.2);

    uint64_t local_read_pos = 0;

    /*
     * Randomly try to read, peek, or consume from the buffer.
     *
     * Compare the read values to the content in buf
     */
    while (1)
    {
        RingbufReadOps op = static_cast<RingbufReadOps>(cmd_distrib(rng));
        uint64_t msg_size = msg_size_distrib(rng);

        switch (op)
        {
        case RingbufReadOps::READ:
        {
            const std::byte* msg = res->read(msg_size);
            if (msg == nullptr)
            {
                // TODO: logic to check if the buf is really empty or really full
                continue;
            }

            if (memcmp(msg, buf + local_read_pos, msg_size) != 0)
            {
                std::print("Message and backing buffer are not equal!\n");
                std::exit(1);
            }

            local_read_pos = (local_read_pos + msg_size) % pagesz;
        }
        break;

        case RingbufReadOps::CONSUME:
            res->consume();

            break;
        case RingbufReadOps::PEEK:
        {
            const std::byte* msg = res->peek(msg_size);
            if (msg == nullptr)
            {
                continue;
            }

            if (memcmp(msg, buf + local_read_pos, msg_size) != 0)
            {
                std::print("Message and backing buffer are not equal!\n");
                std::exit(1);
            }
        }
        break;
        }
    }
}

int main(void)
{
    int pagesz = getpagesize();

    std::mt19937_64 rng(std::chrono::steady_clock::now().time_since_epoch().count());

    /*
     * buf is our "static ringbuffer" from which we read the content we write and to which
     * we compare the read value to.
     */
    buf = reinterpret_cast<char*>(malloc(pagesz * 2));

    assert(pagesz % sizeof(uint64_t) == 0);
    for (uint64_t i = 0; i < pagesz / sizeof(uint64_t); i++)
    {
        reinterpret_cast<uint64_t*>(buf)[i] = rng();
    }
    /*
     * Use the same "alloc twice" trick as used for the ringbuffer itself, so that
     * we do not have to think about wrap-around
     */
    memcpy(buf + pagesz, buf, pagesz);

    auto res = twenty6::Ringbuf::create_memfd_ringbuf(1);
    if (!res.has_value())
    {
        std::print("Could not create ringbuffer: {}\n", res.error().str);
    }

    /*
     * Start a separate thread for reading
     */
    std::thread read(read_thread, res->fd());
    uint64_t local_write_pos = 0;

    std::uniform_int_distribution<int> cmd_distrib(0, 1);
    std::uniform_int_distribution<int> msg_size_distrib(0, pagesz * 1.2);
    while (1)
    {
        uint64_t input = msg_size_distrib(rng);
        RingbufWriteOps command = static_cast<RingbufWriteOps>(cmd_distrib(rng));
        switch (command)
        {
        case RingbufWriteOps::PUBLISH:
            res->publish();
            break;
        case RingbufWriteOps::RESERVE:
        {
            std::byte* msg = res->reserve(input);
            if (msg == nullptr)
            {
                continue;
            }
            memcpy(msg, buf + local_write_pos, input);
            local_write_pos = (local_write_pos + input) % pagesz;
        }
        break;
        }
    }
}
