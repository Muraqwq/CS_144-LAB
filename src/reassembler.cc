#include "reassembler.hh"
#include <algorithm>
#include <ranges>
using namespace std;

void Reassembler::push_to_output( std::string data )
{
  next_index_ += data.size();
  output_.writer().push( move( data ) );
}

void Reassembler::buffer_push( uint64_t first_index, uint64_t last_index, std::string data )
{
  auto l = first_index, r = last_index;
  auto beg = buffer_.begin(), end = buffer_.end();

  // find the node whose finish_index < first_index, determine the left node
  // find the node whose first_index > finish_index, determine the right node
  /**
   *   |||||||||||node_finish|   |left|  |。。。。| |right| |node_first|
   */
  auto lef = lower_bound( beg, end, l, []( auto& a, auto& b ) { return get<1>( a ) < b; } );
  auto rig = upper_bound( lef, end, r, []( auto& b, auto& a ) { return get<0>( a ) > b; } );

  // then if the input is not the last, it should determine the node's real first and finish index
  if ( lef != end )
    l = min( l, get<0>( *lef ) );
  if ( rig != beg )
    r = max( r, get<1>( *prev( rig ) ) ); // the reason why *prev is the rig is the next node really
  // 当data已在buffer_中时，直接返回
  if ( lef != end && get<0>( *lef ) == l && get<1>( *lef ) == r ) {
    return;
  }

  buffer_size_ += 1 + r - l;
  if ( data.size() == r - l + 1 && lef == rig ) { // 此时data的长度 = r - l + 1, 且在同一个迭代位置，
                                                  // 说明是当buffer_中没有data重叠的部分, 新增节点即可
    buffer_.emplace( rig, l, r, move( data ) );
    return;
  }
  string s( 1 + r - l, 0 );

  for ( auto&& it : views::iota( lef, rig ) ) {
    auto& [a, b, c] = *it;
    buffer_size_ -= c.size();
    ranges::copy( c, s.begin() + a - l ); // 根据下标放到对应位置 以first_index - left 为0
  }
  ranges::copy( data, s.begin() + first_index - l );
  buffer_.emplace( buffer_.erase( lef, rig ), l, r, move( s ) );
}

void Reassembler::buffer_pop()
{
  while ( !buffer_.empty() && get<0>( buffer_.front() ) == next_index_ ) {
    auto& [a, b, c] = buffer_.front();
    buffer_size_ -= c.size();
    push_to_output( move( c ) );

    buffer_.pop_front();
  }

  if ( had_last_ && buffer_.empty() ) {
    output_.writer().close();
  }
}

void Reassembler::insert( uint64_t first_index, string data, bool is_last_substring )
{
  if ( data.empty() ) {
    if ( is_last_substring ) {
      output_.writer().close();
    }
    return;
  }
  auto end_index = first_index + data.size();                            // data: [first_index, end_index)
  auto last_index = next_index_ + output_.writer().available_capacity(); // 可用范围: [next_index_, last_index)
  if ( end_index < next_index_ || first_index >= last_index ) {
    return; // 不在可用范围内, 直接返回
  }

  // 调整data的范围
  if ( last_index < end_index ) {
    end_index = last_index;
    data.resize( end_index - first_index );
    is_last_substring = false;
  }
  if ( first_index < next_index_ ) {
    data = data.substr( next_index_ - first_index );
    first_index = next_index_;
  }

  // 若data可以直接写入output_, 则直接写入
  if ( first_index == next_index_ && ( buffer_.empty() || end_index < get<1>( buffer_.front() ) + 2 ) ) {
    if ( buffer_.size() ) { // 若重叠, 则调整data的范围
      data.resize( min( end_index, get<0>( buffer_.front() ) ) - first_index );
    }
    push_to_output( move( data ) );
  } else { // 否则, 将data插入buffer_
    buffer_push( first_index, end_index - 1, data );
  }
  had_last_ |= is_last_substring;

  // 尝试将buffer_中的数据写入output_
  buffer_pop();
}

