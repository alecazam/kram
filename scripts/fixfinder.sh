#!/bin/zsh

set -x

# When dropping a new build of the app into /Applications, finder still runs an old cached copy
# this resets launch servies to run the new code.  Why is this even necessary?

# reset launch services that hold the cached app 
/System/Library/Frameworks/CoreServices.framework/Frameworks/LaunchServices.framework/Support/lsregister -kill -r -domain local -domain system -domain user

# reset finder, so that double clicking on ktx/png files launches the new app
killall Finder
