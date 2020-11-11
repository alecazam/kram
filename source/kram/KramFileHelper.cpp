// kram - Copyright 2020 by Alec Miller. - MIT License
// The license and copyright notice shall be included
// in all copies or substantial portions of the Software.

#include "KramFileHelper.h"

// here's how to mmmap data, but NSData may have another way
#include <stdio.h>
#include <sys/stat.h>

// These were for mkstemps
#if KRAM_MAC || KRAM_LINUX
#include <sys/mman.h>
#include <unistd.h>
#endif

// Windows doesn't implement mkstemps.
// The whole C tmpfile setup is so messy.
// Need both a tmp file and a filename to move it.
// https://www.di-mgt.com.au/c_function_to_create_temp_file.html
#define USE_TMPFILEPLUS 1
#if USE_TMPFILEPLUS
#include "tmpfileplus/tmpfileplus.h"
#endif

namespace kram {
using namespace std;

FileHelper::~FileHelper() { close(); }

// no current extension
bool FileHelper::openTemporaryFile(const char* suffix, const char* access)
{
    close();

    // temp file api in POSIX is way too convoluted
    // might want to update to std::filesystem that has possibly abstracted this
    // it's likely the C code doesn't translate to Windows, and std::filesystem
    // has a move operation not found in C calls that can also create intermediate dirs.

#if USE_TMPFILEPLUS
    (void)access;

    char* pathname = nullptr;

    // Don't want unlink happening in tmpfileplus or rename fails.
    // But do wan't temporary flag set on Windows by setting keep = 0
    // since unlink doesn't work there.
    int keep = 1;
#if KRAM_WIN
    keep = 0;
#endif
    
    // Note: can't pass access either, always opened as rw
    _fp = tmpfileplus(nullptr, "tmp", suffix, &pathname, keep);
    if (!_fp) {
        return false;
    }

    _filename = pathname;
    free(pathname);

#else
    // this string will be modified
    _filename = "/tmp/kramimage-XXXXXX";
    // docs state filename must end with XXXXXX or rename won't occur
    _filename += suffix;

    // file opened with 600 permission (rw only for current user)
    // 666 was a security violation.
    int fd = mkstemps((char*)_filename.data(), strlen(suffix));
    if (fd == -1) {
        _filename.clear();
        return false;
    }

    // so temp file is auto-deleted on close of _fp if do unlink
    // but unlink prevents rename, since filename is not valid ?
    // so risk leaking temp file if app is killed in between open and renameFile.

    //unlink(_filename.c_str());

    // grab the fileptr from the file descriptor
    _fp = fdopen(fd, access);

    if (!_fp) {
        // unlink won't occur in close, so do it here
        unlink(_filename.c_str());
        _filename.clear();

        return false;
    }
#endif

    _isTmpFile = true;

    return true;
}

bool FileHelper::renameFile(const char* dstFilename)
{
    if (!_fp) return false;
    if (_filename.empty()) return false;

#if KRAM_WIN
    fclose(_fp);

    // windows doesn't remove any existing file, so have to do it explicitly
    remove(dstFilename);

    // now do the rename
    bool success = (rename(_filename.c_str(), dstFilename) == 0);

    // so that close doesn't do another fclose()
    _fp = nullptr;
    _isTmpFile = false;
    _filename.clear();

#else
    // since we're not closing, need to flush output
    fflush(_fp);

    // somehow even though the file isn't closed, can rename it
    // if an open temp file is closed, then the following fails since the fd/fp are released.

    // this only works if tmp and filename are on same volume
    // and must have an actual filename to call this, which tmpfile() doesn't supply
    // this removes any old file present
    bool success = (rename(_filename.c_str(), dstFilename) == 0);
#endif
    // Note: this doesn't change _filename to dstFilename

    return success;
}

bool FileHelper::open(const char* filename, const char* access)
{
    close();

    _fp = fopen(filename, access);
    if (!_fp) {
        return false;
    }

    _filename = filename;
    return true;
}

void FileHelper::close()
{
    if (_fp) {
        if (_isTmpFile) {
// tmpfileplus on windows opens file as temporary, since unlink doesn't work
#if !KRAM_WIN
            // so temp file is auto-deleted on close of _fp
            // if this is done in open, then fd is unlinked from name needed for rename...
            unlink(_filename.c_str());
#endif
            _filename.clear();
            _isTmpFile = false;
        }

        fclose(_fp);
        _fp = nullptr;
    }
}

int FileHelper::size() const
{
    int fd = fileno(_fp);

    struct stat stats;
    if (fstat(fd, &stats) < 0) {
        return -1;
    }
    return (int)stats.st_size;
}

}  // namespace kram
