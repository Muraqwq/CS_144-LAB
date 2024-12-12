#pragma once

#include <memory>
#include <optional>

#include "exception.hh"
#include "network_interface.hh"

// \brief A router that has multiple network interfaces and
// performs longest-prefix-match routing between them.
class Router
{
public:
  // Add an interface to the router
  // \param[in] interface an already-constructed network interface
  // \returns The index of the interface after it has been added to the router
  size_t add_interface( std::shared_ptr<NetworkInterface> interface )
  {
    _interfaces.push_back( notnull( "add_interface", std::move( interface ) ) );
    return _interfaces.size() - 1;
  }

  // Access an interface by index
  std::shared_ptr<NetworkInterface> interface( const size_t N ) { return _interfaces.at( N ); }

  // Add a route (a forwarding rule)
  void add_route( uint32_t route_prefix,
                  uint8_t prefix_length,
                  std::optional<Address> next_hop,
                  size_t interface_num );

  // Route packets between the interfaces
  void route();
  void route_alg( InternetDatagram& dgram_ );

private:
  struct route_input
  {
    uint32_t route_prefix {};
    uint8_t prefix_length {};

    route_input( uint32_t route_prefix_arg, uint8_t prefix_length_arg )
      : route_prefix( route_prefix_arg ), prefix_length( prefix_length_arg ) {};

    bool operator<( const route_input& other ) const
    {
      if ( this->route_prefix != other.route_prefix )
        return this->prefix_length < other.prefix_length;
      return this->route_prefix < other.route_prefix;
    }
  };

  struct route_forward
  {
    std::optional<Address> next_hop {};
    size_t interface_num {};

    route_forward() {};
    route_forward( std::optional<Address> next_hop_arg, size_t interface_num_arg )
      : next_hop( next_hop_arg ), interface_num( interface_num_arg ) {};
  };
  // The router's collection of network interfaces
  std::vector<std::shared_ptr<NetworkInterface>> _interfaces {};

  // The route_table
  std::map<route_input, route_forward> route_map {};
};
