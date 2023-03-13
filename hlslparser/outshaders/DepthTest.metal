#include <metal_stdlib>

// https://developer.apple.com/forums/thread/726383
using namespace metal;

struct SamplePSNS {
    struct InputPS {
        float4 position [[position]];
    };
    
    thread depth2d<float>& shadowMap;
    thread sampler& sampleBorder;
    
    float4 SamplePS(InputPS input) {
        return shadowMap.sample_compare(sampleBorder, input.position.xy, input.position.z);
    };

    
    SamplePSNS(
      thread depth2d<float>& shadowMap,
      thread sampler& sampleBorder)
       : shadowMap(shadowMap),
         sampleBorder(sampleBorder)
    {}
};

fragment float4 SamplePS(
    SamplePSNS::InputPS input [[stage_in]],
    depth2d<float> shadowMap [[texture(0)]],
    sampler sampleBorder [[sampler(0)]])
{
    SamplePSNS shader(shadowMap, sampleBorder);
    return shader.SamplePS(input);
}
