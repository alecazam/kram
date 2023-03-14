//--------------------------------------------------------------------------------------
// File: Compute.hlsl
//
// This file contains the Compute Shader to perform array A + array B
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

// adapted from https://learn.microsoft.com/en-us/windows/win32/direct3d11/direct3d-11-advanced-stages-compute-create

struct BufType
{
    int i;
    float f;
};

StructuredBuffer<BufType> Buffer0 : register(t0);
StructuredBuffer<BufType> Buffer1 : register(t1);

RWStructuredBuffer<BufType> BufferOut : register(u2);

// TODO: support numthreads designator
// [numthreads(1, 1, 1)]
void ComputeCS( uint3 tid : SV_DispatchThreadID )
{
    BufferOut[tid.x].i = Buffer0[tid.x].i + Buffer1[tid.x].i;
    BufferOut[tid.x].f = Buffer0[tid.x].f + Buffer1[tid.x].f;
}

//-------------------

// Need better way to search entry points, don't use filename
// Just search functions ending in VS/PS/CS
// [numthreads(1, 1, 1)]
// void ComputeCS( uint3 DTid : SV_DispatchThreadID )
// {
//     int i0 = asint( Buffer0.Load( DTid.x*8 ) );
//     float f0 = asfloat( Buffer0.Load( DTid.x*8+4 ) );
//     int i1 = asint( Buffer1.Load( DTid.x*8 ) );
//     float f1 = asfloat( Buffer1.Load( DTid.x*8+4 ) );
//
//     BufferOut.Store( DTid.x*8, asuint(i0 + i1) );
//     BufferOut.Store( DTid.x*8+4, asuint(f0 + f1) );
//
//     There is this new templated Load call.
//     This greatly simplifies using BAB.
//     float3 pos = Buffer0.Load<float3>(idx);
//     Bufferout.Store<float3>(idx);
// }

