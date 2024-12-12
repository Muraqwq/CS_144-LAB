#include "wrapping_integers.hh"
#include "cmath"
using namespace std;

Wrap32 Wrap32::wrap( uint64_t n, Wrap32 zero_point )
{
  return Wrap32 { (uint32_t)( ( n + zero_point.raw_value_ ) % ( 1LL << 32 ) ) };
}

uint64_t Wrap32::unwrap( Wrap32 zero_point, uint64_t checkpoint ) const
{
  uint64_t max_ = 1LL << 32;
  uint32_t checkpoint_wrap_ = wrap( checkpoint, zero_point ).raw_value_;
  uint64_t diff = max( raw_value_, checkpoint_wrap_ ) - min( raw_value_, checkpoint_wrap_ );
  uint64_t res
    = checkpoint
      + ( ( checkpoint < diff )
            ? ( raw_value_ > checkpoint_wrap_ ? diff : max_ - diff )
            : ( diff <= ( max_ >> 1 ) ? ( raw_value_ > checkpoint_wrap_ ? diff : -diff )
                                      : ( raw_value_ > checkpoint_wrap_ ? diff - max_ : max_ - diff ) ) );

  return res;
}
