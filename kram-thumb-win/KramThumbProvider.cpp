#include "KramLib.h"

#include <shlwapi.h>
#include <thumbcache.h> // For IThumbnailProvider.
#include <wrl/client.h> // For ComPtr
#include <new>
#include <atomic>
#include <vector>

using namespace kram;
using namespace std; // or STL_NAMESPACE

template <typename T>
using ComPtr = Microsoft::WRL::ComPtr<T>;

// This thumbnail provider implements IInitializeWithStream to enable being hosted
// in an isolated process for robustness.
//
// This will build to a DLL
// reg:   regsrv32.exe KramThumbProvider.dll
// unreg: regsrv32.exe /u KramThumbProvider.dll

inline void* KLOGF(uint32_t code, const char* format, ...)
{
    string str;

    va_list args;
    va_start(args, format);
    /* int32_t len = */ append_vsprintf(str, format, args);
    va_end(args);

    // log here, so it can see it in Console.  But this never appears.
    // How are you supposed to debug failures?  Resorted to passing a unique code into this call.
    // It wasn't originally supposed to generate an NSError
    // NSLog(@"%s", str.c_str());

    // Console prints this as <private>, so what's the point of producing a localizedString ?
    // This doesn't seem to work to Console app, but maybe if logs are to terminal
    // sudo log config --mode "level:debug" --subsystem com.hialec.kramv

    //NSString* errorText = [NSString stringWithUTF8String:str.c_str()];
    // return [NSError errorWithDomain:@"com.hialec.kramv" code:code userInfo:@{NSLocalizedDescriptionKey : errorText}];
    return nullptr;
}

struct ImageToPass {
    KTXImage image;
    KTXImageData imageData;
};

class KramThumbProvider final : public IInitializeWithStream, public IThumbnailProvider 
{
public:
    KramThumbProvider()
        : mReferences(1)
        , mStream{} {
    }

    virtual ~KramThumbProvider() {
    }

    // IUnknown
    IFACEMETHODIMP QueryInterface(REFIID riid, void** ppv) {
        static const QITAB qit[] = {
            QITABENT(KramThumbProvider, IInitializeWithStream),
            QITABENT(KramThumbProvider, IThumbnailProvider),
            { 0 },
        };
        return QISearch(this, qit, riid, ppv);
    }

    IFACEMETHODIMP_(ULONG) AddRef() {
        return ++mReferences;
    }

    IFACEMETHODIMP_(ULONG) Release() {
        long refs = --mReferences;
        if (!refs) {
            delete this;
        }
        return refs;
    }

    // IInitializeWithStream
    IFACEMETHODIMP Initialize(IStream* pStream, DWORD /*grfMode*/) {
        HRESULT hr = E_UNEXPECTED;  // can only be inited once
        if (!mStream) {
            // take a reference to the stream if we have not been inited yet
            hr = pStream->QueryInterface(mStream.ReleaseAndGetAddressOf());
        }
        return hr;
    }

