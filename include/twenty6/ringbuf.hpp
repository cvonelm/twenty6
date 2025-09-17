// SPDX-License-Identifier: MIT
//
// Main Ringbuf class for interfacing with the twenty6 ringbuffer
//
// Copyright (C) 2025 Technische Universität Dresden
// Christian von Elm <christian.von_elm@tu-dresden.de>

#pragma once

#include <stdexcept>
#include <twenty6/types.hpp>

#include <fmt/core.h>

#include <algorithm>
#include <cassert>
#include <iostream>
#include <vector>

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
extern "C"
{
#include <sys/mman.h>
#include <sys/types.h>
#include <unistd.h>
}

namespace twenty6
{

typedef void (*watermark_cb_fn)(void*);

class Ringbuf
{
public:
    static Ringbuf create_memfd_ringbuf(size_t pages)
    {
        int fd = memfd_create("", 0);
        if (fd == -1)
        {
            throw std::runtime_error(
                fmt::format("Can not create memfd for Ringbuffer: {}", strerror(errno)));
        }
        if (ftruncate(fd, getpagesize() * (pages + 1)) == -1)
        {
            throw std::runtime_error(fmt::format("Can not set size of ring buffer to {} pages: {}",
                                                 pages, strerror(errno)));
        }

        auto rb = Ringbuf::attach_ringbuf(fd);

        rb.owns_fd_ = true;

        rb.hdr_->size = pages * getpagesize();
        rb.hdr_->version = 1;
        rb.hdr_->head = 0;
        rb.hdr_->tail = 0;

        return rb;
    }

