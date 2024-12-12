#include "tcp_receiver.hh"
#include "cmath"
#include "cstdint"
using namespace std;

void TCPReceiver::receive( TCPSenderMessage message )
{
  if ( message.RST ) {
    set_error();
  } else {
    if ( message.SYN ) {
      IS_SYN_ = true;
      zero = move( message.seqno );
      ackno_ = max( (uint64_t)1, ackno_ );
    }

    if ( IS_SYN_ ) {
      uint64_t seqno = message.seqno.unwrap( zero, ackno_ ) + message.SYN;
      reassembler_.insert( seqno - 1, message.payload, message.FIN );
      ackno_ = reassembler_.next_index() + 1 + reassembler_.writer().is_closed();
      msg.ackno = Wrap32::wrap( ackno_, zero );
    }
  }
  send();
}

TCPReceiverMessage TCPReceiver::send() const
{
  update_msg();
  return msg;
}
