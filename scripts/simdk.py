# add this to your ~/.lldbinit with
# command script import ~/yourpath/kram/scripts/simdk.py

import lldb

# simd library

# the vector ext allow various forms of addressing, but they require python eval to do so.
# but everything else fails.

def float2_summary(valobj, internal_dict):
    frame = valobj.GetFrame()
    name = valobj.GetName()
    x_value = frame.EvaluateExpression('{0}.x'.format(name))
    y_value = frame.EvaluateExpression('{0}.y'.format(name))
    x = x_value.GetValue()
    y = y_value.GetValue()
    return '({0},{1})'.format(x, y)
   
def float3_summary(valobj, internal_dict):
    frame = valobj.GetFrame()
    name = valobj.GetName()
    x_value = frame.EvaluateExpression('{0}.x'.format(name))
    y_value = frame.EvaluateExpression('{0}.y'.format(name))
    z_value = frame.EvaluateExpression('{0}.z'.format(name))
    x = x_value.GetValue()
    y = y_value.GetValue()
    z = z_value.GetValue()
    return '({0},{1},{2})'.format(x, y, z)

def float4_summary(valobj, internal_dict):
    frame = valobj.GetFrame()
    name = valobj.GetName()
    x_value = frame.EvaluateExpression('{0}.x'.format(name))
    y_value = frame.EvaluateExpression('{0}.y'.format(name))
    z_value = frame.EvaluateExpression('{0}.z'.format(name))
    w_value = frame.EvaluateExpression('{0}.w'.format(name))
    x = x_value.GetValue()
    y = y_value.GetValue()
    z = z_value.GetValue()
    w = w_value.GetValue()
    return '({0},{1},{2},{3})'.format(x, y, z, w)

def float2x2_summary(valobj, internal_dict):
    frame = valobj.GetFrame()
    name = valobj.GetName()
    
#    columns = frame.EvaluateExpression('{0}.columns'.format(name))
#
#    x = columns.GetValueAtIndex(0)
#    y = columns.GetValueAtIndex(1)
    
    structVar = valobj.GetChildAtIndex(0)
        
    x = structVar.GetChildAtIndex(0)
    y = structVar.GetChildAtIndex(1)
    
    # TODO: This isn't using the formatters, so may want to evalExpression
    return '\n{0}\n{1}'.format(x, y)
   
def float3x3_summary(valobj, internal_dict):
    frame = valobj.GetFrame()
    name = valobj.GetName()

    #columns = frame.EvaluateExpression('{0}.columns'.format(name))
    #x = columns.GetValueAtIndex(0)
    #y = columns.GetValueAtIndex(1)
    #z = columns.GetValueAtIndex(2)
    
    structVar = valobj.GetChildAtIndex(0)
        
    x = structVar.GetChildAtIndex(0)
    y = structVar.GetChildAtIndex(1)
    z = structVar.GetChildAtIndex(2)
    
    # TODO: This isn't using the formatters, so may want to evalExpression
    return '\n{0}\n{1}\n{2}'.format(x, y, z)

def float3x4_summary(valobj, interal_dict):
    return float3x3_summary(valobj, internal_dict)

def float4x4_summary(valobj, internal_dict):
    frame = valobj.GetFrame()
    name = valobj.GetName()
    
    structVar = valobj.GetChildAtIndex(0)
    #structVar = frame.FindVariable(name)
    #columns = structVar.GetChildMemberWithName("columns")
    
    #x = structVar.GetChildMemberWithName("columns").GetValueAtIndex(0)
    #y = structVar.GetChildMemberWithName("columns").GetValueAtIndex(1)
    #z = structVar.GetChildMemberWithName("columns").GetValueAtIndex(2)
    #w = structVar.GetChildMemberWithName("columns").GetValueAtIndex(3)
    
    x = structVar.GetChildAtIndex(0)
    y = structVar.GetChildAtIndex(1)
    z = structVar.GetChildAtIndex(2)
    w = structVar.GetChildAtIndex(3)
    
# how to make this work?  Just reports "None" is the frame incorrect?
#    x = float4_summary(x, internal_dict)
#    y = float4_summary(y, internal_dict)
#    z = float4_summary(z, internal_dict)
#    w = float4_summary(w, internal_dict)
    
    # TODO: This isn't using the formatters, so may want to evalExpression
    return '\n{0}\n{1}\n{2}\n{3}'.format(x, y, z, w)


