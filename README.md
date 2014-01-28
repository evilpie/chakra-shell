# Chakra Shell

This shell is able to run the Octane benchmark. It supports one argument, which is the file to run and
has three additional host functions available to JS: `load`, `print` and `read`.

# How to run

description.html has some information on how to build the shell. You will need the Windows SDK 8.1, Visual Studio C++
and Internet Explorer 11. I got it working with Visual Studio C++ 2012, following
these [steps](http://blogs.msdn.com/b/vcblog/archive/2012/03/25/10287354.aspx) and
changing all occurrences from `Windows Kits\8.0\` to `Windows Kits\8.1\` .

# License

The code is heavily based on the "[JavaScript Runtime Hosting Sample](http://code.msdn.microsoft.com/windowsdesktop/JavaScript-Runtime-Hosting-d3a13880)" by Paul Vick, but modified by me. The code is licensed under Apache License, Version 2.0.