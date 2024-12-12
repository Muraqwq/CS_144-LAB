#include "tcp_sender.hh"
#include "tcp_config.hh"

using namespace std;

uint64_t TCPSender::sequence_numbers_in_flight() const
{
  return next_seqno_ - ackno_;
}

uint64_t TCPSender::consecutive_retransmissions() const
{
  return consecutive_retransmissions_time_;
}

void TCPSender::push( const TransmitFunction& transmit )
{

  TCPSenderMessage msg;
  if ( !is_syn_ ) {
    msg.seqno = isn_;
    msg.SYN = true;
    msg.RST = reader().has_error();
    is_syn_ = true;

    if ( reader().is_finished() ) {
      msg.FIN = true;
      next_seqno_++;
      is_fin_ = true;
    }
    transmit( msg );
    outstanding_list_.push_back( msg );
    return;
  }

  if ( !is_waiting_ and !windows_size_ ) {
    msg.RST = reader().has_error();
    if ( !reader().is_finished() ) {
      read( writer().reader(), 1, msg.payload );
    } else {
      msg.FIN = true;
    }
    msg.seqno = Wrap32::wrap( next_seqno_, isn_ );
    transmit( msg );
    outstanding_list_.push_back( msg );
    next_seqno_ += msg.sequence_length();
    is_waiting_ = true;
    return;
  }

  while ( next_seqno_ < ackno_ + windows_size_ ) {
    if ( is_fin_ ) {
      return;
    }
    msg = TCPSenderMessage();
    msg.RST = reader().has_error();
    if ( !reader().is_finished() ) {
      read(
        writer().reader(), min( TCPConfig::MAX_PAYLOAD_SIZE, ackno_ + windows_size_ - next_seqno_ ), msg.payload );
    }
    if ( reader().is_finished() ) {
      if ( next_seqno_ + msg.payload.size() < ackno_ + windows_size_ ) {
        msg.FIN = true;
        is_fin_ = true;
      }
    }

    if ( !msg.sequence_length() ) {
      break;
    }

    if ( !outstanding_list_.size() )
      timer_.setCountdown( RTO_ms_ );

    msg.seqno = Wrap32::wrap( next_seqno_, isn_ );
    transmit( msg );

    outstanding_list_.push_back( msg );
    next_seqno_ += msg.sequence_length();
  }

  return;
}

TCPSenderMessage TCPSender::make_empty_message() const
{
  // Your code here.
  TCPSenderMessage msg;
  msg.seqno = Wrap32::wrap( next_seqno_, isn_ );
  msg.RST = reader().has_error();
  return msg;
}

void TCPSender::receive( const TCPReceiverMessage& msg )
{

  is_waiting_ = false;
  if ( msg.RST ) {
    writer().set_error();
  }

  if ( msg.ackno.has_value() ) {
    uint64_t new_ackno_ = msg.ackno.value().unwrap( isn_, ackno_ );
    if ( next_seqno_ < new_ackno_ )
      return;

    if ( new_ackno_ == ackno_ ) {
      windows_size_ = msg.window_size;
      return;
    }
    if ( new_ackno_ > ackno_ ) {

      while ( outstanding_list_.size()
              and outstanding_list_.front().seqno.unwrap( isn_, ackno_ )
                      + outstanding_list_.front().sequence_length()
                    <= new_ackno_ )
        outstanding_list_.pop_front();

      ackno_ = new_ackno_;
      windows_size_ = msg.window_size;
      RTO_ms_ = initial_RTO_ms_;
      if ( outstanding_list_.size() )
        timer_.setCountdown( RTO_ms_ );
      consecutive_retransmissions_time_ = 0;
    }
  }
}

void TCPSender::tick( uint64_t ms_since_last_tick, const TransmitFunction& transmit )
{
  if ( outstanding_list_.size() ) {
    timer_.countDown( ms_since_last_tick );
  }

  if ( timer_.alarm() ) {
    transmit( outstanding_list_.front() );
    consecutive_retransmissions_time_++;
    if ( !is_waiting_ )
      RTO_ms_ = RTO_ms_ * 2;
    timer_.setCountdown( RTO_ms_ );
  }
}
