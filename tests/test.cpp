// SPDX-License-Identifier: MIT
//
// Catch2 twenty6 ringbuffer test cases
//
// Copyright (C) 2025 Technische Universität Drbden
// Christian von Elm <christian.von_elm@tu-drbden.de>

#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include <memory>
#include <sys/types.h>
#include <twenty6/ringbuf.hpp>
#include <unistd.h>

TEST_CASE("Create Ringbuffer", "[create_ringbuffer]")
{
    REQUIRE_NOTHROW(twenty6::Ringbuf::create_memfd_ringbuf(1));
};

TEST_CASE("Ringbuffer creation fails for too big ringbuffers", "[fails_rb_too_big]")
{
    // If this works for your computer, because you have more than an Exabyte of RAM, I'm sorry
    REQUIRE_THROWS_AS(twenty6::Ringbuf::create_memfd_ringbuf(1024ULL * 1024 * 1024 * 1024 * 1024),
                      std::runtime_error);
};

TEST_CASE("Can reserve memory on the buffer", "[reserve_on_rb]")
{
    std::unique_ptr<twenty6::Ringbuf> rb;

    REQUIRE_NOTHROW(
        rb = std::make_unique<twenty6::Ringbuf>(twenty6::Ringbuf::create_memfd_ringbuf(1)));

    REQUIRE(rb->reserve(4) != nullptr);
}

TEST_CASE("Can read and write  the buffer", "[rw_on_rb]")
{
    std::unique_ptr<twenty6::Ringbuf> rb;

    REQUIRE_NOTHROW(
        rb = std::make_unique<twenty6::Ringbuf>(twenty6::Ringbuf::create_memfd_ringbuf(1)));

    uint64_t* ptr = reinterpret_cast<uint64_t*>(rb->reserve(sizeof(uint64_t)));
    REQUIRE(ptr != nullptr);
    *ptr = 42;

    rb->publish();

    const uint64_t* read_ptr = reinterpret_cast<const uint64_t*>(rb->read(sizeof(uint64_t)));
    REQUIRE(read_ptr != nullptr);
    REQUIRE(*read_ptr == 42);
}

TEST_CASE("Wraparound works", "[Wraparound]")
{
    std::unique_ptr<twenty6::Ringbuf> rb;

    REQUIRE_NOTHROW(
        rb = std::make_unique<twenty6::Ringbuf>(twenty6::Ringbuf::create_memfd_ringbuf(1)));

    uint64_t size = getpagesize() * 0.8;
    REQUIRE(rb->reserve(size) != nullptr);
    rb->publish();
    REQUIRE(rb->read(size) != nullptr);
    rb->consume();

    uint64_t ev_size = getpagesize() * 0.5;
    REQUIRE(ev_size > sizeof(uint64_t));
    std::byte* data = rb->reserve(ev_size);

    REQUIRE(data != nullptr);

    uint64_t* uint = reinterpret_cast<uint64_t*>(data + ev_size - sizeof(uint64_t));

    *uint = 42;
    rb->publish();

    const std::byte* rbult = rb->read(ev_size);

    REQUIRE(rbult != nullptr);
    const uint64_t* output = reinterpret_cast<const uint64_t*>(rbult + ev_size - sizeof(uint64_t));

    REQUIRE(*output == 42);
}

TEST_CASE("Read fails on empty buffer", "[read_on_empty]")
{
    std::unique_ptr<twenty6::Ringbuf> rb;

    REQUIRE_NOTHROW(
        rb = std::make_unique<twenty6::Ringbuf>(twenty6::Ringbuf::create_memfd_ringbuf(1)));
    REQUIRE(rb->read(4) == nullptr);
}

TEST_CASE("Reserve fails if ev_size == rb_size", "[reserve_fails_ev_size_eq_rb_size]")
{
    std::unique_ptr<twenty6::Ringbuf> rb;

    REQUIRE_NOTHROW(
        rb = std::make_unique<twenty6::Ringbuf>(twenty6::Ringbuf::create_memfd_ringbuf(1)));
    REQUIRE(rb->reserve(getpagesize()) == nullptr);
}

TEST_CASE("Reserve succeeds if ev_size == rb_size -1", "[write_fails_ev_size_eq_rb_size]")
{
    std::unique_ptr<twenty6::Ringbuf> rb;

    REQUIRE_NOTHROW(
        rb = std::make_unique<twenty6::Ringbuf>(twenty6::Ringbuf::create_memfd_ringbuf(1)));
    REQUIRE(rb->reserve(getpagesize() - 1) != nullptr);
}
