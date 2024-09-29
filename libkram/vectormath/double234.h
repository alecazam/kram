// kram - Copyright 2020-2024 by Alec Miller. - MIT License
// The license and copyright notice shall be included
// in all copies or substantial portions of the Software.

#pragma once

// This is not yet standalone.  vectormath++.h includes it.
#if USE_SIMDLIB && SIMD_DOUBLE

#ifdef __cplusplus
extern "C" {
#endif

// define c vector/matrix types
macroVector8TypesStorage(double, double)
macroVector8TypesPacked(double, double)

// storage type for matrix
typedef struct { double2s columns[2]; } double2x2s;
typedef struct { double3s columns[3]; } double3x3s;
typedef struct { double4s columns[3]; } double3x4s;
typedef struct { double4s columns[4]; } double4x4s;

// glue to Accelerate
#if SIMD_RENAME_TO_SIMD_NAMESPACE
macroVector8TypesStorageRenames(double, simd_double)
#endif

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus
namespace SIMD_NAMESPACE {

macroVector8TypesStorageRenames(double, double)

// zeroext - internal helper
SIMD_CALL double4 zeroext(double2 x) {
    return (double4){x.x,x.y,0,0};
}
SIMD_CALL double4 zeroext(double3 x) {
    return (double4){x.x,x.y,x.z,0};
}

SIMD_CALL double2 double2m(double x) {
    return x;
}
SIMD_CALL double2 double2m(double x, double y) {
    return {x,y};
}

SIMD_CALL double3 double3m(double x) {
    return x;
}
SIMD_CALL double3 double3m(double x, double y, double z) {
    return {x,y,z};
}
SIMD_CALL double3 double3m(double2 v, float z) {
    double3 r; r.xy = v; r.z = z; return r;
}

SIMD_CALL double4 double4m(double x) {
    return x;
}
SIMD_CALL double4 double4m(double2 xy, double2 zw) {
    double4 r; r.xy = xy; r.zw = zw; return r;
}
SIMD_CALL double4 double4m(double x, double y, double z, double w = 1.0) {
    return {x,y,z,w};
}
SIMD_CALL double4 double4m(double3 v, double w = 1.0) {
    double4 r; r.xyz = v; r.w = w; return r;
}

// power series
macroVectorRepeatFnDecl(double, log)
macroVectorRepeatFnDecl(double, exp)
//macroVectorRepeatFnDecl(double, pow)

// trig
macroVectorRepeatFnDecl(double, cos)
macroVectorRepeatFnDecl(double, sin)
macroVectorRepeatFnDecl(double, tan)

SIMD_CALL double2 pow(double2 x, double2 y) {
    return exp(log(x) * y);
}
SIMD_CALL double3 pow(double3 x, double3 y) {
    return exp(log(x) * y);
}
SIMD_CALL double4 pow(double4 x, double4 y) {
    return exp(log(x) * y);
}

// Need to split out float234.cpp, then can alter
// that for double calls.


// TODO: would need matrix class derivations
// and all of the matrix ops, which then need vector ops, and need double
// constants.  So this starts to really add to codegen.  But double
// is one of the last bastions of cpu, since many gpu don't support it.


struct double2x2 : double2x2s
{
    // can be split out to traits
    static constexpr int col = 2;
    static constexpr int row = 2;
    using column_t = double2;
    using scalar_t = double;
    
    static const double2x2& zero();
    static const double2x2& identity();
    
    double2x2() { }  // no default init
    explicit double2x2(double2 diag);
    double2x2(double2 c0, double2 c1)
    : double2x2s((double2x2s){c0, c1}) { }
    double2x2(const double2x2s& m)
    : double2x2s(m) { }
    
    // simd lacks these ops
    double2& operator[](int idx) { return columns[idx]; }
    const double2& operator[](int idx) const { return columns[idx]; }
};

struct double3x3 : double3x3s
{
    static constexpr int col = 3;
    static constexpr int row = 3;
    using column_t = double3;
    using scalar_t = double;
    
    // Done as wordy c funcs otherwize.  Funcs allow statics to init.
    static const double3x3& zero();
    static const double3x3& identity();
    
    double3x3() { }  // no default init
    explicit double3x3(double3 diag);
    double3x3(double3 c0, double3 c1, double3 c2)
    : double3x3s((double3x3s){c0, c1, c2}) { }
    double3x3(const double3x3s& m)
    : double3x3s(m) { }
    
    double3& operator[](int idx) { return columns[idx]; }
    const double3& operator[](int idx) const { return columns[idx]; }
};

// This is mostly a transposed holder for a 4x4, so very few ops defined
// Can also serve as a SOA for some types of cpu math.
struct double3x4 : double3x4s
{
    static constexpr int col = 3;
    static constexpr int row = 4;
    using column_t = double4;
    using scalar_t = double;
    
    static const double3x4& zero();
    static const double3x4& identity();
    
    double3x4() { } // no default init
    explicit double3x4(double3 diag);
    double3x4(double4 c0, double4 c1, double4 c2)
    : double3x4s((double3x4s){c0, c1, c2}) { }
    double3x4(const double3x4s& m)
    : double3x4s(m) { }
    
    double4& operator[](int idx) { return columns[idx]; }
    const double4& operator[](int idx) const { return columns[idx]; }
};

struct double4x4 : double4x4s
{
    static constexpr int col = 4;
    static constexpr int row = 4;
    using column_t = double4;
    using scalar_t = double;
    
    static const double4x4& zero();
    static const double4x4& identity();
    
    double4x4() { } // no default init
    explicit double4x4(double4 diag);
    double4x4(double4 c0, double4 c1, double4 c2, double4 c3)
    : double4x4s((double4x4s){c0, c1, c2, c3}) { }
    double4x4(const double4x4s& m)
    : double4x4s(m) { }
    
    double4& operator[](int idx) { return columns[idx]; }
    const double4& operator[](int idx) const { return columns[idx]; }
};

double2x2 diagonal_matrix(double2 x);
double3x3 diagonal_matrix(double3 x);
double3x4 diagonal_matrix3x4(double3 x);
double4x4 diagonal_matrix(double4 x);

// TODO: port these over from float versions
// ops need to call these
#if 0

// using refs here, 3x3 and 4x4 are large to pass by value (3 simd regs)
double2x2 transpose(const double2x2& x);
double3x3 transpose(const double3x3& x);
double4x4 transpose(const double4x4& x);

double2x2 inverse(const double2x2& x);
double3x3 inverse(const double3x3& x);
double4x4 inverse(const double4x4& x);

double determinant(const double2x2& x);
double determinant(const double3x3& x);
double determinant(const double4x4& x);

double trace(const double2x2& x);
double trace(const double3x3& x);
double trace(const double4x4& x);

// premul = dot + premul
double2 mul(double2 y, const double2x2& x);
double3 mul(double3 y, const double3x3& x);
double4 mul(double4 y, const double4x4& x);

// posmul = mul + mad
double2x2 mul(const double2x2& x, const double2x2& y);
double3x3 mul(const double3x3& x, const double3x3& y);
double4x4 mul(const double4x4& x, const double4x4& y);

double2 mul(const double2x2& x, double2 y);
double3 mul(const double3x3& x, double3 y);
double4 mul(const double4x4& x, double4 y);

double2x2 sub(const double2x2& x, const double2x2& y);
double3x3 sub(const double3x3& x, const double3x3& y);
double4x4 sub(const double4x4& x, const double4x4& y);

double2x2 add(const double2x2& x, const double2x2& y);
double3x3 add(const double3x3& x, const double3x3& y);
double4x4 add(const double4x4& x, const double4x4& y);

bool equal(const double2x2& x, const double2x2& y);
bool equal(const double3x3& x, const double3x3& y);
bool equal(const double4x4& x, const double4x4& y);

// TODO: equal_abs, equal_rel

// operators for C++
macroMatrixOps(double2x2);
macroMatrixOps(double3x3);
// TODO: no mat ops yet on storage type double3x4
// macroMatrixOps(double3x4);
macroMatrixOps(double4x4);

// fast conversions where possible
SIMD_CALL const double3x3& as_double3x3(const double4x4& m) {
    return reinterpret_cast<const double3x3&>(m);
}

#endif // 0

} // SIMD_NAMESPACE

#endif

#endif // USE_SIMDLIB && SIMD_DOUBLE
