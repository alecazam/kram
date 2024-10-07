#include "vectormath234.h"

#if SIMD_FLOAT && SIMD_INT

namespace SIMD_NAMESPACE {

culler::culler(const float4x4& projView) {
    // build a worldspace frustum
    // https://fgiesen.wordpress.com/2010/10/17/view-frustum-culling/
    // but don't test farZ plane if infFarZ

    float4x4 m = transpose(projView);
    const float4& x = m[0];
    const float4& y = m[1];
    const float4& z = m[2];
    const float4& w = m[3];
    
    // x < w     0 < w - x
    // x > -w    0 < w + x
    
    _planes[0] = normalize(w + x);
    _planes[1] = normalize(w - x);
    _planes[2] = normalize(w + y);
    _planes[3] = normalize(w - y);
    
    // This uses 0 to 1
    
    // revZ
    _planes[4] = normalize(w - z);
    
    bool isInfFarPlane = projView[2][2] == 0;
    if (isInfFarPlane)
        _planes[5] = 0;
    else
        _planes[5] = normalize(z);
    
    // anyway to always use 6 for unrolling?
    // f.e. above use 0,0,-1,FLT_MAX, instead of 0
    _planeCount = isInfFarPlane ? 5 : 6;
    
    // select min or max based on normal direction
    for (int i = 0; i < _planeCount; ++i) {
        _selectionMasks[i] = _planes[i] < 0;
    }
}

bool culler::cullBox(float3 min, float3 max) const {
    int count = 0;
    
    // Note: make sure box min <= max, or this call will fail
    
    // TODO: convert this from dot to a mul of 4, then finish plane 5,6
    // could precompute/store the selection masks.
    // Also may need to do 4 boxes at a time.
    
    // TODO: also if frustum is finite farZ, then may need to test for
    // frustum in box.  This is a rather expensive test though
    // of the 8 frustum corners.
    
    float4 min1 = float4m(min,1);
    float4 max1 = float4m(max,1);

    // test the min/max against the x planes
    for (int i = 0; i < _planeCount; ++i) {
        count += dot(_planes[i], select(min1, max1, _selectionMasks[i])) > 0;
    }
            
    return count == _planeCount;
}
            
bool culler::cullSphere(float4 sphere) const {
    int count = 0;
            
    // TODO: convert this from dot to a mul of 4, then finish plane 5,6
    // keep everything in simd reg.
    // Also may need to do 4 spheres at a time.
    
    float4 sphere1 = float4m(sphere.xyz,1);
    float radius = sphere.w;
    
    for (int i = 0; i < _planeCount; ++i) {
        count += dot(_planes[i], sphere1) < radius;
            
        // note: gpu can cull and do occlusion lookup
        // cpu can just do frustum culls
    }
                     
    return count == _planeCount;
}
            
void culler::cullBoxes(const float3* boxes, int count, uint8_t* results) const {
    // box array is 2x count
    for (int i = 0; i < count; ++i) {
        float3 min = boxes[2*i];
        float3 max = boxes[2*i+1];
        
        results[i] = cullBox(min, max);
        
        // note: gpu can cull and do occlusion lookup
        // cpu can just do frustum culls
    }
}

void culler::cullSpheres(const float4* sphere, int count, uint8_t* results) const {
    for(int i = 0; i < count; ++i) {
        results[i] = cullSphere(sphere[i]);
    }
}
    
bsphere culler::transformSphereTRU(bsphere sphere, const float4x4& modelTfm) {
    // May be better to convert to box with non-uniform scale
    // sphere gets huge otherwise.  Cache these too.
    
#if 0
    // not sure which code is smaller, still have to add t
    float size = reduce_max(decomposeScale(modelTfm));
    float radius = sphere.radius() * size;
    float4 sphereCenter = float4m(sphere.center(), 1);
    sphereCenter = modelTfm * sphereCenter;

    sphere = bsphere(sphereCenter.xyz, radius);
    return sphere;
#else
    // really just a 3x3 and translation
    const float3x3& m = as_float3x3(modelTfm);
    float3 t = m[3];
    
    float size = reduce_max(decomposeScale(modelTfm));
    float radius = sphere.radius() * size;
    float3 sphereCenter = m * sphere.center();
    sphereCenter += t;
    
    sphere = bsphere(sphereCenter, radius);
    return sphere;
#endif
}

bbox culler::transformBoxTRS(bbox box, const float4x4& modelTfm) {
    // Woth doing on cpu and caching.  So can still process an array
    // but should transform only ones thatt didn't change transform or bound.
    
#if 0
    // This is for a full general 4x4, but want a simpler affine version
    float4 min1 = float4m(box.min, 1);
    float4 max1 = float4m(box.max, 1);
    
    // convert the box to 8 pts first
    float4 pt[8];
   
    pt[0] = min1;
    pt[1] = max1;
        
    pt[2] = float4m(min1.xy, max1.zw);
    pt[3] = float4m(max1.xy, min1.zw);
    
    pt[4] = min1; pt[4].y = max1.y; // float4m(min1.x, max1.y, min1.zw),
    pt[5] = max1; pt[5].x = min1.x; // float4m(min1.x, max1.yzw),
        
    pt[6] = max1; pt[6].y = min1.y; // float4m(max1.x, min1.y, max1.zw),
    pt[7] = min1; pt[7].x = max1.x; // float4m(max1.x, min1.yzw),
    
    box.setInvalid();
    for (int i = 0; i < 8; ++i) {
        float3 v = (modelTfm * pt[i]).xyz;
        box.unionWith(v);
    }
    
#elif 0
    
    float3 min1 = box.min;
    float3 max1 = box.max;
    
    // really just a 3x3 and translation
    const float3x3& m = as_float3x3(modelTfm);
    float3 t = m[3];
    
    // convert the box to 8 pts first
    float3 pt[8];
   
    pt[0] = min1;
    pt[1] = max1;
        
    pt[2] = float3m(min1.xy, max1.z);
    pt[3] = float3m(max1.xy, min1.z);
    
    pt[4] = min1; pt[4].y = max1.y;
    pt[5] = max1; pt[5].x = min1.x;
        
    pt[6] = max1; pt[6].y = min1.y;
    pt[7] = min1; pt[7].x = max1.x;
    
    box.setInvalid();
    for (int i = 0; i < 8; ++i) {
        float3 v = m * pt[i];
        box.unionWith(v);
    }
    box.offset(t);
    
#else
    // This is way less setup on the points, and only 2 transformed points
    // instead of 8.  At least the inspiration for code below.
    // https://github.com/erich666/GraphicsGems/blob/master/gems/TransBox.c
   
    const float3x3& m = as_float3x3(modelTfm);
    float3 t = m[3];
   
    box.min = m * box.min;
    box.max = m * box.max;
    
    // swap back extrema that flipped due to rot/invert
    box.fix();
    
    box.offset(t);
    
#endif
    
    return box;
}

}

#endif

