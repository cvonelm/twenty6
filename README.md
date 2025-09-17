# Twenty6: A RingBuffer Library

Twenty6 is a fuzzed and tested C++17 ring buffer library based on memfd's for
efficiently sharing information between threads.

## Interface

To create a new ring buffer:

```cpp
twenty6::Ringbuf rb = twenty6::Ringbuf::create_memfd_ringbuf(1);
```

`create_memfd_ringbuf` creates a ring buffer with the given amount of pages of memory.
If an error occured, this throws std::runtime_error.

To attach to an existing ring buffer:


```cpp
twenty6::Ringbuf res = twenty6::Ringbuf::attach_ringbuf(other_rb.fd());
```
If an error occured, this throws std::runtime_error.


### Ring Buffer Operations
The ring buffers support the following operations:


```cpp
// Reserves "amount" amount of bytes from the current ring buffer 
// position and returns the allocated space on the ring buffer.
// Returns nullptr if there is no free space
uint8_t* msg = ringbuffer.reserve(amount);

// Makes the messages written by previous calls to "reserve()" 
// available to the reader.
ringbuffer.publish();

// Reads "amount" of bytes from the current ring buffer read position and 
// advances it.
// Returns a pointer to the memory region on the ring buffer, or nullptr,
// if there is not that much data on the ring buffer.
const uint8_t* msg = ringbuffer.read(amount);

// Does the same thing as "read", but without advancing the ring buffer
// read position. Useful for "peeking" at headers.
const uint8_t* msg = ringbuffer.peek(amount);

// Frees the memory read in previous calls of "read", and makes it available for
// the write side to read.
ringbuffer.consume();
```

## Trivia

twenty6 is named after the ["26er Ring"](https://de.wikipedia.org/wiki/26er_Ring), which is a street ring around downtown Dresden named after a former tram line.