    // IThumbnailProvider
    IFACEMETHODIMP GetThumbnail(UINT cx, HBITMAP* phbmp, WTS_ALPHATYPE* pdwAlpha) {
       
        // read from stream and create a thumbnail
        if (!ImageToHBITMAP(cx, phbmp)) {
            return E_OUTOFMEMORY;
        }

        // always 4 channels
        *pdwAlpha = WTSAT_ARGB;
         
        return S_OK;
    }

private:
    bool ImageToHBITMAP(uint32_t maxSize, HBITMAP* phbmp)
    {
        if (!mStream)
            return false;

        // only know that we have a stream
        const char* filename = "";

        ULARGE_INTEGER streamSizeUint = {};
        IStream_Size(mStream.Get(), &streamSizeUint);
        size_t streamSize = (size_t)streamSizeUint.QuadPart;

        // TODO: for now read the entire stream in, but eventually test the first 4-6B for type
        vector<uint8_t> streamData;
        streamData.resize(streamSize);
        ULONG bytesRead = 0;
        HRESULT hr = mStream->Read(streamData.data(), streamSize, &bytesRead);  // can only read ULONG
        if (FAILED(hr) || streamSize != bytesRead)
            return false;


        // https://learn.microsoft.com/en-us/windows/win32/api/thumbcache/nf-thumbcache-ithumbnailprovider-getthumbnail

        std::shared_ptr<ImageToPass> imageToPass = std::make_shared<ImageToPass>();
        TexEncoder decoderType = kTexEncoderUnknown;
        uint32_t imageWidth, imageHeight;

        {
            KTXImage& image = imageToPass->image;
            KTXImageData& imageData = imageToPass->imageData;

            if (!imageData.open(streamData.data(), streamData.size(), image)) {
                KLOGF(2, "kramv %s could not open file\n", filename);
                return false;
            }

            // This will set decoder
            auto textureType = MyMTLTextureType2D;  // image.textureType
            if (!validateFormatAndDecoder(textureType, image.pixelFormat, decoderType)) {
                KLOGF(3, "format decode only supports ktx and ktx2 output");
                return false;
            }

            imageWidth = std::max(1U, image.width);
            imageHeight = std::max(1U, image.height);
        }

        // This is retina factor
        //float requestScale = 1.0;  // request.scale;

        // One of the sides must match maximumSize, but can have
        // different aspect ratios below that on a given sides.
        struct NSSize {
            float width;
            float height;
        };
        NSSize contextSize = { (float)maxSize, (float)maxSize };  

        // compute w/h from aspect ratio of image
        float requestWidth, requestHeight;

        float imageAspect = imageWidth / (float)imageHeight;
        if (imageAspect >= 1.0f) {
            requestWidth = contextSize.width;
            requestHeight = std::clamp((contextSize.width / imageAspect), 1.0f, contextSize.height);
        }
        else {
            requestWidth = std::clamp((contextSize.height * imageAspect), 1.0f, contextSize.width);
            requestHeight = contextSize.height;
        }

        // will be further scaled by requestScale (only on macOS)
        contextSize.width = requestWidth;
        contextSize.height = requestHeight;

        //-----------------

        KTXImage& image = imageToPass->image;

        bool isPremul = image.isPremul();
        bool isSrgb = isSrgbFormat(image.pixelFormat);

        // unpack a level to get the blocks
        uint32_t mipNumber = 0;
        uint32_t mipCount = image.mipCount();

        uint32_t w, h, d;
        for (uint32_t i = 0; i < mipCount; ++i) {
            image.mipDimensions(i, w, h, d);
            if (w > contextSize.width || h > contextSize.height) {
                mipNumber++;
            }
        }

        // clamp to smallest
        mipNumber = std::min(mipNumber, mipCount - 1);
        image.mipDimensions(mipNumber, w, h, d);

        //-----------------

        uint32_t chunkNum = 0;  // TODO: could embed chunk(s) to gen thumbnail from, cube/array?
        uint32_t numChunks = image.totalChunks();

        vector<uint8_t> mipData;

        // now decode the blocks in that chunk to Color
        if (isBlockFormat(image.pixelFormat)) {
            // then decode any blocks to rgba8u, not dealing with HDR formats yet
            uint64_t mipLength = image.mipLevels[mipNumber].length;

            if (image.isSupercompressed()) {
                const uint8_t* srcData = image.fileData + image.mipLevels[mipNumber].offset;

                mipData.resize(mipLength * numChunks);
                uint8_t* dstData = mipData.data();
                if (!image.unpackLevel(mipNumber, srcData, dstData)) {
                    // KLOGF("kramv %s failed to unpack mip\n", filename);
                    return false;
                }

                // now extract the chunk for the thumbnail out of that level
                if (numChunks > 1) {
                    macroUnusedVar(chunkNum);
                    assert(chunkNum == 0);

                    // this just truncate to chunk 0 instead of copying chunkNum first
                    mipData.resize(mipLength);
                }
            }
            else {
                // this just truncate to chunk 0 instead of copying chunkNum first
                mipData.resize(mipLength);

                const uint8_t* srcData = image.fileData + image.mipLevels[mipNumber].offset;

                memcpy(mipData.data(), srcData, mipLength);
            }

            KramDecoder decoder;
            KramDecoderParams params;
            params.decoder = decoderType;

            // TODO: should honor swizzle in the ktx image
            // TODO: probaby need an snorm rgba format to convert the snorm versions, so they're not all red
            // if sdf, will be signed format and that will stay red

            switch (image.pixelFormat) {
                // To avoid showing single channel content in red, replicate to rgb
                case MyMTLPixelFormatBC4_RUnorm:
                case MyMTLPixelFormatEAC_R11Unorm:
                    params.swizzleText = "rrr1";
                    break;

                default:
                    break;
            }

            vector<uint8_t> dstMipData;

            // only space for one chunk for now
            dstMipData.resize(h * w * sizeof(Color));

            // want to just decode one chunk of the level that was unpacked abovve
            if (!decoder.decodeBlocks(w, h, mipData.data(), (int32_t)mipData.size(), image.pixelFormat, dstMipData, params)) {
                // Can't return NSError
                // error = KLOGF("kramv %s failed to decode blocks\n", filename);
                return false;
            }

            // copy over original encoded data
            mipData = dstMipData;
        }
        else if (isExplicitFormat(image.pixelFormat)) {
            // explicit formats like r/rg/rgb and 16f/32F need to be converted to rgba8 here
            // this should currently clamp, but could do range tonemap, see Image::convertToFourChannel()
            // but this needs to be slightly different.  This will decompress mip again

            Image image2D;
            if (!image2D.loadThumbnailFromKTX(image, mipNumber)) {
                // KLOGF("kramv %s failed to convert image to 4 channels\n", filename);
                return false;
            }

            // copy from Color back to uint8_t
            uint32_t mipSize = h * w * sizeof(Color);
            mipData.resize(mipSize);
            memcpy(mipData.data(), image2D.pixels().data(), mipSize);
        }
        

    
        //---------------------
        
        // create a bitmap, and allocate memory for the pixels
        BITMAPINFO bmi = {};
        bmi.bmiHeader.biSize = sizeof(bmi.bmiHeader);
        bmi.bmiHeader.biWidth = static_cast<LONG>(w);
        bmi.bmiHeader.biHeight = -static_cast<LONG>(h);  // -h to be top-down
        bmi.bmiHeader.biPlanes = 1;
        bmi.bmiHeader.biBitCount = 32;
        bmi.bmiHeader.biCompression = BI_RGB;  // TODO: use BI_PNG to shrink thumbnails

        Color* dstPixels = nullptr;
        HBITMAP hbmp = CreateDIBSection(nullptr, &bmi, DIB_RGB_COLORS, reinterpret_cast<void**>(&dstPixels), nullptr, 0);
        if (!hbmp) {
            return false;
        }

        // TODO: super tiny icons like 2x2 or 4x4 look terrible.  Windows says it never upsamples textures
        // but a 2x2 thumbnail inside a 32x32 thumbnail isn't visible.  Apple does the right thing and upsamples.

        // copy into bgra image (swizzle b and r).
        const Color* srcPixels = (const Color*)mipData.data();
        // copy pixels over and swap RGBA -> BGRA
        const uint32_t numPixels = w * h;
        for (uint32_t i = 0; i < numPixels; ++i) {
            // TODO: use uint32_t to do component swizzle
            dstPixels[i].r = srcPixels[i].b;
            dstPixels[i].g = srcPixels[i].g;
            dstPixels[i].b = srcPixels[i].r;

            // setting to 1 for premul is equivalent of blend to opaque black
            dstPixels[i].a = 255;
             
            if (!isPremul) {
                uint32_t alpha = srcPixels[i].a;
                if (alpha < 255) {
                    dstPixels[i].r = (dstPixels[i].r * alpha) / 255;
                    dstPixels[i].g = (dstPixels[i].g * alpha) / 255;
                    dstPixels[i].b = (dstPixels[i].b * alpha) / 255;
                }
            }
        }

        *phbmp = hbmp;
        return true;
    }

private:
    std::atomic_long    mReferences;
    ComPtr<IStream>     mStream;     // provided during initialization.
};

HRESULT KramThumbProvider_CreateInstance(REFIID riid, void** ppv) {
    KramThumbProvider* provider = new (std::nothrow) KramThumbProvider();
    HRESULT hr = provider ? S_OK : E_OUTOFMEMORY;
    if (SUCCEEDED(hr)) {
        hr = provider->QueryInterface(riid, ppv);
        provider->Release();
    }
    return hr;
}
