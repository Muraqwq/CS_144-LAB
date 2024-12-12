#pragma once

#include "byte_stream.hh"
#include "tcp_receiver_message.hh"
#include "tcp_sender_message.hh"

#include <cstdint>
#include <functional>
#include <list>
#include <memory>
#include <optional>
#include <queue>

class TCPTimer
{
public:
  TCPTimer( uint64_t rto_ ) : countdown_( rto_ ) {}
  uint64_t getCountdown() { return countdown_; }
  void setCountdown( uint64_t rto_ ) { countdown_ = rto_; }
  void countDown( uint64_t time ) { countdown_ -= ( time > countdown_ ? countdown_ : time ); }
  bool alarm() { return countdown_ == 0; }

private:
  uint64_t countdown_ {};
};

class TCPSender
{
public:
  /* Construct TCP sender with given default Retransmission Timeout and possible ISN */
  TCPSender( ByteStream&& input, Wrap32 isn, uint64_t initial_RTO_ms )
    : timer_( initial_RTO_ms )
    , input_( std::move( input ) )
    , isn_( isn )
    , initial_RTO_ms_( initial_RTO_ms )
    , RTO_ms_( initial_RTO_ms )

  {}

  TCPTimer timer_;

  /* Generate an empty TCPSenderMessage */
  TCPSenderMessage make_empty_message() const;

  /* Receive and process a TCPReceiverMessage from the peer's receiver */
  void receive( const TCPReceiverMessage& msg );

  /* Type of the `transmit` function that the push and tick methods can use to send messages */
  using TransmitFunction = std::function<void( const TCPSenderMessage& )>;

  /* Push bytes from the outbound stream */
  void push( const TransmitFunction& transmit );

  /* Time has passed by the given # of milliseconds since the last time the tick() method was called */
  void tick( uint64_t ms_since_last_tick, const TransmitFunction& transmit );

  // Accessors

  uint64_t sequence_numbers_in_flight() const;  // How many sequence numbers are outstanding?
  uint64_t consecutive_retransmissions() const; // How many consecutive *re*transmissions have happened?
  Writer& writer() { return input_.writer(); }
  const Writer& writer() const { return input_.writer(); }

  // Access input stream reader, but const-only (can't read from outside)
  const Reader& reader() const { return input_.reader(); }

private:
  // Variables initialized in constructor
  ByteStream input_;
  Wrap32 isn_;
  uint64_t initial_RTO_ms_;

  uint64_t RTO_ms_;
  // uint64_t seqno_ {};
  uint64_t ackno_ {};
  uint64_t next_seqno_ { 1 };
  uint64_t windows_size_ { 1 };
  uint64_t consecutive_retransmissions_time_ {};

  bool is_syn_ = false;
  bool is_fin_ = false;
  bool is_waiting_ = false;
  // TCPSenderMessage waiting_msg_ {};
  std::deque<TCPSenderMessage> outstanding_list_ {};
};
