# Haiku WebKit #

This repository contains the Haiku WebKit port source code.
For more information, please visit the [project's wiki and issue tracker](http://dev.haiku-os.org/)

## Quick build instructions ##

### Cloning the repository ###

This repository is *huge* (about 5 gigabytes). If you are only interested in building
the latest version of WebKit, remember to use the --depth option to git clone.
This can be used to download only a limited part of the history and will reduce the
checkout to about 600MB.

### Requirements ###

- A recent version of Haiku (beta4 or later)
- The GCC11 development tools
- The dependencies as listed below
- At least about 3G of RAM
- Preferably a fast computer!

Dependencies can be installed (for a gcc2hybrid version) via:

    $ pkgman install cmake_x86 curl_x86_devel gcc_x86 gperf haiku_x86_devel icu74_x86_devel \
    libavif_x86_devel libjpeg_turbo_x86_devel libjxl_x86_devel sqlite_x86_devel libpng16_x86_devel \
    libxml2_x86_devel libxslt_x86_devel libexecinfo_x86_devel libwebp_x86_devel \
    libpsl_x86_devel libidn2_x86_devel libunistring_x86_devel lcms_x86_devel ninja_x86 \
    pkgconfig_x86 perl python3.10 ruby_x86 woff2_x86_devel

(for other versions, remove the _x86 part from all package names)

Additionally if you want to run the tests:

    $ pkgman install lighttpd_x86
    $ gem install webrick

For the other versions of Haiku, it should be similar but remove all the _x86 suffixes from the
package names.

##### NOTE:

If you get an _Ruby missing error_ even after you have installed ruby, similar to <br>
`Could NOT find Ruby  (missing: RUBY_INCLUDE_DIR RUBY_LIBRARY RUBY_CONFIG_INCLUDE_DIR)  (found suitable version "2.2.0", minimum required is "1.9")`, you can skip that.

### Building WebKit ###

#### Configure using Curl ####

Curl is available as an alternative network backend. This can be useful for comparing performance
and bugs with the libnetservices based implementation.

If you want to use curl as a network backend, make sure the needed libraries are installed:

    $ pkgman install devel:libcurl

Edit Source/cmake/OptionsHaiku.cmake and turn USE\_CURL ON (near the bottom of the file). Then
build as usual following the instructions below, the resulting build will use CURL.

Note that some things are currently disabled:

- No support for showing certificate errors in the UI
- No support for showing icons in BNotification
- Possibly a few other problems

#### Configuring your build for the first time ####
Commands to run from the webkit checkout directory:

On a gcc2hybrid (32bit) Haiku:

    $ PKG_CONFIG_LIBDIR=/boot/system/develop/lib/x86/pkgconfig \
        CC=gcc-x86 CXX=g++-x86 Tools/Scripts/build-webkit \
        --cmakeargs="-DCMAKE_AR=/bin/ar-x86 -DCMAKE_RANLIB=/bin/ranlib-x86 -DCMAKE_CXX_FLAGS='-ftrack-macro-expansion=0 --param ggc-min-expand=10'" --haiku \
        --no-fatal-warnings

On other versions:

    $ Tools/Scripts/build-webkit --haiku --no-fatal-warnings

#### Regular build, once configured ####

    $ cd WebKitBuild/Release
    $ ninja

This will build a release version of WebKit libraries on a quad core cpu.

On a successful build, executables and libraries are generated in the WebKitBuild/Release directory.

##### NOTE:
If you are getting "out of memory" error despite having sufficient memory, disable ASLR:
	$ export DISABLE_ASLR=1

### Advanced Build, other targets ###

The following make targets are available:

- libwtf.so - WebKit Template Framework (a complement of the STL used in WebKit)
- libjavascriptcore.so - The JavaScript interpreter
- jsc - The JavaScript executable shell
- libwebcore.so - The WebCore library (cross-platform WebKit code)
- libwebkitlegacy.so - The Haiku specific parts of WebKit
- HaikuLauncher - A simple browsing test app
- DumpRenderTree - The tree parsing test tool

Example given, this will build the JavaScriptCore library:

    $ ninja libjavascriptcore.so

In some rare cases the build system can be confused, to be sure that everything gets rebuilt from scratch,
you can remove the WebKitBuild/ directory and start over.

There are several cmake variables available to configure the build in various ways.
These can be given to build-webkit using the --cmakeargs option, or changed later on
using "cmake -Dvar=value WebKitBuild/Release".

### Speeding up the build with llvm lld ###

If the llvm17_lld package is installed, WebKit will use lld for linking instead of GNU ld.
This results in a much faster linking step, sometimes saving several minutes off the build time.

### Speeding up the build with distcc ###

You can set the compiler while calling the configure script:
    $ CC="distcc gcc-x86" CXX="distcc g++-x86" build-webkit ...

It is a good idea to set the NUMBER\_OF\_PROCESSORS environment variable as well
(this will be given to cmake through the -j option). If you don't set it, only
the local CPUs will be counted, leading to a sub-optimal distcc distribution.

distcc will look for a compiler named gcc-x86 and g++-x86. You'll need to adjust
the path on the slaves to get that pointing to the gcc11 version (the gcc11 compiler
is already visible under this name on the local machine and haiku slaves).
CMake usually tries to resolve the compiler to an absolute path on the first
time it is called, but this doesn't work when the compiler is called through
distcc.

## Testing ##

### Testing the test framework ###
    $ ruby Tools/Scripts/test-webkitruby
    $ perl Tools/Scripts/test-webkitperl
    $ python3.10 Tools/Scripts/test-webkitpy

The ruby tests pass (all 2 of them!)
The perl test almost pass: Failed 1/27 test programs. 1/482 subtests failed.
The python tests hit some errors related to file locking, tracked in #13795, as
well as some other issues.

### JSC ###
    $ perl Tools/Scripts/run-javascriptcore-tests

This will recompile jsc as well as some additional test tools. Currently this doesn't work on
32bit systems, because it tries to build testb3 which is 64bit only.

Current results:
- 9258 tests are run (some are excluded because of missing features in our Ruby port)
- 10 failures related to parsing dates and trigonometry:

    mozilla-tests.yaml/ecma_3/Date/15.9.5.6.js.mozilla
    mozilla-tests.yaml/ecma_3/Date/15.9.5.6.js.mozilla-baseline
    mozilla-tests.yaml/ecma_3/Date/15.9.5.6.js.mozilla-dfg-eager-no-cjit-validate-phases
    mozilla-tests.yaml/ecma_3/Date/15.9.5.6.js.mozilla-llint
    stress/ftl-arithcos.js.always-trigger-copy-phase
    stress/ftl-arithcos.js.default
    stress/ftl-arithcos.js.dfg-eager
    stress/ftl-arithcos.js.dfg-eager-no-cjit-validate
    stress/ftl-arithcos.js.no-cjit-validate-phases
    stress/ftl-arithcos.js.no-llint


### WebKit ###
You will have to install the Ahem font for layout tests to work properly. This
is a font with known-size glyphs that render the same on all platforms. Most of
the characters look like black squares, this is expected and not a bug!
http://www.w3.org/Style/CSS/Test/Fonts/Ahem/

$ pkgman install ahem

It is also a good idea to enable automated debug reports for DumpRenderTree.
Create the file ~/config/settings/system/debug\_server/settings and add:

    executable_actions {
        DumpRenderTree log
    }

The crash reports will be moved from the Desktop to the test result directory
and renamed to the name of the test that triggered the crash. If you don't do
this, you have to manually click the "save report" button, and while the
testsuite waits on that, it may mark one or several tests as "timed out".

WebKit also needs an HTTP server for some of the tests, with CGI support for
PHP, Perl, and a few others. You can use the --no-http option to
run-webkit-tests to skip this part. Otherwise you need to install lighttpd
and PHP (both available in HaikuPorts package depot).

Finally, the tests are a mix of html and xhtml files. The file:// loader in
Haiku relies on MIME sniffing to tell them apart. This is not completely
reliable, so for some tests the type needs to be forced (unfortunately this
can't be stored in the git repo):

    $ sh Tools/haiku/mimefix.sh

You can then run the testsuite:

    $ python3.10 Tools/Scripts/run-webkit-tests --platform=haiku --dump-render-tree --no-build \
        --no-retry-failures --clobber-old-results --no-new-test-results

The options will prevent the script to try updating DumpRenderTree (it doesn't
know how to do that on Haiku, yet). It doesn't retry failed tests, will remove
previous results before starting, and will not generate missing "expected" files
in the LayoutTests directory.

A lot of tests are currently failing. The problems are either in the WebKit
code itself, or in the various parts of the test harness, none of which are
actually complete: DumpRenderTree, webkitpy, etc. Some of them are triggering
asserts in WebKit code.

You can run the tests manually using either DumpRenderTree or HaikuLauncher
(both accept an URL from the command line). For tests that require the page to
be served over http (and not directly read from a file), you need an HTTP server.
Install the lighttpd package and run:

    Tools/Scripts/new-run-webkit-httpd --server start

This will start lighttpd with the appropriate setting file, allowing you to run
the tests. The server listens on port 8000 by default.

### WebKit2 ###

Same as above, but:

    $ python Tools/Scripts/run-webkit-tests --platform=haiku-wk2 --no-build \
        --no-retry-failures --clobber-old-results --no-new-test-results

This is currently broken.

### Others ###

There are more tests, but the build-\* scripts must be working before we can run them.

## Status of WebKit2 port ##

The Haiku port currently uses the WebKitLegacy API. Eventually we should move to
WebKit2 (simply called WebKit in the sources). WebKit2 splits the web engine into
multiple processes: an user interface, a web process, a network process, etc. This
allows for better sandboxing, and better stability (the user interface will not
crash or freeze when it hits a problematic website).

The work on WebKit2 is found in the GSoC2019 tag. It has not been updated since
and the internals of WebKit have changed a bit. An attempt to rebase it is found
in the haiku-webkit2 branch, but it's broken. This branch is a bit more than a rebase,
there is also a lot of cleanup of the commit history, and some obsolete changes were removed.
It is a good starting point to get WebKit2 running on Haiku, but a lot of things will need
to be added or debugged. It is possible to build the MiniBrowser test browser, but inter-process
communications are not working, and so, it is not possible to even start loading a page at the
moment. There is some ongoing work to restore this to a working state.

### Logging ###

To facilitate debugging with multiple processes, logging is done using the DevConsole tool. This
allows clearly tagging each log with the originating process (especially useful for WebKit2 where
there are multiple processes).

Logging can be controlled using the WEBKIT_DEBUG environment variable. The default is to have
all logs disabled. You can enable everythin by setting the variable to "all", or enable specific
debug sources. Most logs require some compile time options as well.

## Notes ##

cmake is smart enough to detect when a variable has changed and will rebuild everything.
You can keep several generated folders with different settings if you need to switch
between them very often (eg. debug and release builds). Just invoke the build-webkit
script with different settings and different output dirs. You can then run make 
(or ninja) from each of these folders.

You can copy a WebPositive binary from your Haiku installation into the
WebKitBuild/Release folder. Launching it will then use the freshly built
libraries instead of the system ones. It is a good idea to test this because
HaikuLauncher doesn't use tabs, which sometimes expose different bugs.

This document was last updated August 13, 2020.

Authors: Maxime Simon, Alexandre Deckner, Adrien Destugues
