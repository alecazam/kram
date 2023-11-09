#!/usr/bin/env python3

# kram - Copyright 2020-2023 by Alec Miller. - MIT License
# The license and copyright notice shall be included
# in all copies or substantial portions of the Software.
#
# This is derived from GpuMemDumpVis.py, and like it, doesn't handle aliasing.
# Only reads size and not offset of allocations.  But Perfetto can't handle overlapping rects.
# Unlike the png, Pefetto can zoom in and display the allocation names and sizes.

#
# Copyright (c) 2018-2023 Advanced Micro Devices, Inc. All rights reserved.
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.
#

import argparse
import json

# this is all for doing queries, not writing out traces which only has C++ calls
# did a pip3 install perfetto, but no docs on it https://pypi.org/project/perfetto/
# import perfetto
# https://android.googlesource.com/platform/external/perfetto/+/refs/heads/master/python/example.py
# from perfetto.trace_processor import TraceProcessor, TraceProcessorConfig
# https://perfetto.dev/docs/analysis/batch-trace-processor
#
# https://perfetto.dev/docs/reference/synthetic-track-event
# No phtyon writer, and so everything has to be protobuf based
# https://perfetto.dev/docs/design-docs/protozero

PROGRAM_VERSION = 'Vulkan/D3D12 Memory Allocator Dump Perfetto 3.0.3'

# DONE: Perfetto can't handle empty string for name on the allocs
#   so had to set them to 'M'.  This is getting fixed.
#   https://r.android.com/2817378
# DONE: cname doesn't seem to import and has limited colors
#  cname is ignored by parser, there is a layer and bg color, but need to set color for a block
#  find out how Peretto consistently colors layers by name
#  https://github.com/google/perfetto/blob/master/src/trace_processor/importers/json/json_trace_parser.cc
#  https://github.com/google/perfetto/issues/620
# DONE: call 'set timestamp format' to seconds from UI (cmd+shift+P), 
#  but this doesn't affect duration display which is still formatted. Second lines reflect MB.
# TODO: Pefetto doesn't want to extend/support Cataylst json format, but doesn't have a json of its own
#  https://github.com/google/perfetto/issues/622
#  https://github.com/google/perfetto/issues/623
# TODO: add totals, already know none are empty.  Add these to a summary track.
#   can have count/mem of each time and potentially across all types

# dx12 or vulkan
currentApi = ""

# input data dictionary
data = {}

# remap names to index for obfuscation, can then share mem maps
nameIndexer = { "": 0 }
nameIndexerCounter = 0

 # now convert the dictionaries to new dictionaries, and then out to json 
 # TODO: ms = *1e-3 not quite kb, ns = *1E-9
 # when using ms, then values can hit minute and hour time conversions which /60 instead of /100,
 # but ns is also goofy due to 1e9 being gb.
perfettoDict = {
    'displayTimeUnit': 'ms', 
    'systemTraceEvents': 'SystemTraceData',
    'traceEvents': [],
}    

def ParseArgs():
    argParser = argparse.ArgumentParser(description='Visualization of Vulkan/D3D12 Memory Allocator JSON dump in Perfetto.')
    argParser.add_argument('DumpFile', help='Path to source JSON file with memory dump created by Vulkan/D3D12 Memory Allocator library')
    argParser.add_argument('-v', '--version', action='version', version=PROGRAM_VERSION)
    # TODO: derive output from input name if not present
    argParser.add_argument('-o', '--output', required=True, help='Path to destination trace file')
    return argParser.parse_args()

def GetDataForMemoryPool(poolTypeName):
    global data
    if poolTypeName in data:
        return data[poolTypeName]
    else:
        newPoolData = {'DedicatedAllocations':[], 
                       'Blocks':[], 
                       'CustomPools':{}}
        data[poolTypeName] = newPoolData
        return newPoolData

def ProcessBlock(poolData, block):
    blockInfo = {'ID': block[0], 
                 'Size': int(block[1]['TotalBytes']), 
                 'Suballocations':[]}
    for alloc in block[1]['Suballocations']:
        allocData = {'Type': alloc['Type'], 
                     'Size': int(alloc['Size']), 
                     'Usage': int(alloc['Usage']) if 'Usage' in alloc else 0,
                     'Name': alloc['Name'] if 'Name' in alloc else 'M' }
        blockInfo['Suballocations'].append(allocData)
    poolData['Blocks'].append(blockInfo)
    
