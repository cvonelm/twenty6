// SPDX-License-Identifier: MIT
//
// Basic on-ringbuffer types
//
// Copyright (C) 2025 Technische Universität Dresden
// Christian von Elm <christian.von_elm@tu-dresden.de>

#pragma once

#include <atomic>
#include <cstdint>

struct ringbuf_header
{
    uint64_t version;
    uint64_t size;
    std::atomic_uint64_t head;
    std::atomic_uint64_t tail;
};
