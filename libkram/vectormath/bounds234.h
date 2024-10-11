// kram - Copyright 2020-2024 by Alec Miller. - MIT License
// The license and copyright notice shall be included
// in all copies or substantial portions of the Software.

#pragma once

#if USE_SIMDLIB && SIMD_FLOAT

namespace SIMD_NAMESPACE {

// TODO: may want a rect, circle, capsule as well.

struct bbox {
    bbox() {} // nothing
    bbox(float3 minv, float3 maxv) : min(minv), max(maxv) {}
    
    // TODO: add a unit radius and unit diameter box
    
    // can use this to accumulate points into a box
    void setInvalid() { min = (float3)FLT_MAX; max = -(float3)FLT_MAX; }
    bool isInvalid() const { return any(min > max); }
    
    void unionWith(float3 v) {
        min = SIMD_NAMESPACE::min(min, v);
        max = SIMD_NAMESPACE::max(max, v);
    }
    void unionWith(bbox b) {
        min = SIMD_NAMESPACE::min(min, b.min);
        max = SIMD_NAMESPACE::max(max, b.max);
    }
    
    // TODO: call to intersect or combine bbox
    
    float3 center() const { return 0.5f * (min + max); }
    float3 dimensions() const { return max - min; }
    
    float width() const { return dimensions().x; }
    float height() const { return dimensions().y; }
    float depth() const { return dimensions().z; }
    
    float diameter() const { return length(dimensions()); }
    float radius() const { return 0.5f * diameter(); }
    float radiusSquared() const { return length_squared(dimensions() * 0.5f); }
    
    void scale(float3 s) { min *= s; max *= s; }
    void offset(float3 o) { min += o; max += o; }
    
    // after transforms (f.e. rotate, min/max can swap)
    void fix() {
        // don't call this on invalid box, or it will be huge
        float3 tmp = SIMD_NAMESPACE::max(min, max);
        min = SIMD_NAMESPACE::min(min, max);
        max = tmp;
    }
    
public:
    float3 min;
    float3 max;
};

// center + radius
struct bsphere {
    bsphere() {} // nothing
    bsphere(float3 center, float radius) : centerRadius(float4m(center, radius)) {}
    
    // TODO: add a unit radius and unit diameter
    
    float3 center() const { return centerRadius.xyz; }
    float radius() const { return centerRadius.w; }
    float radiusSquared() const { return centerRadius.w * centerRadius.w; }
    
public:
    float4 centerRadius;
};

// Fast cpu culler per frustum.  Easy port to gpu which can do occlusion.
// This only tests 5 or 6 planes.
struct culler {
    culler();
    
    void update(const float4x4& projView);
    
    // TODO: should pass bitmask instead of uint8_t array
    // caller must zero the results array, and visible state sets only low bit
    
    void cullBoxes(const float3* boxes, int count, uint8_t* results) const;
    void cullSpheres(const float4* sphere, int count, uint8_t* results) const;
    
    bool cullSphere(float4 sphere) const;
    bool cullBox(float3 min, float3 max) const;
    
    // can use the helper types instead
    void cullBoxes(const bbox* boxes, int count, uint8_t* results) const {
        cullBoxes((const float3*)boxes, count, results);
    }
    bool cullBoxes(const bbox& box) const {
        return cullBox(box.min, box.max);
    }
    void cullSpheres(const bsphere* spheres, int count, uint8_t* results) const {
        cullSpheres((const float4*)spheres, count, results);
    }
    bool cullSphere(const bsphere& sphere) const {
        return cullSphere(sphere.centerRadius);
    }
    
    // should probably move these
    static bsphere transformSphereTRS(bsphere sphere, const float4x4& modelTfm);
    static bbox transformBoxTRS(bbox box, const float4x4& modelTfm);
    
    void boxCorners(bbox box, float3 pt[8]) const;
    void boxCorners(bbox box, float4 pt[8]) const;
   
    bool isFrustumInBox(bbox box) const;
    bool isFrustumOutsideBox(bbox box) const;

    // Camera corners in world space
    const float3* cameraCorners() const {
        return as_float3(_corners);
    }
    int cameraCornersCount() const { return 8; }
    
    // Camera clip planes in world space
    const float4* cameraPlanes() const { return _planes; }
    int cameraPlanesCount() const { return _planesCount; }
    
private:
    float4 _planes[6];
    // This won't work if SIMD_INT is not defined.
#if SIMD_INT
    int4 _selectionMasks[6];
#endif
    uint32_t _planesCount;
    
    // 8 corners of frustum
    float4 _corners[8];
};

} // namespace SIMD_NAMESPACE

#endif //  USE_SIMDLIB && SIMD_FLOAT

