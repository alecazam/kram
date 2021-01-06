#!/bin/zsh

# when dropping a new build of the app into /Applications, finder still runs an old cached copy
# this resets launch servies to run the new code.  Why is this even necessary?

/System/Library/Frameworks/CoreServices.framework/Frameworks/LaunchServices.framework/Support/lsregister -kill -r -domain local -domain system -domain user

killall Finder
