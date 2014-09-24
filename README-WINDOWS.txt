# ===========================================================================
#
#                            PUBLIC DOMAIN NOTICE
#               National Center for Biotechnology Information
#
#  This software/database is a "United States Government Work" under the
#  terms of the United States Copyright Act.  It was written as part of
#  the author's official duties as a United States Government employee and
#  thus cannot be copyrighted.  This software/database is freely available
#  to the public for use. The National Library of Medicine and the U.S.
#  Government have not placed any restriction on its use or reproduction.
#
#  Although all reasonable efforts have been taken to ensure the accuracy
#  and reliability of the software and data, the NLM and the U.S.
#  Government do not and cannot warrant the performance or results that
#  may be obtained by using this software or data. The NLM and the U.S.
#  Government disclaim all warranties, express or implied, including
#  warranties of performance, merchantability or fitness for any particular
#  purpose.
#
#  Please cite the author in any work or product based on this material.
#
# ===========================================================================


The NCBI SRA ( Sequence Read Archive ) SDK ( Software Development Kit )


Contact: sra-tools@ncbi.nlm.nih.gov


NOTICE:

  Effective as of release 2.1.8, NCBI is no longer supporting public sources for
  Windows builds. The sources will still contain everything that we use to build
  the binaries, but may not build in your environment.

  The reason for this change has to do with dependencies upon third party
  libraries, which are commonly installed on other platforms, but could be
  described as "hit and miss" on Windows. When the libraries DO exist, it is
  difficult to know if they have a correct or compatible calling convention.

  We will continue to distribute pre-built binaries for Windows. You are welcome
  to try your luck at compiling the sources yourself under Cygwin.


ENVIRONMENT:

  The Windows build uses the same makefiles as Linux and Mac, and has
  been tested under Cygwin. You need to execute Cygwin AFTER sourcing
  the Microsoft batch file from Visual Studio. We edit the
  "cygwin.bat" file to first source "vsvars32.bat":

      @echo off
      call "%VS80COMNTOOLS%\vsvars32.bat"
      C:
      chdir C:\cygwin\bin
      bash --login -i

  By including vsvars32.bat before launching Cygwin, your bash shell
  will have all of the Microsoft tools in its path, and the Microsoft
  tools will know how to find includes and libraries.

  There is a conflict between the Cygwin and Microsoft link tools. The
  GNU version (that "avoids the bells and whistles of the more
  commonly-used `ln' command" but otherwise duplicates it) is not used
  or needed by the build, while the Microsoft version IS needed. To
  address this issue, our linker scripts (build/ld.win.*.sh) reorder
  the PATH directories in an attempt to ensure that the correct tool
  is located. If this proves ineffective in your environment, try
  renaming the GNU tool, e.g.:

      $ mv /usr/bin/link.exe /usr/bin/gnu-link.exe


CYGWIN:

  While we are using Cygwin as a build environment, the binaries are
  NOT Cygwin binaries and do not link against any of their
  libraries. As a result, there is no dependency upon their runtime
  being present, but neither is there any of their emulation.

  In particular, paths behave very differently. Our SDK is based upon
  POSIX path conventions. Our Windows tools accept relative and
  absolute paths well, in DOS, Cygwin or MinGW POSIX-style form. But
  since we do not use Cygwin libraries, our tools have no idea of how
  to interpret "full" paths within the Cygwin Unix root, e.g.:

      $ cygpath -w /home
      C:\cygwin\home

  They CAN, however, interpret fully drive-letter qualified Cygwin
  paths:

      $ fastq-dump /cygdrive/C/cygwin/home/myname/SRRxxxxxx.sra

  Internally, this Cygwin path will be interpreted as

      C:\cygwin\home\myname\SRRxxxxxx.sra


WINDOWS PATHS IN GENERAL:

  Windows is the only supported platform that does not present a
  single-root unified file system, such as POSIX. In the POSIX model,
  navigation from any one point in the file system to any other is a
  matter of entering and exiting directories by name. Under Windows,
  each drive has its own file system, and network paths form yet
  another file system.

  Like Cygwin and MinGW and probably countless other projects, we have
  artificially bridged the drive letter file systems by introducing a
  virtual root node whose immediate sub-directories are the
  single-letter drive names that are currently mounted, e.g.:

      C:, D: are real drives, while Z: is a mounted network drive
      "/C" and "/D" and "/Z" are all subdirectories of "/"

      navigation from C:\cygwin\home to D:\data
      /C/cygwin/home/../../../D/data

  We have NOT bridged drive letters and network paths at this
  point. There is no way for us to get from C:\cygwin\home to
  \\server\sradata for example.

  Network paths are represented using POSIX-style slashes, but
  otherwise look much like their Windows counterparts:

      \\server\data
      //server/data

  Because network and drive paths cannot be connected, we recommend
  that you execute your tools completely within one space. If you are
  running your tools from local or network mounted drive letters, you
  should access data from local or network mounted drives. If you are
  using network paths during operation, you should run your binaries
  from a network path.

  Note that the Windows tool "cmd.exe" does not support cd'ing to a
  network path for similar reasons.

  Your tools will execute from "cmd.exe" or from bash under Cygwin
  since they are general Windows command line utilities.

