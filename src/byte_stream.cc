#include "byte_stream.hh"
#include "string_view"
#include <stdexcept>

using namespace std;

ByteStream::ByteStream( uint64_t capacity ) : capacity_( capacity ) {}

bool Writer::is_closed() const
{
  // Your code here.
  return is_close;
}

void Writer::push( string data ) noexcept
{
  // Your code here.

  // the condition :1.the stream has been closed 2.the data is too long
  // if ( data.empty() || available_capacity() == 0 )
  //   return;

  // auto const data_size_ = data.size();
  // auto const available_space_ = available_capacity();
  // auto const true_size_ = min( available_space_, data_size_ );

  // data = data.substr( 0, true_size_ );
  // buffer_.append( data );
  // num_pushed_ += true_size_;

  auto len = min( data.size(), available_capacity() );
  if ( len == 0 ) {
    return;
  } else if ( len < data.size() ) {
    data.resize( len );
  }

  buffer_data.push( move( data ) );
  if ( buffer_data.size() == 1 ) {
    buffer_view = buffer_data.front();
  }

  bytes_pushed_ += len;
}

void Writer::close()
{
  // Your code here.
  is_close = true;
}

uint64_t Writer::available_capacity() const noexcept
{ // Your code here.

  return capacity_ - reader().bytes_buffered();
}

uint64_t Writer::bytes_pushed() const noexcept
{
  // Your code here.
  // return num_pushed_;
  return bytes_pushed_;
}

bool Reader::is_finished() const noexcept
{
  // Your code here.
  return writer().is_closed() and ( bytes_buffered() == 0 );
}

uint64_t Reader::bytes_popped() const noexcept
{
  // Your code here.
  return bytes_popped_;
}

string_view Reader::peek() const noexcept
{
  // Your code here.

  // return buffer_;
  return buffer_view;
}

void Reader::pop( uint64_t len ) noexcept
{
  // Your code here.
  // uint64_t true_len_ = min( len, buffer_.size() );
  // num_poped_ += true_len_;
  // buffer_ = buffer_.substr( true_len_ );

  if ( len > bytes_buffered() ) {
    return;
  }

  bytes_popped_ += len;
  while ( len > 0 ) {
    if ( len >= buffer_view.size() ) {
      len -= buffer_view.size();
      buffer_data.pop();
      buffer_view = buffer_data.front();
    } else {
      buffer_view.remove_prefix( len );
      len = 0;
    }
  }
}

uint64_t Reader::bytes_buffered() const noexcept
{
  // Your code here.
  return writer().bytes_pushed() - bytes_popped();
}
