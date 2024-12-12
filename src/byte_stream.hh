#pragma once

#include "string_view"
#include <cstdint>
#include <queue>
#include <stdexcept>
#include <string>
class Reader;
class Writer;

class ByteStream
{
public:
  explicit ByteStream( uint64_t capacity );

  // Helper functions (provided) to access the ByteStream's Reader and Writer interfaces
  Reader& reader();
  const Reader& reader() const;
  Writer& writer();
  const Writer& writer() const;

  void set_error() { error_ = true; };       // Signal that the stream suffered an error.
  bool has_error() const { return error_; }; // Has the stream had an error?

protected:
  // Please add any additional state to the ByteStream here, and not to the Writer and Reader interfaces.
  uint64_t capacity_;
  bool error_ {};
  bool is_close = false;

  uint64_t bytes_pushed_ {};
  uint64_t bytes_popped_ {};
  std::queue<std::string> buffer_data {};
  std::string_view buffer_view {};
};

class Writer : public ByteStream
{
public:
  void push( std::string data ) noexcept; // Push data to stream, but only as much as available capacity allows.
  void close(); // Signal that the stream has reached its ending. Nothing more will be written.

  bool is_closed() const;                       // Has the stream been closed?
  uint64_t available_capacity() const noexcept; // How many bytes can be pushed to the stream right now?
  uint64_t bytes_pushed() const noexcept;       // Total number of bytes cumulatively pushed to the stream
};

class Reader : public ByteStream
{
public:
  std::string_view peek() const noexcept; // Peek at the next bytes in the buffer
  void pop( uint64_t len ) noexcept;      // Remove `len` bytes from the buffer

  bool is_finished() const noexcept;        // Is the stream finished (closed and fully popped)?
  uint64_t bytes_buffered() const noexcept; // Number of bytes currently buffered (pushed and not popped)
  uint64_t bytes_popped() const noexcept;   // Total number of bytes cumulatively popped from stream
};

/*
 * read: A (provided) helper function thats peeks and pops up to `len` bytes
 * from a ByteStream Reader into a string;
 */
void read( Reader& reader, uint64_t len, std::string& out );