def IsDataEmpty():
    global data
    for poolData in data.values():
        if len(poolData['DedicatedAllocations']) > 0:
            return False
        if len(poolData['Blocks']) > 0:
            return False
        for customPool in poolData['CustomPools'].values():
            if len(customPool['Blocks']) > 0:
                return False
            if len(customPool['DedicatedAllocations']) > 0:
                return False
    return True

def RemoveEmptyType():
    global data
    for poolType in list(data.keys()):
        pool = data[poolType]
        if len(pool['DedicatedAllocations']) > 0:
           continue
        if len(pool['Blocks']) > 0:
            continue
        empty = True
        for customPool in pool['CustomPools'].values():
            if len(customPool['Blocks']) > 0:
                empty = False
                break
            if len(customPool['DedicatedAllocations']) > 0:
                empty = False
                break
        if empty:
            del data[poolType]


def AllocTypeToCategory(type, usage):
    global currentApi
    if type == 'FREE':
        return "  "
    elif type == 'UNKNOWN':
        return "??"

    if currentApi == 'Vulkan':
        if type == 'BUFFER':
            # https://registry.khronos.org/vulkan/specs/1.3-extensions/man/html/VkBufferUsageFlagBits.html
            if (usage & 0x0080) != 0:  # VK_USAGE_VERTEX_BUFFER_BIT
                return "VB"
            elif (usage & 0x040) != 0: # INDEX_BUFFER
                return "IB"
            elif (usage & 0x0014) != 0: # UNIFORM_BUFFER | UNIFORM_TEXEL_BUFFER
                return "UB"
            elif (usage & 0x0100) != 0: # INDIRECT_BUFFER
                return "DB"
            elif (usage & 0x0028) != 0: # STORAGE_BUFFER | STORAGE_TEXEL_BUFFER
                return "SB"
            elif (usage & 0x0003) != 0: # Staging buffer only sets 1 or 2 bit, calling this MB for memory
                return "MB"
            else:
                return "?B" # TODO: getting this on some buffers, so identify more
        elif type == 'IMAGE_OPTIMAL':
            # TODO: need tex type (2d, 3d, ...)
            # https://registry.khronos.org/vulkan/specs/1.3-extensions/man/html/VkImageUsageFlagBits.html
            if (usage & 0x20) != 0: # DEPTH_STENCIL_ATTACHMENT
                return "DT"
            elif (usage & 0xD0) != 0: # INPUT_ATTACHMENT | TRANSIENT_ATTACHMENT | COLOR_ATTACHMENT
                return "RT"
            elif (usage & 0x4) != 0: # SAMPLED
                return "TT"
            else:
                return "?T"
        elif type == 'IMAGE_LINEAR' :
            return "LT"
        elif type == 'IMAGE_UNKNOWN':
            return "?T"
    elif currentApi == 'Direct3D 12':
        if type == 'BUFFER':
            return "?B"
        elif type == 'TEXTURE1D' or type == 'TEXTURE2D' or type == 'TEXTURE3D':
            if (usage & 0x2) != 0: # D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL
                return "DT"
            elif (usage & 0x5) != 0: # D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET | D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS
                return "RT"
            elif (usage & 0x8) == 0: # Not having D3D12_RESOURCE_FLAG_DENY_SHARED_RESOURCE
                return "TT"
            else:
                return "?T"
    else:
        print("Unknown graphics API!")
        exit(1)
    assert False
    return "??"

# not many cname colors to choose from
# https://github.com/catapult-project/catapult/blob/master/tracing/tracing/base/color_scheme.html
def AllocCategoryToColor(category):
    color = "grey"

    if category[1] == 'B':
        if category == 'VB':
            color = "olive"
        elif category == 'IB':
            color = "white"
        elif category == 'DB':
            color = "white"
        else:
            color = "white"
    elif category[1] == 'T':
        color = "yellow"
    
    return color

# a way to obscure names, so can share maps publicly
def RemapName(name):

    # TODO: perfetto doesn't uniquely color the names if using numbers
    # or even N + num when they differ.  Find out what color criteria needs.
    useNameIndexer = False
    if useNameIndexer:
        global nameIndexer
        global nameIndexerCounter
        
        if name in nameIndexer:
            name = str(nameIndexer[name])
        else:
            nameIndexer[name] = nameIndexerCounter
            name = str(nameIndexerCounter)
            nameIndexerCounter += 1

    return name

