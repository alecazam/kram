/* -----------------------------------------------------------------------------

	Copyright (c) 2006 Simon Brown                          si@sjbrown.co.uk

	Permission is hereby granted, free of charge, to any person obtaining
	a copy of this software and associated documentation files (the 
	"Software"), to	deal in the Software without restriction, including
	without limitation the rights to use, copy, modify, merge, publish,
	distribute, sublicense, and/or sell copies of the Software, and to 
	permit persons to whom the Software is furnished to do so, subject to 
	the following conditions:

	The above copyright notice and this permission notice shall be included
	in all copies or substantial portions of the Software.

	THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
	OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF 
	MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
	IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY 
	CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, 
	TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE 
	SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
	
   -------------------------------------------------------------------------- */
   
#ifndef SQUISH_MATHS_H
#define SQUISH_MATHS_H

#include <cmath>
#include <algorithm>

namespace squish {

class Vec3
{
public:
	typedef Vec3 const& Arg;

	Vec3()
	{
	}

	explicit Vec3( float s )
	{
		m_x = s;
		m_y = s;
		m_z = s;
	}

	Vec3( float x, float y, float z )
	{
		m_x = x;
		m_y = y;
		m_z = z;
	}
	
	float X() const { return m_x; }
	float Y() const { return m_y; }
	float Z() const { return m_z; }
	
	Vec3 operator-() const
	{
		return Vec3( -m_x, -m_y, -m_z );
	}
	
	Vec3& operator+=( Arg v )
	{
		m_x += v.m_x;
		m_y += v.m_y;
		m_z += v.m_z;
		return *this;
	}
	
	Vec3& operator-=( Arg v )
	{
		m_x -= v.m_x;
		m_y -= v.m_y;
		m_z -= v.m_z;
		return *this;
	}
	
	Vec3& operator*=( Arg v )
	{
		m_x *= v.m_x;
		m_y *= v.m_y;
		m_z *= v.m_z;
		return *this;
	}
	
	Vec3& operator*=( float s )
	{
		m_x *= s;
		m_y *= s;
		m_z *= s;
		return *this;
	}
	
	Vec3& operator/=( Arg v )
	{
		m_x /= v.m_x;
		m_y /= v.m_y;
		m_z /= v.m_z;
		return *this;
	}
	
	Vec3& operator/=( float s )
	{
		float t = 1.0f/s;
		m_x *= t;
		m_y *= t;
		m_z *= t;
		return *this;
	}
	
	friend Vec3 operator+( Arg left, Arg right )
	{
		Vec3 copy( left );
		return copy += right;
	}
	
	friend Vec3 operator-( Arg left, Arg right )
	{
		Vec3 copy( left );
		return copy -= right;
	}
	
	friend Vec3 operator*( Arg left, Arg right )
	{
		Vec3 copy( left );
		return copy *= right;
	}
	
	friend Vec3 operator*( Arg left, float right )
	{
		Vec3 copy( left );
		return copy *= right;
	}
	
	friend Vec3 operator*( float left, Arg right )
	{
		Vec3 copy( right );
		return copy *= left;
	}
	
	friend Vec3 operator/( Arg left, Arg right )
	{
		Vec3 copy( left );
		return copy /= right;
	}
	
	friend Vec3 operator/( Arg left, float right )
	{
		Vec3 copy( left );
		return copy /= right;
	}
	
	friend float Dot( Arg left, Arg right )
	{
		return left.m_x*right.m_x + left.m_y*right.m_y + left.m_z*right.m_z;
	}
	
	friend Vec3 Min( Arg left, Arg right )
	{
		return Vec3(
			std::min( left.m_x, right.m_x ), 
			std::min( left.m_y, right.m_y ), 
			std::min( left.m_z, right.m_z )
		);
	}

	friend Vec3 Max( Arg left, Arg right )
	{
		return Vec3(
			std::max( left.m_x, right.m_x ), 
			std::max( left.m_y, right.m_y ), 
			std::max( left.m_z, right.m_z )
		);
	}

	friend Vec3 Truncate( Arg v )
	{
		return Vec3(
			v.m_x > 0.0f ? std::floor( v.m_x ) : std::ceil( v.m_x ), 
			v.m_y > 0.0f ? std::floor( v.m_y ) : std::ceil( v.m_y ), 
			v.m_z > 0.0f ? std::floor( v.m_z ) : std::ceil( v.m_z )
		);
	}

private:
	float m_x;
	float m_y;
	float m_z;
};

inline float LengthSquared( Vec3::Arg v )
{
	return Dot( v, v );
}

class Sym3x3
{
public:
	Sym3x3()
	{
	}

	Sym3x3( float s )
	{
		for( int i = 0; i < 6; ++i )
			m_x[i] = s;
	}

	float operator[]( int index ) const
	{
		return m_x[index];
	}

