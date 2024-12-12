#pragma once

#include "reassembler.hh"
#include "tcp_receiver_message.hh"
#include "tcp_sender_message.hh"

class TCPReceiver
{
public:
  // Construct with given Reassembler
  explicit TCPReceiver( Reassembler&& reassembler ) : reassembler_( std::move( reassembler ) ) {}

  /*
   * The TCPReceiver receives TCPSenderMessages, inserting their payload into the Reassembler
   * at the correct stream index.
   */
  void receive( TCPSenderMessage message );

  // The TCPReceiver sends TCPReceiverMessages to the peer's TCPSender.
  TCPReceiverMessage send() const;

  // Access the output (only Reader is accessible non-const)
  const Reassembler& reassembler() const { return reassembler_; }
  Reader& reader() { return reassembler_.reader(); }
  const Reader& reader() const { return reassembler_.reader(); }
  const Writer& writer() const { return reassembler_.writer(); }

private:
  Reassembler reassembler_;
  void update_msg() const
  {
    msg.window_size = reassembler_.writer().available_capacity() >= UINT16_MAX
                        ? UINT16_MAX
                        : (uint16_t)reassembler_.writer().available_capacity();
    msg.RST |= writer().has_error();
  }
  void set_error() { reader().set_error(); }
  bool IS_SYN_ = false;
  uint64_t ackno_ {};
  Wrap32 zero = Wrap32 { 0 };
  mutable TCPReceiverMessage msg = { std::nullopt, 0, false };
};