def __lldb_init_module(debugger, internal_dict):

    # v.x produces
    #  summay string error.  These are using clang vector math ext

    # type summary add --summary-string "${var[0-1]}" simd_float2

    # This doesn't work
    #debugger.HandleCommand("type summary add --summary-string \"${var[0]},${var[1]}\" simd_float2")
    #debugger.HandleCommand("type summary add --summary-string \"${var[0]},${var[1]},${var[2]}\" simd_float3")
    #debugger.HandleCommand("type summary add --summary-string \"${var[0]},${var[1]},${var[2]},${var[3]}\" simd_float4")

    debugger.HandleCommand("type summary add -F simdk.float2_summary simd_float2")
    debugger.HandleCommand("type summary add -F simdk.float3_summary simd_float3")
    debugger.HandleCommand("type summary add -F simdk.float4_summary simd_float4")

    debugger.HandleCommand("type summary add -F simdk.float4x4_summary simd_float4x4")
    debugger.HandleCommand("type summary add -F simdk.float3x4_summary simd_float3x4")
    debugger.HandleCommand("type summary add -F simdk.float3x3_summary simd_float3x3")
    debugger.HandleCommand("type summary add -F simdk.float2x2_summary simd_float2x2")
   
    # debugger.HandleCommand("type summary add --summary-string \"${var.columns[0]}\n${var.columns[1]}\n${var.columns[2]}\n${var.columns[3]}\" simd_float4x4")
    
    #type summary add --summary-string "${var.columns[0]}, ${var.columns[1]}, ${var.columns[2]}, ${var.columns[3]}" simd::float4x4

    #type summary add --summary-string "${var.columns[0]}\n${var.columns[1]}\n${var.columns[2]}" simd_float3x3
    #type summary add --summary-string "${var.columns[0]}\n${var.columns[1]}\n${var.columns[2]}" simd::float3x3

    # simdk library
    
    debugger.HandleCommand("type summary add -F simdk.float2_summary int2a")
    debugger.HandleCommand("type summary add -F simdk.float3_summary int3a")
    debugger.HandleCommand("type summary add -F simdk.float4_summary int4a")

    debugger.HandleCommand("type summary add -F simdk.float2_summary long2a")
    debugger.HandleCommand("type summary add -F simdk.float3_summary long3a")
    debugger.HandleCommand("type summary add -F simdk.float4_summary long4a")

    debugger.HandleCommand("type summary add -F simdk.float2_summary half2a")
    debugger.HandleCommand("type summary add -F simdk.float3_summary half3a")
    debugger.HandleCommand("type summary add -F simdk.float4_summary half4a")

    # float234
    debugger.HandleCommand("type summary add -F simdk.float2_summary float2a")
    debugger.HandleCommand("type summary add -F simdk.float3_summary float3a")
    debugger.HandleCommand("type summary add -F simdk.float4_summary float4a")

    debugger.HandleCommand("type summary add -F simdk.float4x4_summary simdk::float4x4")
    debugger.HandleCommand("type summary add -F simdk.float3x3_summary simdk::float3x4")
    debugger.HandleCommand("type summary add -F simdk.float3x3_summary simdk::float3x3")
    debugger.HandleCommand("type summary add -F simdk.float2x2_summary simdk::float2x2")
   
    debugger.HandleCommand("type summary add -F simdk.float4_summary simdk::quatf")
    
    # double234
    debugger.HandleCommand("type summary add -F simdk.float2_summary double2a")
    debugger.HandleCommand("type summary add -F simdk.float3_summary double3a")
    debugger.HandleCommand("type summary add -F simdk.float4_summary double4a")

    debugger.HandleCommand("type summary add -F simdk.float4x4_summary simdk::double4x4")
    debugger.HandleCommand("type summary add -F simdk.float3x3_summary simdk::double3x4")
    debugger.HandleCommand("type summary add -F simdk.float3x3_summary simdk::double3x3")
    debugger.HandleCommand("type summary add -F simdk.float2x2_summary simdk::double2x2")
   
   
    #type summary add --summary-string "${var[0]},${var[1]}" float2a
    #type summary add --summary-string "${var[0]},${var[1]},${var[2]}}" float3a
    #type summary add --summary-string "${var[0]},${var[1]},${var[2]},${var[3]}" float4a

    #type summary add -F simdk.float2_summary float2a
    #type summary add -F simdk.float3_summary float3a
    #type summary add -F simdk.float4_summary float4a

    #type summary add --summary-string "${var.columns[0]}\n${var.columns[1]}\n${var.columns[2]}\n${var.columns[3]}" simdk::float4x4
    #type summary add --summary-string "${var.columns[0]}\n${var.columns[1]}\n${var.columns[2]}" simdk::float3x3
