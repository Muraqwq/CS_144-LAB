#include <iostream>

#include "arp_message.hh"
#include "exception.hh"
#include "network_interface.hh"
#include "parser.hh"
using namespace std;

#define MAX_Query_Intervial 5000
#define MAX_Save_Time 30000

//! \param[in] ethernet_address Ethernet (what ARP calls "hardware") address of the interface
//! \param[in] ip_address IP (what ARP calls "protocol") address of the interface
NetworkInterface::NetworkInterface( string_view name,
                                    shared_ptr<OutputPort> port,
                                    const EthernetAddress& ethernet_address,
                                    const Address& ip_address )
  : name_( name )
  , port_( notnull( "OutputPort", move( port ) ) )
  , ethernet_address_( ethernet_address )
  , ip_address_( ip_address )
{
  cerr << "DEBUG: Network interface has Ethernet address " << to_string( ethernet_address ) << " and IP address "
       << ip_address.ip() << "\n";
}

//! \param[in] dgram the IPv4 datagram to be sent
//! \param[in] next_hop the IP address of the interface to send it to (typically a router or default gateway, but
//! may also be another host if directly connected to the same network as the destination) Note: the Address type
//! can be converted to a uint32_t (raw 32-bit IP address) by using the Address::ipv4_numeric() method.
void NetworkInterface::send_datagram( const InternetDatagram& dgram, const Address& next_hop )
{
  // Your code here.
  auto tuple_target_ethernet_addr_ = ( arp_map_.find( next_hop.ipv4_numeric() ) );
  Serializer serializer = Serializer();
  if ( tuple_target_ethernet_addr_ == arp_map_.end() ) {
    auto query_item_ = arp_requery_map_.find( next_hop.ipv4_numeric() );
    if ( query_item_ != arp_requery_map_.end() ) {
      return;
    }
    // 构造ARPMessage,并且序列化作为payload
    ARPMessage arp_msg_;
    arp_msg_.opcode = ARPMessage::OPCODE_REQUEST;
    arp_msg_.sender_ethernet_address = ethernet_address_;
    arp_msg_.sender_ip_address = ip_address_.ipv4_numeric();
    arp_msg_.target_ip_address = next_hop.ipv4_numeric();
    arp_msg_.serialize( serializer );

    // 构造ARP以太帧，和帧头（我认为是给transmit的时候parse的，parse header中的targetEthernetaddr）

    EthernetHeader arp_frame_header_;
    arp_frame_header_.type = EthernetHeader::TYPE_ARP;
    arp_frame_header_.dst = ETHERNET_BROADCAST;
    arp_frame_header_.src = ethernet_address_;

    EthernetFrame arp_frame_;
    arp_frame_.header = arp_frame_header_;
    arp_frame_.payload = serializer.output();
    transmit( arp_frame_ );

    // 放进queue中等待push
    // datagrams_queue.push_back( pair<InternetDatagram, const Address&>( dgram, next_hop ) );
    datagrams_queue.push_back( pair<InternetDatagram, uint32_t>( dgram, next_hop.ipv4_numeric() ) );
    arp_requery_map_.insert( pair<uint32_t, uint32_t>( next_hop.ipv4_numeric(), MAX_Query_Intervial ) );
    return;
  }

  // 将数据包序列化，作为payload
  dgram.serialize( serializer );

  // 同上，构造以太头，只不过type和dst需要变化
  EthernetHeader data_frame_header_;
  data_frame_header_.type = EthernetHeader::TYPE_IPv4;
  data_frame_header_.dst = tuple_target_ethernet_addr_->second.first;
  data_frame_header_.src = ethernet_address_;

  EthernetFrame data_frame_;

  data_frame_.payload = move( serializer.output() );
  data_frame_.header = data_frame_header_;
  transmit( data_frame_ );
}