uint64_t Reassembler::bytes_pending() const
{
  return buffer_size_;
}

// void Reassembler::buffer_pop( Writer& output ) {}

// void Reassembler::insert( uint64_t first_index, string data, bool is_last_substring )
// {
//   uint64_t capaticy_ = writer().available_capacity();
//   // Your code here.

//   if ( !data.size() ) {
//     if ( is_last_substring )
//       output_.writer().close();
//     return;
//   }
//   if ( !capaticy_ )
//     return;
//   uint64_t next_index_ = send_data_index_;
//   uint64_t finish_index_ = first_index + data.size() - 1;

//   if ( finish_index_ < next_index_ ) {
//     if ( buffer_.size() ) {
//       auto item = buffer_.front();
//       buffer_.pop_front();
//       insert( item.first_index, item.data, item.is_last_substring );
//     }
//     return;
//   }
//   uint64_t max_index_ = next_index_ + capaticy_ - 1;
//   uint64_t max_length_ = min( max_index_ - first_index + 1, data.size() );
//   if ( max_length_ != data.size() )
//     is_last_substring = false;
//   if ( first_index <= next_index_ ) {

//     data = data.substr( next_index_ - first_index, max_length_ );
//     send_data_index_ += data.size();
//     output_.writer().push( move( data ) );
//     if ( is_last_substring ) {
//       output_.writer().close();
//     }

//     if ( buffer_.size() ) {
//       auto item = buffer_.front();
//       buffer_.pop_front();
//       insert( item.first_index, item.data, item.is_last_substring );
//     }

//   } else {
//     if ( first_index > max_index_ )
//       return;
//     data = data.substr( 0, max_length_ );
//     if ( !buffer_.size() ) {
//       buffer_.push_back( { first_index, data, is_last_substring } );
//       return;
//     }

//     auto item = buffer_.begin();

//     while ( true ) {
//       next_index_ = item->first_index;
//       finish_index_ = item->first_index + item->data.size() - 1;

//       if ( first_index + data.size() < next_index_ ) {
//         buffer_.insert( item, { first_index, data, is_last_substring } );
//         break;
//       } else {
//         if ( finish_index_ >= first_index - 1 ) {
//           std::string substr1;
//           std::string substr2;
//           uint64_t finish_index = first_index + data.size() - 1;
//           uint64_t new_index_;
//           if ( first_index <= next_index_ ) {
//             new_index_ = first_index;
//             if ( finish_index >= finish_index_ ) {
//               substr1 = data;
//               substr2 = "";
//             } else {
//               substr1 = data.substr( 0, next_index_ - first_index );
//               substr2 = item->data;
//             }
//           } else {
//             new_index_ = next_index_;
//             if ( finish_index_ >= finish_index ) {
//               substr1 = item->data;
//               substr2 = "";
//             } else {
//               substr1 = item->data.substr( 0, first_index - next_index_ );
//               substr2 = data;
//             }
//           }
//           std::string new_str_ = substr1 + substr2;
//           first_index = new_index_;
//           data = new_str_;
//           is_last_substring = item->is_last_substring ? true : is_last_substring;
//           if ( item + 1 == buffer_.end() ) {
//             buffer_.push_back( { first_index, data, is_last_substring } );
//             buffer_.erase( item );
//             break;
//           }
//           item = buffer_.erase( item );

//         } else {
//           if ( item + 1 == buffer_.end() ) {
//             buffer_.push_back( { first_index, data, is_last_substring } );
//             break;
//           }
//           item++;
//           continue;
//         }
//       }
//     }
//   }
// }

// uint64_t Reassembler::bytes_pending() const
// {
//   // Your code here.
//   uint64_t sum = { 0 };
//   if ( buffer_.size() ) {
//     for ( const auto& item : buffer_ ) {
//       sum += item.data.size();
//     }
//   }
//   return sum;
// }