def AddTraceEventsAlloc(alloc, addr, blockCounter):
    global perfettoDict
    
    # settings

    # this makes it harder to look for strings, but Perfetto can't control color
    # so this prepends the type/category
    prependCategory = True

    # this has a downside that empty blocks and tail end of block isn't clear
    # but it does cut down on data 
    skipFreeAlloc = True

    isFreeAlloc = alloc['Type'] == 'FREE'

    if (skipFreeAlloc and isFreeAlloc):
        return

    size = alloc['Size']
    category = AllocTypeToCategory(alloc['Type'], alloc['Usage'])
    
    # this is optinonal, Pefetto will psuedocolor different names
    # but this is one option for consistent coloring
    # perfetto doesn't seem to honor set cname
    # https://github.com/catapult-project/catapult/blob/master/tracing/tracing/base/color_scheme.html
    #color = AllocCategoryToColor(category)

    name = RemapName(alloc['Name'])

    # prepend category
    if (prependCategory and not isFreeAlloc):
        name = category + "-" + name

    traceEvent = {
        'name': name,
        'ph': 'X',
        'ts': int(addr),
        'dur': int(size),
        'tid': int(blockCounter),
        #'pid': 0,
        #'cname': color, 
        'cat': category
    }

    # complete event X is much less data than B/E
    # these cannot be nested or overlap, so no aliasing
    perfettoDict['traceEvents'].append(traceEvent)

def AddTraceEventsBlock(block, blockCounter):
    global perfettoDict

    # TODO: could collect together contig empty blocks.  Lots of 'M' values otherwise.
    #  this would require passing down size 
       
    addr = int(0)
    for alloc in block['Suballocations']:
        AddTraceEventsAlloc(alloc, addr, blockCounter)
        addr = addr + int(alloc['Size'])

def AddBlockName(blockName, blockCounter):
    global perfettoDict

    perfettoDict['traceEvents'].append({
        'name': 'thread_name',
        'ph': 'M',
        'tid': int(blockCounter),
        #'pid': 0,
        'args': {
            'name': blockName
        }
    })

def AddProcessName(processName):
    global perfettoDict

    perfettoDict['traceEvents'].append({
        'name': 'process_name',
        'ph': 'M',
        'pid': 0,
        'args': {
            'name': processName
        }
    })


def AddTraceEvents(addBlockNames):
    global perfettoDict
    blockCounter = 0
    
    # DONE: add all block names first across all pools, then add the allocations
    # TODO: do dedicated allocations need sorted?
    # TODO: could specify pid for memType, but think has to be stored per alloc

    # for poolData in data.values():
    for memType in sorted(data.keys()):
        poolData = data[memType]

        # strip 'Type ' off string
        poolIndex = memType[5:]
        
        # report for default pool

        # block allocs
        blockIndex = 0
        for block in poolData['Blocks']:
            if addBlockNames:
                blockName = "T{} b{} {}".format(poolIndex, blockIndex, block['ID'])
                AddBlockName(blockName, blockCounter)
            else:
                AddTraceEventsBlock(block, blockCounter)
            blockCounter += 1
            blockIndex += 1

        # dedicated allocs
        allocationIndex = 0
        for dedicatedAlloc in poolData['DedicatedAllocations']:
            if addBlockNames:
                blockName = 'T{} a{} {}'.format(poolIndex, allocationIndex, dedicatedAlloc['Name'])
                AddBlockName(blockName, blockCounter)
            else:
                AddTraceEventsAlloc(dedicatedAlloc, 0, blockCounter)
            blockCounter += 1
            allocationIndex += 1
            
        # repeat for custom pools
        for customPoolName, customPoolData in poolData['CustomPools'].items():
           
            # pool block allocs
            blockIndex = 0
            for block in customPoolData['Blocks']:
                if addBlockNames:
                    blockName = 'T{} {} b{} {}'.format(poolIndex, customPoolName, blockIndex, block['ID'])
                    AddBlockName(blockName, blockCounter)
                else:
                    AddTraceEventsBlock(block, blockCounter)
                blockCounter += 1
                blockIndex += 1

            # pool dedicated allocs
            allocationIndex = 0
            for dedicatedAlloc in customPoolData['DedicatedAllocations']:
                if addBlockNames:
                    blockName = 'T{} {} a{} {}'.format(poolIndex, customPoolName, allocationIndex, dedicatedAlloc['Name'])
                    AddBlockName(blockName, blockCounter)
                else:
                    AddTraceEventsAlloc(dedicatedAlloc, 0, blockCounter)
                blockCounter += 1
                allocationIndex += 1
                
       