//! \param[in] frame the incoming Ethernet frame
void NetworkInterface::recv_frame( const EthernetFrame& frame )
{
  // Your code here.
  Parser parser_ = Parser( move( frame.payload ) );
  if ( frame.header.dst != ETHERNET_BROADCAST and frame.header.dst != ethernet_address_ )
    return;
  // 如果是ARP请求，可能是发送出去的请求和发送回来的回复
  switch ( frame.header.type ) {
    case EthernetHeader::TYPE_ARP: {
      ARPMessage arp_msg_;
      arp_msg_.parse( parser_ );

      // 请求，要处理请求，给出回复或者不处理
      switch ( arp_msg_.opcode ) {
          // CASE Req

        case ARPMessage::OPCODE_REQUEST: {
          if ( arp_msg_.target_ip_address != ip_address_.ipv4_numeric() ) {
            return;
          }

          Serializer serializer = Serializer();

          // 构造回复帧
          ARPMessage resp_arp_msg_;
          resp_arp_msg_.opcode = ARPMessage::OPCODE_REPLY;
          resp_arp_msg_.sender_ethernet_address = ethernet_address_;
          resp_arp_msg_.sender_ip_address = ip_address_.ipv4_numeric();
          resp_arp_msg_.target_ethernet_address = arp_msg_.sender_ethernet_address;
          resp_arp_msg_.target_ip_address = arp_msg_.sender_ip_address;
          resp_arp_msg_.serialize( serializer );

          EthernetHeader arp_resp_header_;
          arp_resp_header_.type = EthernetHeader::TYPE_ARP;
          arp_resp_header_.dst = arp_msg_.sender_ethernet_address;
          arp_resp_header_.src = ethernet_address_;

          EthernetFrame arp_resp_frame_;
          arp_resp_frame_.header = arp_resp_header_;
          arp_resp_frame_.payload = move( serializer.output() );

          // 回复arp查询结果
          transmit( arp_resp_frame_ );
          arp_map_.insert( std::pair<uint32_t, std::pair<EthernetAddress, uint32_t>>(
            arp_msg_.sender_ip_address,
            std::pair<EthernetAddress, uint32_t>( arp_msg_.sender_ethernet_address, MAX_Save_Time ) ) );
          break;
        }

        // CASE: Resp
        // 收到回复，需要处理回复
        case ARPMessage::OPCODE_REPLY: {
          auto res_item_ = arp_requery_map_.find( arp_msg_.sender_ip_address );
          if ( res_item_ != arp_requery_map_.end() ) {
            arp_requery_map_.erase( res_item_ );
          }
          arp_map_.insert( std::pair<uint32_t, std::pair<EthernetAddress, uint32_t>>(
            arp_msg_.sender_ip_address,
            std::pair<EthernetAddress, uint32_t>( arp_msg_.sender_ethernet_address, MAX_Save_Time ) ) );

          if ( datagrams_queue.size() ) {
            for ( auto datagram = datagrams_queue.begin(); datagram != datagrams_queue.end(); ) {
              if ( datagram->second == arp_msg_.sender_ip_address ) {
                send_datagram( datagram->first, Address::from_ipv4_numeric( datagram->second ) );
                datagram = datagrams_queue.erase( datagram );
              } else {
                datagram++;
              }
            }
          }

          break;
        }
        default:
          break;
      }
      break;
    }

    case EthernetHeader::TYPE_IPv4: {
      InternetDatagram datagram_;
      datagram_.parse( parser_ );
      if ( parser_.has_error() ) {
        return;
      }

      datagrams_received_.push( datagram_ );
      break;
    }
    default:
      break;
  }
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void NetworkInterface::tick( const size_t ms_since_last_tick )
{
  // Your code here.
  if ( arp_map_.size() ) {
    for ( auto record = arp_map_.begin(); record != arp_map_.end(); ) {
      auto temp = record;
      record++;
      auto new_record = pair<uint32_t, pair<EthernetAddress, uint32_t>>(
        temp->first, pair<EthernetAddress, uint32_t>( temp->second.first, temp->second.second ) );
      arp_map_.erase( temp );

      if ( new_record.second.second > ms_since_last_tick ) {
        new_record.second.second -= ms_since_last_tick;
        arp_map_.insert( new_record );
      }
    }
  }

  if ( arp_requery_map_.size() ) {
    for ( auto record = arp_requery_map_.begin(); record != arp_requery_map_.end(); ) {
      auto temp = record;
      record++;
      auto new_record = pair<uint32_t, uint32_t>( temp->first, temp->second );
      arp_requery_map_.erase( temp );

      if ( new_record.second > ms_since_last_tick ) {
        new_record.second -= ms_since_last_tick;
        arp_requery_map_.insert( new_record );
      }
    }
  }
}
