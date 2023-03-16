#include "HLSLParser.h"

//#include "GLSLGenerator.h"
#include "HLSLGenerator.h"
#include "MSLGenerator.h"

#include <stdio.h>
#include <sys/stat.h>

#include <filesystem>

using namespace std;

enum Language
{
    Language_MSL,
	Language_HLSL,
};

bool ReadFile( const char* fileName, string& str )
{
    struct stat stats = {};
    if (stat(fileName, &stats) < 0) {
        return false;
    }
    size_t size = (int64_t)stats.st_size;

    str.resize(size);

    FILE* fp = fopen(fileName, "r");
    if (fp) {
        fread((char*)str.data(), 1, size, fp);
    }
    fclose(fp);
    return true;
}

void PrintUsage()
{
	fprintf(stderr,
        "usage: hlslparser [-h|-g] -i shader.hlsl -o [shader.hlsl | shader.metal]\n"
		 "Translate DX9-style HLSL shader to HLSL/MSL shader.\n"
         " -i          input HLSL\n"
         " -o          output HLSL or MSL\n"
		 "optional arguments:\n"
         " -g          debug mode, preserve comments\n"
         " -h, --help  show this help message and exit\n"
		);
}

// Taken from KrmaLog.cpp
static bool endsWith(const string& value, const string& ending)
{
    if (ending.size() > value.size()) {
        return false;
    }

    // reverse comparison at end of value
    if (value.size() < ending.size())
        return false;
    uint32_t start = (uint32_t)(value.size() - ending.size());
        
    for (uint32_t i = 0; i < ending.size(); ++i) {
        if (value[start + i] != ending[i])
            return false;
    }
    
    return true;
}

// Alec, brought over from kram
static string filenameNoExtension(const char* filename)
{
    const char* dotPosStr = strrchr(filename, '.');
    if (dotPosStr == nullptr)
        return filename;
    auto dotPos = dotPosStr - filename;
    
    // now chop off the extension
    string filenameNoExt = filename;
    return filenameNoExt.substr(0, dotPos);
}

