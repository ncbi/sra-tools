# Installation using pre-compiled binaries

!!! info end "Pre-compiled binaries"
    The SRA Toolkit provides **64-bit** binary installations for the Ubuntu and Alma Linux distributions, for Mac OS X, and for Windows.

=== "Ubuntu"

    ``` sh
    wget --output-document sratoolkit.tar.gz https://ftp-trace.ncbi.nlm.nih.gov/sra/sdk/current/sratoolkit.current-ubuntu64.tar.gz
    tar -vxzf sratoolkit.tar.gz
    cd sratoolkit.*
    export PATH="$PWD/bin:$PATH"
    ```

=== "AlmaLinux"

    ``` sh
    wget --output-document sratoolkit.tar.gz https://ftp-trace.ncbi.nlm.nih.gov/sra/sdk/current/sratoolkit.current-alma_linux64.tar.gz
    tar -vxzf sratoolkit.tar.gz
    cd sratoolkit.*
    export PATH="$PWD/bin:$PATH"
    ```

=== "Mac OS X"

    ``` sh
    curl --output sratoolkit.tar.gz https://ftp-trace.ncbi.nlm.nih.gov/sra/sdk/current/sratoolkit.current-mac64.tar.gz
    tar -vxzf sratoolkit.tar.gz
    cd sratoolkit.*
    export PATH="$PWD/bin:$PATH"
    ```

=== "Windows"

    1. Download [sratoolkit.current-win64.zip](https://ftp-trace.ncbi.nlm.nih.gov/sra/sdk/current/sratoolkit.current-win64.zip)
    2. Extract it to your desktop, for example
    3. Open a command shell, for example Start/Run `cmd.exe`
    4. `cd` to the directory you extracted the zip file to, for example `Desktop`
    5. `cd bin`


??? info "Build from source"

    To build from source, you need one of the supported operating systems (Linux, Windows, macOS) and CMake (minimum version 3.16). On Linux and macOS you need the GNU C/C++ toolchain (On macOS, you may also use CMake to generate an Xcode project), on Windows a set of MS Build Tools for Visual Studio 2017 or 2019.

    As a prerequisite, you need a set of libraries and headers from the NCBI SRA SDK (https://github.com/ncbi/ncbi-vdb). The easiest way to build the toolkit from the sources is to checkout the SDK and the toolkit side-by-side (ncbi-vdb/ and sra-tools/ under a common parent directory) and run configure/make first in ncbi-vdb/ and then in sra-tools/, as described below.

    === "Linux/macOS (gmake)"

        0. Checkout and build the SDK. For instructions, see README.md in https://github.com/ncbi/ncbi-vdb

        1. In the root of the sra-tools checkout, run:

        ```
        ./configure
        ```

        Use ```./configure -h``` for the list of available options

        If the SDK is checked out side-by-side with the toolkit (i.e. ncbi-vdb/ and sra-tools/ share a common parent directory), the configuration step will locate it and set the internal variables to point to its headers and libraries. Otherwise, you would need to specify their location using the configuration option --with-ncbi-vdb-prefix=```<path-to-sdk>```
        If you do not specify the --with-ncbi-vdb-prefix and there is no ncbi-vdb/ checked out side-by-side with sra-tools, the configuration script will look for a copy of the SDK installed into the system location, /usr/local/ncbi/ncbi-vdb.


        2. Once the configuration script has successfully finished, run:

        ```
        make
        ```

        This will invoke a Makefile that performs the following sequence:
        * retrieve all the settings saved by the configuration script
        * pass the settings to CMake
        * if this is the first time CMake is invoked, it will generate a project tree. The project tree will be located underneath the directory specified in the ```--build-prefix``` option of the configuration. The location can be displayed by running ```make config``` or ```make help``` inside the source tree.
        * build the CMake-generated project

        Running ```make``` from any directory inside the source tree will invoke the same sequence but limit the scope of the build to the sub-tree with the current directory as the root.

        The ```make``` command inside the source tree supports several additional targets; run ```make help``` for the list and short descriptions.

    === "macOS (Xcode)"

        To generate an Xcode project, you will need to first checkout and build ncbi-vdb, then check out sra-tools and run the standard CMake out-of-source build. For that, run CMake GUI and point it at the checkout directory. Click "Configure", then edit the entries for `VDB_BINDIR` and `VDB_INCDIR` to contain the paths for the CMake build directory for ncbi-vdb and the `interfaces` directory in the ncbi-vdb source directory, respectively. Choose "Xcode" as the generator and click "Configure" and then "Generate". Once the CMake generation succeeds, there will be an Xcode project file `sra-tools.xcodeproj` in the build's binary directory. You can open it with Xcode and build from the IDE.

        Alternatively, you can configure and build from the command line, in which case you would need to provide the 2 paths to the SDK's libraries and headers:

        ```
        cmake <path-to-sra-tools> -G Xcode -DVDB_BINDIR=<path-to-sdk-build> -DVDB_INCDIR=<path-to-sdk-headers>
        cmake --build . --config Debug      # or Release
        ```

    === "Windows (Visual Studio)"

        To generate an MS Visual Studio solution, you will need to first checkout and build ncbi-vdb, then check out sra-tools and run the standard CMake out-of-source build. For that, run CMake GUI and point it at the checkout directory. Click "Configure", then edit the entries for `VDB_BINDIR` and `VDB_INCDIR` to contain the paths for the CMake build directory for ncbi-vdb and the `interfaces` directory in the ncbi-vdb source directory, respectively. Now, choose one of the supported Visual Studio generators (see NOTE below) and a 64-bit generator, and click "Configure" and then "Generate". Once the CMake generation succeeds, there will be an MS VS solution file ```sra-tools.sln``` in the build's binary directory. You can open it with the Visual Studio and build from the IDE.

        NOTE: This release supports generators ```Visual Studio 15 2017``` and ```Visual Studio 16 2019```, only for 64 bit platforms.


        Alternatively, you can configure and build from the command line (assuming the correct MSVS Build Tools are in the %PATH%), in which case you would need to provide the 2 paths to the SDK's libraries and headers:

        ```
        cmake <path-to-sra-tools> -G "Visual Studio 16 2019" -A x64 -DVDB_BINDIR=<path-to-sdk-build> -DVDB_INCDIR=<path-to-sdk-headers>
        cmake --build . --config Debug      # or Release
        ```

# Run a quick test

<!-- termynal -->
```sh
$ fasterq-dump SRR000001
spots read      : 470,985
reads read      : 1,883,940
reads written   : 707,026
reads 0-length  : 468,635
technical reads : 708,279
```
