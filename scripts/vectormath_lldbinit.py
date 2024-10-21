# simd library
type summary add --summary-string "${var.x}, ${var.y}" simd_float2
type summary add --summary-string "${var.x}, ${var.y}, ${var.z}" simd_float3
type summary add --summary-string "${var.x}, ${var.y}, ${var.z}, ${var.w}" simd_float4

type summary add --summary-string "${var.columns[0]}\n${var.columns[1]}\n${var.columns[2]}\n${var.columns[3]}" simd_float4x4
#type summary add --summary-string "${var.columns[0]}, ${var.columns[1]}, ${var.columns[2]}, ${var.columns[3]}" simd::float4x4

type summary add --summary-string "${var.columns[0]}\n${var.columns[1]}\n${var.columns[2]}" simd_float3x3
#type summary add --summary-string "${var.columns[0]}\n${var.columns[1]}\n${var.columns[2]}" simd::float3x3

# simdk library
type summary add --summary-string "${var.x}, ${var.y}" float2p
type summary add --summary-string "${var.x}, ${var.y}, ${var.z}" float3p
type summary add --summary-string "${var.x}, ${var.y}, ${var.z}, ${var.w}" float4p

type summary add --summary-string "${var.x}, ${var.y}" float2a
type summary add --summary-string "${var.x}, ${var.y}, ${var.z}" float3a
type summary add --summary-string "${var.x}, ${var.y}, ${var.z}, ${var.w}" float4a

#type summary add --summary-string "${var.columns[0]}\n${var.columns[1]}\n${var.columns[2]}\n${var.columns[3]}" simdk::float4x4
#type summary add --summary-string "${var.columns[0]}\n${var.columns[1]}\n${var.columns[2]}" simdk::float3x3
