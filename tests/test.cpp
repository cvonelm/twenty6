#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include <sys/types.h>
#include <twenty6/ringbuf.hpp>
#include <unistd.h>

TEST_CASE("Create Ringbuffer", "[create_ringbuffer]")
{
    auto res = twenty6::Ringbuf::create_memfd_ringbuf(1);
    REQUIRE(res.has_value());
};

TEST_CASE("Ringbuffer creation fails for too big ringbuffers", "[fails_rb_too_big]")
{
    // If this works for your computer, because you have more than an Exabyte of RAM, I'm sorry
    auto res = twenty6::Ringbuf::create_memfd_ringbuf(1024ULL * 1024 * 1024 * 1024 * 1024);
    REQUIRE(!res.has_value());
};

TEST_CASE("Can reserve memory on the buffer", "[reserve_on_rb]")
{
    auto res = twenty6::Ringbuf::create_memfd_ringbuf(1);
    REQUIRE(res.has_value());

    REQUIRE(res->reserve(4) != nullptr);
}

TEST_CASE("Can read and write  the buffer", "[rw_on_rb]")
{
    auto res = twenty6::Ringbuf::create_memfd_ringbuf(1);
    REQUIRE(res.has_value());

    uint64_t* ptr = reinterpret_cast<uint64_t*>(res->reserve(sizeof(uint64_t)));
    REQUIRE(ptr != nullptr);
    *ptr = 42;

    res->publish();

    uint64_t* read_ptr = reinterpret_cast<uint64_t*>(res->read(sizeof(uint64_t)));
    REQUIRE(read_ptr != nullptr);
    REQUIRE(*read_ptr == 42);
}

TEST_CASE("Wraparound works", "[Wraparound]")
{
    auto res = twenty6::Ringbuf::create_memfd_ringbuf(1);
    REQUIRE(res.has_value());

    uint64_t size = getpagesize() * 0.8;
    REQUIRE(res->reserve(size) != nullptr);
    res->publish();
    REQUIRE(res->read(size) != nullptr);
    res->consume();

    uint64_t ev_size = getpagesize() * 0.5;
    REQUIRE(ev_size > sizeof(uint64_t));
    std::byte* data = res->reserve(ev_size);

    REQUIRE(data != nullptr);

    uint64_t* uint = reinterpret_cast<uint64_t*>(data + ev_size - sizeof(uint64_t));

    *uint = 42;
    res->publish();

    std::byte* result = res->read(ev_size);

    REQUIRE(result != nullptr);
    uint = reinterpret_cast<uint64_t*>(result + ev_size - sizeof(uint64_t));

    REQUIRE(*uint == 42);
}

TEST_CASE("Read fails on empty buffer", "[read_on_empty]")
{
    auto res = twenty6::Ringbuf::create_memfd_ringbuf(1);
    REQUIRE(res.has_value());
    REQUIRE(res->read(4) == nullptr);
}

TEST_CASE("Reserve fails if ev_size == rb_size", "[reserve_fails_ev_size_eq_rb_size]")
{
    auto res = twenty6::Ringbuf::create_memfd_ringbuf(1);
    REQUIRE(res.has_value());
    REQUIRE(res->reserve(getpagesize()) == nullptr);
}

TEST_CASE("Reserve succeeds if ev_size == rb_size -1", "[write_fails_ev_size_eq_rb_size]")
{
    auto res = twenty6::Ringbuf::create_memfd_ringbuf(1);
    REQUIRE(res.has_value());
    REQUIRE(res->reserve(getpagesize() - 1) != nullptr);
}
