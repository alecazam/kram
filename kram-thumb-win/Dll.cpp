// based on QOI Thumbnail Provider for Windows Explorer
// Written by iOrange in 2021
//
// Based on Microsoft's example
// https://github.com/microsoft/windows-classic-samples/tree/main/Samples/Win7Samples/winui/shell/appshellintegration/RecipeThumbnailProvider
//
// Also more info here:
// https://docs.microsoft.com/en-us/previous-versions/windows/desktop/legacy/cc144118(v=vs.85)

#include <objbase.h>
#include <shlobj.h> // For SHChangeNotify
#include <shlwapi.h>
#include <thumbcache.h> // For IThumbnailProvider.

#include <atomic>
#include <new>
#include <vector> // For std::size

// from KramThumbProvider.cpp
extern HRESULT KramThumbProvider_CreateInstance(REFIID riid, void** ppv);

#define SZ_KramTHUMBHANDLER L"Kram Thumbnail Handler"

// TODO: update CLSID here, is this a fixed id since Win7 vs. Vista said a provider is different ids?
// keepd in sync with kCLSID
// Just a different way of expressing CLSID in the two values below
// made with uuidgen.exe
#define SZ_CLSID_KramTHUMBHANDLER L"{a9a47ef5-c238-42a9-a4e6-a85558811dac}"
constexpr CLSID kCLSID_KramThumbHandler = {0xa9a47ef5, 0xc238, 0x42a9, {0xa4, 0xe6, 0xa8, 0x55, 0x58, 0x81, 0x1d, 0xac}};

typedef HRESULT (*PFNCREATEINSTANCE)(REFIID riid, void** ppvObject);
struct CLASS_OBJECT_INIT {
    const CLSID* pClsid;
    PFNCREATEINSTANCE pfnCreate;
};

// add classes supported by this module here
constexpr CLASS_OBJECT_INIT kClassObjectInit[] = {
    {&kCLSID_KramThumbHandler, KramThumbProvider_CreateInstance}};

std::atomic_long gModuleReferences(0);
HINSTANCE gModuleInstance = nullptr;

// Standard DLL functions
STDAPI_(BOOL)
DllMain(HINSTANCE hInstance, DWORD dwReason, void*)
{
    if (DLL_PROCESS_ATTACH == dwReason) {
        gModuleInstance = hInstance;
        ::DisableThreadLibraryCalls(hInstance);
    }
    else if (DLL_PROCESS_DETACH == dwReason) {
        gModuleInstance = nullptr;
    }
    return TRUE;
}

STDAPI DllCanUnloadNow()
{
    // Only allow the DLL to be unloaded after all outstanding references have been released
    return (gModuleReferences > 0) ? S_FALSE : S_OK;
}

void DllAddRef()
{
    ++gModuleReferences;
}

void DllRelease()
{
    --gModuleReferences;
}

class CClassFactory : public IClassFactory {
public:
    static HRESULT CreateInstance(REFCLSID clsid, const CLASS_OBJECT_INIT* pClassObjectInits, size_t cClassObjectInits, REFIID riid, void** ppv)
    {
        *ppv = NULL;
        HRESULT hr = CLASS_E_CLASSNOTAVAILABLE;
        for (size_t i = 0; i < cClassObjectInits; ++i) {
            if (clsid == *pClassObjectInits[i].pClsid) {
                IClassFactory* pClassFactory = new (std::nothrow) CClassFactory(pClassObjectInits[i].pfnCreate);
                hr = pClassFactory ? S_OK : E_OUTOFMEMORY;
                if (SUCCEEDED(hr)) {
                    hr = pClassFactory->QueryInterface(riid, ppv);
                    pClassFactory->Release();
                }
                break; // match found
            }
        }
        return hr;
    }

    CClassFactory(PFNCREATEINSTANCE pfnCreate)
        : mReferences(1), mCreateFunc(pfnCreate)
    {
        DllAddRef();
    }

    virtual ~CClassFactory()
    {
        DllRelease();
    }

    // IUnknown
    IFACEMETHODIMP QueryInterface(REFIID riid, void** ppv)
    {
        static const QITAB qit[] = {
            QITABENT(CClassFactory, IClassFactory),
            {0}};
        return QISearch(this, qit, riid, ppv);
    }

    IFACEMETHODIMP_(ULONG)
    AddRef()
    {
        return ++mReferences;
    }

    IFACEMETHODIMP_(ULONG)
    Release()
    {
        const long refs = --mReferences;
        if (!refs) {
            delete this;
        }
        return refs;
    }

    // IClassFactory
    IFACEMETHODIMP CreateInstance(IUnknown* punkOuter, REFIID riid, void** ppv)
    {
        return punkOuter ? CLASS_E_NOAGGREGATION : mCreateFunc(riid, ppv);
    }

    IFACEMETHODIMP LockServer(BOOL fLock)
    {
        if (fLock) {
            DllAddRef();
        }
        else {
            DllRelease();
        }
        return S_OK;
    }

private:
    std::atomic_long mReferences;
    PFNCREATEINSTANCE mCreateFunc;
};

STDAPI DllGetClassObject(REFCLSID clsid, REFIID riid, void** ppv)
{
    return CClassFactory::CreateInstance(clsid, kClassObjectInit, std::size(kClassObjectInit), riid, ppv);
}

