#include "KramLib.h"

int main(int argc, char* argv[])
{
    int errorCode = kram::kramAppMain(argc, argv);

    // returning -1 from main results in exit code of 255, so fix this to return 1 on failure.
    if (errorCode != 0) {
        exit(1);
    }

    return 0;
}
