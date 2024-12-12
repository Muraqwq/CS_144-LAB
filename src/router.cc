#include "router.hh"

#include <iostream>
#include <limits>

using namespace std;

// route_prefix: The "up-to-32-bit" IPv4 address prefix to match the datagram's destination address against
// prefix_length: For this route to be applicable, how many high-order (most-significant) bits of
//    the route_prefix will need to match the corresponding bits of the datagram's destination address?
// next_hop: The IP address of the next hop. Will be empty if the network is directly attached to the router (in
//    which case, the next hop address should be the datagram's final destination).
// interface_num: The index of the interface to send the datagram out on.
void Router::add_route( const uint32_t route_prefix,
                        const uint8_t prefix_length,
                        const optional<Address> next_hop,
                        const size_t interface_num )
{
  cerr << "DEBUG: adding route " << Address::from_ipv4_numeric( route_prefix ).ip() << "/"
       << static_cast<int>( prefix_length ) << " => " << ( next_hop.has_value() ? next_hop->ip() : "(direct)" )
       << " on interface " << interface_num << "\n";

  // Your code here.

  route_map.insert( pair<route_input, route_forward>( route_input( route_prefix, prefix_length ),
                                                      route_forward( next_hop, interface_num ) ) );
}

// Go through all the interfaces, and route every incoming datagram to its proper outgoing interface.
void Router::route()
{
  // Your code here.

  // 遍历所有inteface的datagrams_received， 弹出并转发
  if ( !_interfaces.size() )
    return;

  for ( auto interface : _interfaces ) {
    while ( !interface->datagrams_received().empty() ) {
      auto dgram = interface->datagrams_received().front();
      interface->datagrams_received().pop();
      if ( dgram.header.ttl == 0 )
        continue;
      route_alg( dgram );
    }
  }
}

void Router::route_alg( InternetDatagram& dgram_ )
{

  dgram_.header.ttl -= 1;
  if ( dgram_.header.ttl == 0 )
    return;
  // longset-prefix-match alg
  auto route_row_ = route_map.begin();

  bool is_drop_ = true;
  uint8_t max_prefix_length_ {};
  route_forward output_ {};
  while ( route_row_ != route_map.end() ) {
    route_input match_uint_ = route_row_->first;

    if ( ( dgram_.header.dst - match_uint_.route_prefix ) >> match_uint_.prefix_length == 0
         and match_uint_.prefix_length >= max_prefix_length_ ) {
      output_ = route_row_->second;
      is_drop_ = false;
    }
    route_row_++;
  }

  if ( is_drop_ )
    return;

  if ( output_.next_hop.has_value() )
    interface( output_.interface_num )->send_datagram( dgram_, output_.next_hop.value() );
  else
    interface( output_.interface_num )->send_datagram( dgram_, Address::from_ipv4_numeric( dgram_.header.dst ) );
}