    static Ringbuf attach_ringbuf(int fd)
    {

        off_t filesize = lseek(fd, 0, SEEK_END);

        if (filesize == -1)
        {
            throw std::runtime_error(
                fmt::format("Could not get size of underlying file: {},", strerror(errno)));
        }

        if (lseek(fd, 0, SEEK_CUR) == -1)
        {
            throw std::runtime_error(
                fmt::format("Could not rewind underlying file: {},", strerror(errno)));
        }

        if (filesize % getpagesize() != 0)
        {
            throw std::runtime_error("The file size must be a multiple of the page size!");
        }

        if (filesize == getpagesize())
        {
            throw std::runtime_error(
                ("The data portion of the ring buffer must be at least one page big!"));
        }

        Ringbuf rb;
        rb.fd_ = fd;

        uint64_t data_size = filesize - getpagesize();
        void* first_mapping =
            mmap(nullptr, data_size * 2 + getpagesize(), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
        if (first_mapping == MAP_FAILED)
        {
            throw std::runtime_error(
                fmt::format("Could not create ringbuffer mapping! {}\n", strerror(errno)));
        }

        void* second_mapping =
            mmap(reinterpret_cast<std::byte*>(first_mapping) + getpagesize() + data_size, data_size,
                 PROT_READ | PROT_WRITE, MAP_SHARED | MAP_FIXED, fd, getpagesize());

        if (second_mapping == MAP_FAILED)
        {
            throw std::runtime_error(
                fmt::format("Could not create ringbuffer mapping! {}\n", strerror(errno)));
        }
        rb.hdr_ = reinterpret_cast<struct ringbuf_header*>(first_mapping);
        rb.data_ = reinterpret_cast<std::byte*>(rb.hdr_) + getpagesize();

        return rb;
    }

    int fd()
    {
        return fd_;
    }

    uint64_t size()
    {
        return hdr_->size;
    }

    /*
     * Sets a high watermark for the ring buffer.
     * On a write operation that fills the buffer beyond "watermark" bytes,
     * watermark_cb(payload) is called
     */
    void set_watermark(uint64_t watermark, watermark_cb_fn cb, void* payload)
    {
        if (watermark != 0 && cb == nullptr)
        {
            throw std::runtime_error(
                "If watermark is not zero, you must set the callback function!\n");
        }

        watermark_ = watermark;
        watermark_cb_ = cb;
        watermark_payload_ = payload;
    }

    void print()
    {
        enum class PARTS
        {
            TAIL,
            LOCAL_TAIL,
            HEAD,
            LOCAL_HEAD,
        };

        std::vector<std::pair<PARTS, uint64_t>> contents;

        contents.push_back({ PARTS::HEAD, hdr_->head.load() });
        contents.push_back({ PARTS::LOCAL_HEAD, local_head_ });
        contents.push_back({ PARTS::TAIL, hdr_->tail.load() });
        contents.push_back({ PARTS::LOCAL_TAIL, local_tail_ });

        std::sort(contents.begin(), contents.end(), [](auto& lhs, auto& rhs) {
            if (lhs.second == rhs.second)
            {
                return lhs.first < rhs.first;
            }
            return lhs.second < rhs.second;
        });

        std::vector<std::string> print_parts;

        auto prev = contents.back();
        uint64_t consumed = 0;
        for (auto cur : contents)
        {
            if (cur.second - consumed == 0)
            {
                consumed = cur.second;
                prev = cur;
                continue;
            }
            switch (cur.first)
            {
            case PARTS::LOCAL_HEAD:
                print_parts.push_back(fmt::format("reserved: {}", cur.second - consumed));
                break;
            case PARTS::TAIL:
                print_parts.push_back(fmt::format("free : {}", cur.second - consumed));
                break;
            case PARTS::LOCAL_TAIL:
                print_parts.push_back(fmt::format("consumed: {}", cur.second - consumed));
                break;
            case PARTS::HEAD:
                print_parts.push_back(fmt::format("used: {}", cur.second - consumed));
                break;
            }
            consumed = cur.second;
            prev = cur;
        }

        switch (contents[0].first)
        {
        case PARTS::HEAD:
            print_parts.push_back(fmt::format("used: {}", hdr_->size - consumed));
            break;
        case PARTS::TAIL:
            print_parts.push_back(fmt::format("free: {}", hdr_->size - consumed));
            break;
        case PARTS::LOCAL_TAIL:
            print_parts.push_back(fmt::format("consumed: {}", hdr_->size - consumed));
            break;
        case PARTS::LOCAL_HEAD:
            print_parts.push_back(fmt::format("reserved: {}", hdr_->size - consumed));
            break;
        }

        std::cerr << "[ " << std::endl;
        for (auto& part : print_parts)
        {
            std::cerr << part << " ";
        }
        std::cerr << "]" << std::endl;
    }

    /*
     * reserves size bytes on the ring buffer
     *
     * Returns:
     *  - ptr to size bytes, on the ringbuffer, or nullptr, if no space is left in the buffer.
     */
    std::byte* reserve(size_t size)
    {
        if (size <= 0)
        {
            return nullptr;
        }

        uint64_t tail = hdr_->tail.load();

        if (local_head_ >= tail)
        {
            if (local_head_ + size >= tail + hdr_->size)
            {
                return nullptr;
            }
        }
        else
        {
            if (local_head_ + size >= tail)
            {
                return nullptr;
            }
        }

        uint64_t new_head = (local_head_ + size) % hdr_->size;

        std::byte* res = data_ + local_head_;

        local_head_ = new_head;

        return res;
    }

    /*
     * Make all the data reserve()d since the last call of publish() available
     */

    bool publish()
    {
        uint64_t tail = hdr_->tail.load();
        hdr_->head.store(local_head_);

        if (watermark_ != 0)
        {
            if (get_fill() > watermark_)
            {
                watermark_cb_(watermark_payload_);
            }
        }
        return true;
    }

    /*
     * Returns the current head of the ring buffer without moving forward the
     * buffer
     *
     * Errors:
     *  - There are not size bytes to peek at in the buffer, returns nullptr
     */
    const std::byte* peek(size_t size)
    {
        uint64_t head = hdr_->head.load();

        if (local_tail_ <= head)
        {
            if (local_tail_ + size > head)
            {
                return nullptr;
            }
        }
        else /* tail > head */
        {
            if (local_tail_ + size > head + hdr_->size)
            {
                return nullptr;
            }
        }
        return data_ + local_tail_;
    }

    /*
     * Returns the current head of the ring buffer, consuming size bytes.
     *
     * Errors:
     *  - There are not size bytes to read from the buffer, returns nullptr
     */
    const std::byte* read(size_t size)
    {
        auto* ptr = peek(size);
        if (ptr == nullptr)
        {
            return nullptr;
        }
        local_tail_ = (local_tail_ + size) % hdr_->size;
        return ptr;
    }

    /*
     * Consumes the reads since the last call to consume(). After consume is called,
     * they can be overwritten with new data
     */

    bool consume()
    {
        hdr_->tail.store(local_tail_);
        return true;
    }

    ~Ringbuf()
    {
        if (hdr_ != nullptr)
        {
            munmap(hdr_, getpagesize() + hdr_->size * 2);
        }

        if (owns_fd_)
        {
            close(fd_);
        }
    }

    Ringbuf(Ringbuf&) = delete;
    Ringbuf& operator=(Ringbuf& other) = delete;

    Ringbuf(Ringbuf&& other)
    {
        this->hdr_ = other.hdr_;
        this->data_ = other.data_;
        this->local_tail_ = other.local_tail_;
        this->local_head_ = other.local_head_;
        this->fd_ = other.fd_;
        this->owns_fd_ = other.owns_fd_;
        this->watermark_ = other.watermark_;
        this->watermark_cb_ = other.watermark_cb_;
        this->watermark_payload_ = other.watermark_payload_;

        other.hdr_ = nullptr;
        other.data_ = nullptr;
        other.fd_ = -1;
        other.owns_fd_ = false;
        other.watermark_ = 0;
        other.watermark_cb_ = nullptr;
        other.watermark_payload_ = 0;
    }

    Ringbuf& operator=(Ringbuf&& other)
    {
        this->hdr_ = other.hdr_;
        this->data_ = other.data_;
        this->local_tail_ = other.local_tail_;
        this->local_head_ = other.local_head_;
        this->fd_ = other.fd_;
        this->owns_fd_ = other.owns_fd_;
        this->watermark_ = other.watermark_;
        this->watermark_cb_ = other.watermark_cb_;
        this->watermark_payload_ = other.watermark_payload_;

        other.hdr_ = nullptr;
        other.data_ = nullptr;
        other.fd_ = -1;
        other.owns_fd_ = false;
        other.watermark_ = 0;
        other.watermark_cb_ = nullptr;
        other.watermark_payload_ = 0;
        other.owns_fd_ = false;
        return *this;
    }

private:
    /*
     * Gets the amount of data that is in the ring buffer
     */
    uint64_t get_fill()
    {
        uint64_t tail = hdr_->tail.load();
        uint64_t head = hdr_->head.load();
        if (head > tail)
        {
            return head - tail;
        }
        else
        {
            return head + hdr_->size - tail;
        }
    }

    Ringbuf() = default;

    struct ringbuf_header* hdr_ = nullptr;
    std::byte* data_ = nullptr;

    int fd_ = -1;
    bool owns_fd_ = false;

    size_t local_head_ = 0;
    size_t local_tail_ = 0;

    uint64_t watermark_ = 0;
    watermark_cb_fn watermark_cb_ = nullptr;
    void* watermark_payload_ = nullptr;
};
} // namespace twenty6