if __name__ == '__main__':
    args = ParseArgs()
    jsonSrc = json.load(open(args.DumpFile, 'rb'))
 
    if 'General' in jsonSrc:
        currentApi = jsonSrc['General']['API']
    else:
        print("Wrong JSON format, cannot determine graphics API!")
        exit(1)
            
    # Process default pools
    if 'DefaultPools' in jsonSrc:
        for memoryPool in jsonSrc['DefaultPools'].items():
            poolData = GetDataForMemoryPool(memoryPool[0])
            # Get dedicated allocations
            for dedicatedAlloc in memoryPool[1]['DedicatedAllocations']:
                allocData = {'Type': dedicatedAlloc['Type'], 
                             'Size': int(dedicatedAlloc['Size']), 
                             'Usage': int(dedicatedAlloc['Usage']),
                             'Name': dedicatedAlloc['Name'] if 'Name' in dedicatedAlloc else 'M'}
                poolData['DedicatedAllocations'].append(allocData)
            # Get allocations in block vectors
            for block in memoryPool[1]['Blocks'].items():
                ProcessBlock(poolData, block)
             
    # Process custom pools
    if 'CustomPools' in jsonSrc:
        for memoryPool in jsonSrc['CustomPools'].items():
            poolData = GetDataForMemoryPool(memoryPool[0])
            for pool in memoryPool[1]:
                poolName = pool['Name']
                poolData['CustomPools'][poolName] = {'DedicatedAllocations':[], 'Blocks':[]}
                # Get dedicated allocations
                for dedicatedAlloc in pool['DedicatedAllocations']:
                    allocData = {'Type': dedicatedAlloc['Type'], 
                                 'Size': int(dedicatedAlloc['Size']), 
                                 'Usage': int(dedicatedAlloc['Usage']),
                                 'Name': dedicatedAlloc['Name'] if 'Name' in dedicatedAlloc else 'M'}
                    poolData['CustomPools'][poolName]['DedicatedAllocations'].append(allocData)
                # Get allocations in block vectors
                for block in pool['Blocks'].items():
                    ProcessBlock(poolData['CustomPools'][poolName], block)
    
    
    if IsDataEmpty():
        print("There is nothing to write. Please make sure you generated the stats string with detailed map enabled.")
        exit(1)
    RemoveEmptyType()
    
    # add process name to indicate source file
    AddProcessName(args.DumpFile)

    # add thread names to indicate block names
    AddTraceEvents(True)

    # add the actual memory block size/offset/name
    AddTraceEvents(False)

    # setting
    compactJson = False

    if compactJson:
         perfettoJson = json.dumps(perfettoDict)
    else:
        perfettoJson = json.dumps(perfettoDict, indent=0)
    
    with open(args.output, "w") as outfile:
        outfile.write(perfettoJson)
    
"""
Main data structure - variable `data` - is a dictionary. Key is string - memory type name. Value is dictionary of:
- Fixed key 'DedicatedAllocations'. Value is list of objects, each containing dictionary with:
    - Fixed key 'Type'. Value is string.
    - Fixed key 'Size'. Value is int.
    - Fixed key 'Usage'. Value is int.
    - Key 'Name' optional, Value is string
    - Key'CustomData' optional
- Fixed key 'Blocks'. Value is list of objects, each containing dictionary with:
    - Fixed key 'ID'. Value is int.
    - Fixed key 'Size'. Value is int.
    - Fixed key 'Suballocations'. Value is list of objects as above.
- Fixed key 'CustomPools'. Value is dictionary.
  - Key is string with pool ID/name. Value is a dictionary with:
    - Fixed key 'DedicatedAllocations'. Value is list of objects as above.
    - Fixed key 'Blocks'. Value is a list of objects representing memory blocks as above.
"""