// A struct to hold the information required for a registry entry
struct REGISTRY_ENTRY {
    HKEY hkeyRoot;
    PCWSTR pszKeyName;
    PCWSTR pszValueName;
    PCWSTR pszData;
};

// Creates a registry key (if needed) and sets the default value of the key
HRESULT CreateRegKeyAndSetValue(const REGISTRY_ENTRY* pRegistryEntry)
{
    HKEY hKey;
    HRESULT hr = HRESULT_FROM_WIN32(RegCreateKeyExW(pRegistryEntry->hkeyRoot,
                                                    pRegistryEntry->pszKeyName,
                                                    0, nullptr, REG_OPTION_NON_VOLATILE,
                                                    KEY_SET_VALUE | KEY_WOW64_64KEY,
                                                    nullptr, &hKey, nullptr));
    if (SUCCEEDED(hr)) {
        hr = HRESULT_FROM_WIN32(RegSetValueExW(hKey, pRegistryEntry->pszValueName, 0, REG_SZ,
                                               reinterpret_cast<const BYTE*>(pRegistryEntry->pszData),
                                               static_cast<DWORD>(wcslen(pRegistryEntry->pszData) + 1) * sizeof(WCHAR)));
        RegCloseKey(hKey);
    }
    return hr;
}

// Registers this COM server
STDAPI DllRegisterServer()
{
    HRESULT hr;
    WCHAR szModuleName[MAX_PATH] = {0};

    if (!GetModuleFileNameW(gModuleInstance, szModuleName, ARRAYSIZE(szModuleName))) {
        hr = HRESULT_FROM_WIN32(GetLastError());
    }
    else {
        // List of registry entries we want to create
        const REGISTRY_ENTRY registryEntries[] = {
            // RootKey          KeyName                                                                      ValueName          Data
            {HKEY_CURRENT_USER, L"Software\\Classes\\CLSID\\" SZ_CLSID_KramTHUMBHANDLER, nullptr, SZ_KramTHUMBHANDLER},
            {HKEY_CURRENT_USER, L"Software\\Classes\\CLSID\\" SZ_CLSID_KramTHUMBHANDLER L"\\InProcServer32", nullptr, szModuleName},
            {HKEY_CURRENT_USER, L"Software\\Classes\\CLSID\\" SZ_CLSID_KramTHUMBHANDLER L"\\InProcServer32", L"ThreadingModel", L"Apartment"},

            // libkram can decode any of these and create a thumbnail
            // The Vista GUID for the thumbnail handler Shell extension is E357FCCD-A995-4576-B01F-234630154E96.
            {HKEY_CURRENT_USER, L"Software\\Classes\\.ktx", L"PerceivedType", L"image"},
            {HKEY_CURRENT_USER, L"Software\\Classes\\.ktx\\ShellEx\\{e357fccd-a995-4576-b01f-234630154e96}", nullptr, SZ_CLSID_KramTHUMBHANDLER},
            {HKEY_CURRENT_USER, L"Software\\Classes\\.ktx2", L"PerceivedType", L"image"},
            {HKEY_CURRENT_USER, L"Software\\Classes\\.ktx2\\ShellEx\\{e357fccd-a995-4576-b01f-234630154e96}", nullptr, SZ_CLSID_KramTHUMBHANDLER},
            {HKEY_CURRENT_USER, L"Software\\Classes\\.dds", L"PerceivedType", L"image"},
            {HKEY_CURRENT_USER, L"Software\\Classes\\.dds\\ShellEx\\{e357fccd-a995-4576-b01f-234630154e96}", nullptr, SZ_CLSID_KramTHUMBHANDLER},
            //{HKEY_CURRENT_USER, L"Software\\Classes\\.png",                                                  L"PerceivedType", L"image"},
            //{HKEY_CURRENT_USER, L"Software\\Classes\\.png\\ShellEx\\{e357fccd-a995-4576-b01f-234630154e96}", nullptr, SZ_CLSID_KramTHUMBHANDLER},
        };

        hr = S_OK;
        for (size_t i = 0; i < std::size(registryEntries) && SUCCEEDED(hr); ++i) {
            hr = CreateRegKeyAndSetValue(&registryEntries[i]);
        }
    }

    if (SUCCEEDED(hr)) {
        // This tells the shell to invalidate the thumbnail cache.  This is important because any .qoi files
        // viewed before registering this handler would otherwise show cached blank thumbnails.
        SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
    }

    return hr;
}

// Unregisters this COM server
STDAPI DllUnregisterServer()
{
    HRESULT hr = S_OK;

    const PCWSTR regKeys[] = {
        L"Software\\Classes\\CLSID\\" SZ_CLSID_KramTHUMBHANDLER,
        L"Software\\Classes\\.ktx",
        L"Software\\Classes\\.ktx2",
        L"Software\\Classes\\.dds",
        // L"Software\\Classes\\.png", // only need this if Win png bg is bad
    };

    // Delete the registry entries
    for (size_t i = 0; i < std::size(regKeys) && SUCCEEDED(hr); ++i) {
        hr = HRESULT_FROM_WIN32(RegDeleteTreeW(HKEY_CURRENT_USER, regKeys[i]));
        if (hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND)) {
            // If the registry entry has already been deleted, say S_OK.
            hr = S_OK;
        }
    }

    return hr;
}