	float& operator[]( int index )
	{
		return m_x[index];
	}

private:
	float m_x[6];
};

Sym3x3 ComputeWeightedCovariance( int n, Vec3 const* points, float const* weights );
Vec3 ComputePrincipleComponent( Sym3x3 const& matrix );

/*
#define VEC4_CONST( X ) Vec4( X )
 
#define SQUISH_SSE_SPLAT( a )                                        \
     ( ( a ) | ( ( a ) << 2 ) | ( ( a ) << 4 ) | ( ( a ) << 6 ) )

#define SQUISH_SSE_SHUF( x, y, z, w )                                \
     ( ( x ) | ( ( y ) << 2 ) | ( ( z ) << 4 ) | ( ( w ) << 6 ) )

class Vec4
{
public:
    typedef Vec4 const& Arg;

    Vec4() {}
        
    explicit Vec4( __m128 v ) : m_v( v ) {}
    
    Vec4( Vec4 const& arg ) : m_v( arg.m_v ) {}
    
    Vec4& operator=( Vec4 const& arg )
    {
        m_v = arg.m_v;
        return *this;
    }
    
    explicit Vec4( float s ) : m_v( _mm_set1_ps( s ) ) {}
    
    Vec4( float x, float y, float z, float w ) : m_v( _mm_setr_ps( x, y, z, w ) ) {}
    
    Vec3 GetVec3() const
    {
#ifdef __GNUC__
        __attribute__ ((__aligned__ (16))) float c[4];
#else
        __declspec(align(16)) float c[4];
#endif
        _mm_store_ps( c, m_v );
        return Vec3( c[0], c[1], c[2] );
    }
    
    Vec4 SplatX() const { return Vec4( _mm_shuffle_ps( m_v, m_v, SQUISH_SSE_SPLAT( 0 ) ) ); }
    Vec4 SplatY() const { return Vec4( _mm_shuffle_ps( m_v, m_v, SQUISH_SSE_SPLAT( 1 ) ) ); }
    Vec4 SplatZ() const { return Vec4( _mm_shuffle_ps( m_v, m_v, SQUISH_SSE_SPLAT( 2 ) ) ); }
    Vec4 SplatW() const { return Vec4( _mm_shuffle_ps( m_v, m_v, SQUISH_SSE_SPLAT( 3 ) ) ); }

    Vec4& operator+=( Arg v )
    {
        m_v = _mm_add_ps( m_v, v.m_v );
        return *this;
    }
    
    Vec4& operator-=( Arg v )
    {
        m_v = _mm_sub_ps( m_v, v.m_v );
        return *this;
    }
    
    Vec4& operator*=( Arg v )
    {
        m_v = _mm_mul_ps( m_v, v.m_v );
        return *this;
    }
    
    friend Vec4 operator+( Vec4::Arg left, Vec4::Arg right  )
    {
        return Vec4( _mm_add_ps( left.m_v, right.m_v ) );
    }
    
    friend Vec4 operator-( Vec4::Arg left, Vec4::Arg right  )
    {
        return Vec4( _mm_sub_ps( left.m_v, right.m_v ) );
    }
    
    friend Vec4 operator*( Vec4::Arg left, Vec4::Arg right  )
    {
        return Vec4( _mm_mul_ps( left.m_v, right.m_v ) );
    }
    
    //! Returns a*b + c
    friend Vec4 MultiplyAdd( Vec4::Arg a, Vec4::Arg b, Vec4::Arg c )
    {
        return Vec4( _mm_add_ps( _mm_mul_ps( a.m_v, b.m_v ), c.m_v ) );
    }
    
    //! Returns -( a*b - c )
    friend Vec4 NegativeMultiplySubtract( Vec4::Arg a, Vec4::Arg b, Vec4::Arg c )
    {
        return Vec4( _mm_sub_ps( c.m_v, _mm_mul_ps( a.m_v, b.m_v ) ) );
    }
    
    friend Vec4 Reciprocal( Vec4::Arg v )
    {
        // get the reciprocal estimate
        __m128 estimate = _mm_rcp_ps( v.m_v );

        // one round of Newton-Rhaphson refinement
        __m128 diff = _mm_sub_ps( _mm_set1_ps( 1.0f ), _mm_mul_ps( estimate, v.m_v ) );
        return Vec4( _mm_add_ps( _mm_mul_ps( diff, estimate ), estimate ) );
    }
    
    friend Vec4 Min( Vec4::Arg left, Vec4::Arg right )
    {
        return Vec4( _mm_min_ps( left.m_v, right.m_v ) );
    }
    
    friend Vec4 Max( Vec4::Arg left, Vec4::Arg right )
    {
        return Vec4( _mm_max_ps( left.m_v, right.m_v ) );
    }
    
    friend Vec4 Truncate( Vec4::Arg v )
    {
        // use SSE2 instructions
        return Vec4( _mm_cvtepi32_ps( _mm_cvttps_epi32( v.m_v ) ) );
    }
    
    friend bool CompareAnyLessThan( Vec4::Arg left, Vec4::Arg right )
    {
        __m128 bits = _mm_cmplt_ps( left.m_v, right.m_v );
        int value = _mm_movemask_ps( bits );
        return value != 0;
    }
    
private:
    __m128 m_v;
};
*/


USING_SIMD;

using Vec4 = float4;
// default ctor for float4(1) sets 1,0,0,0 in simd, but impls like Vec4 expect float4(repeating: x)
#define VEC4_CONST(x) Vec4(makeVec4(x,x,x,x))
#define makeVec4(x,y,z,w) simd_make_float4(x,y,z,w)

inline bool CompareAnyLessThan(Vec4 x, Vec4 y) { return any(x < y); }
inline Vec4 Min(Vec4 x, Vec4 y) { return min(x, y); }
inline Vec4 Max(Vec4 x, Vec4 y) { return max(x, y); }
inline Vec4 Reciprocal( Vec4 x ) { return recip(x); }

inline Vec4 MultiplyAdd( Vec4 a, Vec4 b, Vec4 c ) { return a * b + c; }

//! Returns -( a*b - c )
inline Vec4 NegativeMultiplySubtract( Vec4 a, Vec4 b, Vec4 c ) { return c - a * b; }
inline Vec4 Truncate( Vec4 x ) { return select(ceil(x), floor( x ), x < VEC4_CONST(0)); }
inline Vec3 toVec3( Vec4 v ) { return Vec3(v.x, v.y, v.z); }

} // namespace squish



#endif // ndef SQUISH_MATHS_H