int main( int argc, char* argv[] )
{
	using namespace M4;

	// Parse arguments
	string fileName;
	const char* entryName = NULL;

	// TODO: could we take modern DX12 HLSL and translate to MSL only
	// That would simplify all this.  What spriv-cross already does though.
	// Could drop HLSLGenerator then, and just use this to gen MSL.
	// Much of the glue code can just be in a header, but having it
	// in parser, lets this only splice code that is needed.

	Language language = Language_MSL;
	HLSLTarget target = HLSLTarget_PixelShader;
    string outputFileName;
    bool isDebug = false;
    
	for( int argn = 1; argn < argc; ++argn )
	{
		const char* const arg = argv[ argn ];

		if( String_Equal( arg, "-h" ) || String_Equal( arg, "--help" ) )
		{
			PrintUsage();
			return 0;
		}
		
        else if( String_Equal( arg, "-o" ) || String_Equal( arg, "-output" ) )
        {
            if ( ++argn < argc )
                outputFileName = argv[ argn ];
        }
        else if( String_Equal( arg, "-i" ) || String_Equal( arg, "-input" ) )
		{
            if ( ++argn < argc )
                fileName = argv[ argn ];
		}
        else if ( String_Equal( arg, "-g" ))
        {
            // will preserve double-slash comments where possible
            isDebug = true;
        }
        
// This is derived from end characters of entry point
//        else if( String_Equal( arg, "-vs" ) )
//        {
//            target = HLSLTarget_VertexShader;
//        }
//        else if( String_Equal( arg, "-fs" ) )
//        {
//            target = HLSLTarget_PixelShader;
//        }
 // TODO: require a arg to set entryName
//		else if( entryName == NULL )
//		{
//			entryName = arg;
//		}
		else
		{
			Log_Error( "Too many arguments\n" );
			PrintUsage();
			return 1;
		}
	}

	if( fileName.empty() )
	{
		Log_Error( "Missing source filename\n" );
		PrintUsage();
		return 1;
	}
    if( !endsWith( fileName, "hlsl" ) )
    {
        Log_Error( "Input filename must end with .hlsl\n" );
        PrintUsage();
        return 1;
    }
    
    if( outputFileName.empty() )
    {
        Log_Error( "Missing dest filename\n" );
        PrintUsage();
        return 1;
    }
    if( endsWith( outputFileName, "hlsl" ) )
    {
        language = Language_HLSL;
    }
    else if( endsWith( outputFileName, "metal" ) )
    {
        language = Language_MSL;
    }
    else
    {
        Log_Error( "Output file must end with .hlsl or msls\n" );
        PrintUsage();
        return 1;
    }
    
    // replace the extension on the output file
    outputFileName = filenameNoExtension( outputFileName.c_str() );
    
    // Allow a mix of shaders in file.
    // Code now finds entry points.
    // outputFileName += (target == HLSLTarget_PixelShader) ? "PS" : "VS";
    
    if ( language == Language_MSL )
    {
        outputFileName += ".metal";
    }
    else if ( language == Language_HLSL )
    {
        outputFileName += ".hlsl";
    }
    
    // Win build on github is failing on this, so skip for now
#ifndef WIN32
    // find  full pathname of the fileName, so that errors are logged
    // in way that can be clicked to. absolute includes .. in it, canonical does not.
    auto path = filesystem::path(fileName);
    fileName = filesystem::canonical( path );
    
    // if this file doesn't exist, then canonical throws exception
    path = filesystem::path(outputFileName);
    if (filesystem::exists(path))
    {
        outputFileName = filesystem::canonical( path );
        
        if ( outputFileName == fileName )
        {
            Log_Error( "Src and Dst filenames match.  Exiting.\n" );
            return 1;
        }
    }
#endif
    
    //------------------------------------
    // Now start the work
    
	// Read input file
    string source;
    if (!ReadFile( fileName.c_str(), source ))
    {
        Log_Error( "Input file not found\n" );
        return 1;
    }

    
    
	// Parse input file
	Allocator allocator;
	HLSLParser parser( &allocator, fileName.c_str(), source.data(), source.size() );
    if (isDebug)
    {
        parser.SetKeepComments(true);
    }
	HLSLTree tree( &allocator );
	if( !parser.Parse( &tree ) )
	{
		Log_Error( "Parsing failed\n" );
		return 1;
	}
    
    int status = 0;
    
    // build a list of entryPoints
    Array<const char*> entryPoints(&allocator);
    if (entryName != nullptr)
    {
        entryPoints.PushBack(entryName);
    }
    else
    {
        // search all functions with designated endings
        HLSLStatement* statement = tree.GetRoot()->statement;
        while (statement != NULL)
        {
            if (statement->nodeType == HLSLNodeType_Function)
            {
                HLSLFunction* function = (HLSLFunction*)statement;
                const char* name = function->name;
                
                if (endsWith(name, "VS"))
                {
                    entryPoints.PushBack(name);
                }
                else if (endsWith(name, "PS"))
                {
                    entryPoints.PushBack(name);
                }
                else if (endsWith(name, "CS"))
                {
                    entryPoints.PushBack(name);
                }
            }

            statement = statement->nextStatement;
        }
    }
    
    string output;
    
    for (uint32_t i = 0; i < entryPoints.GetSize(); ++i)
    {
        const char* entryPoint = entryPoints[i];
        entryName = entryPoint;
        if (endsWith(entryPoint, "VS"))
            target = HLSLTarget_VertexShader;
        else if (endsWith(entryPoint, "PS"))
            target = HLSLTarget_PixelShader;
        else if (endsWith(entryPoint, "CS"))
            target = HLSLTarget_ComputeShader;
            
        // Generate output
        if (language == Language_HLSL)
        {
            HLSLGenerator generator;
            if (generator.Generate( &tree, target, entryName ))
            {
                // write the buffer out
                output += generator.GetResult();
            }
            else
            {
                Log_Error( "Translation failed, aborting\n" );
                status = 1;
            }
        }
        else if (language == Language_MSL)
        {
            MSLGenerator generator;
            if (generator.Generate( &tree, target, entryName ))
            {
                // write the buffer out
                output += generator.GetResult();
            }
            else
            {
                Log_Error( "Translation failed, aborting\n" );
                status = 1;
            }
        }
        
        if (status != 0)
            break;
    }
    
    if (status == 0)
    {
        // using wb to avoid having Win convert \n to \r\n
        FILE* fp = fopen( outputFileName.c_str(), "wb" );
        if ( !fp )
        {
            Log_Error( "Could not open output file %s\n", outputFileName.c_str() );
            return 1;
        }
        
        fprintf(fp, "%s", output.c_str());
        fclose( fp );
    }
        
    // It's not enough to return 1 from main, but set exit code.
    if (status)
        exit(status);
    
    return status;
}
