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

use strict;

require 'package.pm';
require 'os-arch.pm';

use Cwd qw (abs_path getcwd);
use File::Basename 'fileparse';
use File::Spec 'catdir';
use FindBin qw($Bin);
use Getopt::Long 'GetOptions';

sub println { print @_; print "\n"; }

check();

my %PKG = PKG();

my $PACKAGE_NAME = PACKAGE_NAME();
my $OUT_MAKEFILE = 'Makefile.config';

my $PACKAGE = PACKAGE();

my $HOME = $ENV{HOME} || $ENV{USERPROFILE}
    || $ENV{LOGDIR} || getcwd || (getpwuid($<))[7] || abs_path('.');

$PKG{UPATH} =~ s/(\$\w+)/$1/eeg;

my $OUTDIR = File::Spec->catdir($HOME, $PKG{OUT});

my $package_default_prefix = $PKG{PATH};
my $schema_default_dir = $PKG{SCHEMA_PATH} if ($PKG{SCHEMA_PATH});

my @REQ = REQ();

my @options = ( "arch=s",
                "clean",
                "debug",
                "help",
                "outputdir=s",
#               "output-makefile=s",
                "prefix=s",
                "status",
                "with-debug",
                "without-debug" );
foreach my $href (@REQ) {
    my %a = %$href;
    push @options, "$a{option}=s";
    push @options, "$a{boption}=s" if ($a{boption});
    $href->{usrpath} =~ s/(\$\w+)/$1/eeg;
}
push @options, "shemadir" if ($PKG{SCHEMA_PATH});

my %OPT;
die "configure: error" unless (GetOptions(\%OPT, @options));

if ($OPT{'help'}) {
    help();
    exit(0);
} elsif ($OPT{'clean'}) {
    {
        foreach (glob(CONFIG_OUT() . '/Makefile.config*'),
            File::Spec->catdir(CONFIG_OUT(), 'user.status'),
            File::Spec->catdir(CONFIG_OUT(), 'Makefile.userconfig'))
        {
            my $f = $_;
            print "removing $f... ";
            if (-e $f) {
                if (unlink $f) {
                    println "ok";
                } else {
                    println "failed";
                }
            } else {
                println "not found";
            }
        }
    }
    if (CONFIG_OUT() ne '.') {
        foreach
            (glob('Makefile.config*'), 'user.status', 'Makefile.userconfig')
        {
            my $f = $_;
            print "removing $f... ";
            if (-e $f) {
                if (unlink $f) {
                    println "ok";
                } else {
                    println "failed";
                }
            } else {
                println "not found";
            }
        }
    }
    exit(0);
} elsif ($OPT{'status'}) {
    status(1);
    exit(0);
}
my ($filename, $directories, $suffix) = fileparse($0);
die "configure: error: $filename should be run as ./$filename"
    if ($directories ne "./");

$OPT{'prefix'} = $package_default_prefix unless ($OPT{'prefix'});

my $AUTORUN = $OPT{status};
print "checking system type... " unless ($AUTORUN);
my ($OS, $ARCH, $OSTYPE, $MARCH, @ARCHITECTURES) = OsArch();
println $OSTYPE unless ($AUTORUN);

{
    my $prefix = $OPT{'prefix'};
    $OPT{eprefix} = $prefix unless ($OPT{eprefix} || $OS eq 'win');
    my $eprefix = $OPT{eprefix};
    unless ($OPT{bindir} || $OS eq 'win') {
        $OPT{bindir} = File::Spec->catdir($eprefix, 'bin') ;
    }
    unless ($OPT{libdir} || $OS eq 'win') {
        $OPT{libdir} = File::Spec->catdir($eprefix, 'lib');
    }
    unless ($OPT{includedir} || $OS eq 'win') {
        $OPT{includedir} = File::Spec->catdir($eprefix, 'include');
    }
    if ($PKG{LNG} eq 'PYTHON' && ! $OPT{pythondir} && $OS ne 'win') {
        $OPT{pythondir} = $eprefix;
    }
    if ($PKG{LNG} eq 'JAVA' && ! $OPT{javadir} && $OS ne 'win') {
        $OPT{javadir} = $eprefix;
    }
    if ($PKG{EXAMP} && ! $OPT{sharedir} && $OS ne 'win') {
        $OPT{sharedir} = File::Spec->catdir($eprefix, 'share');
    }
}

if (0 && $AUTORUN) {
    while (1) {
        open F,  File::Spec->catdir(CONFIG_OUT(), 'user.status') or last;
        foreach (<F>) {
            chomp;
            @_ = split /=/;
            if ($#_ == 1) {
                $OPT{$_[0]} = $_[1] unless ($OPT{$_[0]});
            }
        }
        last;
    }
}

# initial values
my $TARGDIR = File::Spec->catdir($OUTDIR, $PACKAGE);
$TARGDIR = expand($OPT{'outputdir'}) if ($OPT{'outputdir'});

# $OUT_MAKEFILE = $OPT{'output-makefile'} if ($OPT{'output-makefile'});

my $BUILD = "rel";

# parse command line
$BUILD = $OPT{'BUILD'} if ($OPT{'BUILD'});
$BUILD = "dbg" if ($OPT{'with-debug'});
$BUILD = "rel" if ($OPT{'without-debug'});

my $BUILD_TYPE = "release";
$BUILD_TYPE = "debug" if ( $BUILD eq "dbg" );

#println unless ($AUTORUN);

if ($OPT{arch}) {
    my $found;
    foreach (@ARCHITECTURES) {
        if ($_ eq $OPT{arch}) {
            ++$found;
            last;
        }
    }
    if ($found) {
        $ARCH = $MARCH = $OPT{arch};
    } else {
        delete $OPT{arch};
    }
}

$OUT_MAKEFILE .= ".$OS.$ARCH";
$OUT_MAKEFILE = File::Spec->catdir(CONFIG_OUT(), $OUT_MAKEFILE);

#my $OSTYPE = `uname -s`; chomp $OSTYPE;

print "checking machine architecture... " unless ($AUTORUN);
#my $MARCH = `uname -m`; chomp $MARCH;
println $MARCH unless ($AUTORUN);

my $TOOLS = "";
$TOOLS = "jdk" if ($PKG{LNG} eq 'JAVA');

print "checking " . PACKAGE_NAME() . " version... " unless ($AUTORUN);
my $FULL_VERSION = VERSION();
println $FULL_VERSION unless ($AUTORUN);

# determine architecture

print "checking for supported architecture... " unless ($AUTORUN);

my $BITS;

if ( $MARCH =~ m/x86_64/i )
{
    $BITS = 64;
}
elsif ( $MARCH =~ m/i?86/i )
{
    $BITS = 32;
}
else
{
    die "unrecognized Architecture - " . $ARCH;
}
println "$MARCH ($BITS bits) is supported" unless ($AUTORUN);

# determine OS and related norms
my ($LPFX, $OBJX, $LOBX, $LIBX, $SHLX, $EXEX, $OSINC);

print "checking for supported OS... " unless ($AUTORUN);
if ( $OSTYPE =~ m/linux/i )
{
    $LPFX = "lib";
    $OBJX = "o";
    $LOBX = "pic.o";
    $LIBX = "a";
    $SHLX = "so";
    $EXEX = "";
    $OSINC = "unix";
    if ( $TOOLS eq "" )
    {
        $TOOLS = "gcc";
    }
}
elsif ( $OSTYPE =~ m/darwin/i )
{
    $LPFX = "lib";
    $OBJX = "o";
    $LOBX = "pic.o";
    $LIBX = "a";
    $SHLX = "dylib";
    $EXEX = "";
    $OSINC = "unix";
    if ( $TOOLS eq "" )
    {
        $TOOLS = "clang";
    }
} elsif ($OSTYPE eq 'win') {
    $TOOLS = "vc++";
} else
{
    die "unrecognized OS - " . $OSTYPE;
}
println "$OSTYPE ($OS) is supported" unless ($AUTORUN);

# tool chain
my ($CC, $CP, $AR, $ARX, $ARLS, $LD, $LP);
my ($JAVAC, $JAVAH, $JAR);
my ($DBG, $OPT, $PIC, $INC, $MD);

print "checking for supported tool chain... " unless ($AUTORUN);
if ( $TOOLS =~ m/gcc/i )
{
    $CC = "gcc -c";
    $CP = "g++ -c";
    $AR = "ar rc";
    $ARX = "ar x";
    $ARLS = "ar t";
    $LD = "gcc";
    $LP = "g++";

    $DBG = "-g -DDEBUG";
    $OPT = "-O3";
    $PIC = "-fPIC";
    $INC = "-I";
    $MD  = "-MD";
}
elsif ( $TOOLS =~ m/clang/i )
{
    $CC = "clang -c";
    $CP = "clang++ -c";
    $AR = "ar rc";
    $ARX = "ar x";
    $ARLS = "ar t";
    $LD = "clang";
    $LP = "clang++";

    $DBG = "-g -DDEBUG";
    $OPT = "-O3";
    $PIC = "-fPIC";
    $INC = "-I";
    $MD  = "-MD";
}
elsif ( $TOOLS =~ m/jdk/i )
{
    $JAVAC = "javac";
    $JAVAH = "javah";
    $JAR   = "jar cf";

    $DBG = "-g";
} elsif ($TOOLS eq 'vc++') {
} else
{
    die "unrecognized tool chain - " . $TOOLS;
}
println "$TOOLS tool chain is supported" unless ($AUTORUN);

if ($OS ne 'win') {
    $TARGDIR = File::Spec->catdir($TARGDIR, $OS, $TOOLS, $ARCH, $BUILD);
}

my @dependencies;

foreach my $href (@REQ) {
    $href->{bldpath} =~ s/(\$\w+)/$1/eeg;
    my ($found_itf, $found_lib, $found_ilib);
    my %a = %$href;
    my $is_optional = $a{type} eq 'SI';
    my $need_source = $a{type} =~ /S/;
    my $need_build = $a{type} =~ /B/;
    my $need_include = $a{type} =~ /I/;
    my $need_lib = $a{type} =~ /L/;
    ++$need_include if ($need_lib);
    my $need_ilib;
    $need_ilib = $a{ilib} if ($need_build);
    unless ($AUTORUN) {
        if ($need_source && $need_build) {
       println "checking for $a{name} package source files and build results..."
        } elsif ($need_source) {
            println "checking for $a{name} package source files...";
        } else {
            println "checking for $a{name} package...";
        }
    }
    my $i = expand($OPT{$a{option}});
    my $l = $i;
    my $has_i_option = $i;
    my $has_b_option = $a{boption} && $OPT{$a{option}};
    if ($has_i_option) {
        if ($need_lib) {
            ($found_lib, $found_ilib, $found_itf)
                = find_lib_in_dir($l, $a{lib}, $need_ilib, $a{include});
            $i = $found_itf if ($found_itf);
            $l = $found_lib if ($found_lib);
        } else {
            $found_itf = find_include_in_dir($l, $a{include});
            $i = $found_itf if ($found_itf);
        }
    }
    if ($has_b_option) {
        ($found_lib, $found_ilib) = find_lib_in_dir($l, $a{lib}, $need_ilib);
        $l = $found_lib if ($found_lib);
    }

    if (! $has_i_option && ! $found_itf && ($need_include || $need_source)) {
        my $try = $a{srcpath};
        $found_itf = find_include_in_dir($a{srcpath}, $a{include});
        $i = $found_itf if ($found_itf);
    }
    if (! $has_i_option && ! $found_itf || (! $found_lib && $need_lib)) {
        my $try = $a{pkgpath};
        if ($need_lib) {
            ($found_lib, $found_ilib, $found_itf)
                = find_lib_in_dir($try, $a{lib}, $need_ilib, $a{include});
        } else {
            $found_itf = find_include_in_dir($try, $a{include});
        }
        if ($found_itf) {
            $i = $found_itf;
        }
        if ($found_lib) {
            $l = $found_lib;
        }
    }
    if (! $has_i_option && ! $found_itf || (! $found_lib && $need_lib)) {
        my $try = $a{usrpath};
        if ($need_lib) {
            ($found_lib, $found_ilib, $found_itf)
                = find_lib_in_dir($try, $a{lib}, $need_ilib, $a{include});
        } else {
            $found_itf = find_include_in_dir($try, $a{include});
        }
        if ($found_itf) {
            $i = $found_itf;
        }
        if ($found_lib) {
            $l = $found_lib;
        }
    }
    if (! $has_b_option && ! $found_lib && ($need_build ||$need_lib)) {
        my $try = $a{bldpath};
        ($found_lib, $found_ilib) = find_lib_in_dir($try, $a{lib}, $need_ilib);
        if ($found_lib) {
            $l = $found_lib;
        }
    }

    if (! $i || (! $found_lib && $need_lib) || ($need_ilib && ! $found_ilib)) {
        if ($is_optional) {
            println "configure: optional $a{name} package not found: skipped.";
        } else {
            if ($OPT{'debug'}) {
                $_ = "$a{name}: includes: ";
                unless ($need_include) {
                    $_ .= "not needed";
                } else {
                    $_ .= $i;
                }
                unless ($need_lib) {
                    $_ .= "; libs: not needed";
                } else {
                    $_ .= "; libs: " . $found_lib;
                }
                unless ($need_ilib) {
                    $_ .= "; ilibs: not needed";
                } else {
                    $_ .= "; ilibs: " . $found_ilib;
                }
                println "\t\t$_";
            }
            println "configure: error: required $a{name} package not found.";
            exit 1;
        }
    } else {
        $i = abs_path($i);
        push(@dependencies, "$a{namew}_INCDIR = $i");
        if ($found_lib && $l) {
            $l = abs_path($l);
            push(@dependencies, "$a{namew}_LIBDIR = $l");
        }
        if ($need_ilib && $found_ilib) {
            $found_ilib = abs_path($found_ilib);
            push(@dependencies, "$a{namew}_ILIBDIR = $found_ilib");
        }
    }
}

my @c_arch;

if ($OS ne 'win') {
    # create Makefile.config
    push (@c_arch, "### AUTO-GENERATED FILE ###" );
    push (@c_arch,  "" );
    push (@c_arch,  "" );
    push (@c_arch,  'OS_ARCH = $(shell perl $(TOP)/os-arch.perl)' );

    push (@c_arch,  "# build type");
    push (@c_arch,  "BUILD = $BUILD");
    push (@c_arch,  "" );
    if ($OPT{'outputdir'}) {
        push (@c_arch,  "# output path");
        push (@c_arch,  "TARGDIR = $TARGDIR/$PACKAGE");
        push (@c_arch,  "" );
    }
    push (@c_arch, "# install paths");
    push (@c_arch, "INST_BINDIR = $OPT{bindir}") if ($OPT{bindir});
    push (@c_arch, "INST_LIBDIR = $OPT{libdir}") if ($OPT{libdir});
    push (@c_arch, "INST_INCDIR = $OPT{includedir}") if ($OPT{includedir});
    push (@c_arch, "INST_SCHEMADIR = $OPT{'shemadir'}") if ($OPT{'shemadir'});
    push (@c_arch, "INST_SHAREDIR = $OPT{'sharedir'}") if ($OPT{'sharedir'});
    push (@c_arch, "INST_JAVADIR = $OPT{'javadir'}") if ($OPT{'javadir'});
    push (@c_arch, "INST_PYTHONDIR = $OPT{'pythondir'}") if ($OPT{'pythondir'});
    push (@c_arch, "");

#   push (@c_arch,  'include $(TOP)/Makefile.userconfig' );
#   push (@c_arch,  'include $(TOP)/Makefile.config.$(OS_ARCH)' );
    push (@c_arch,  "" );
    push (@c_arch, "# build type");
    push (@c_arch, "BUILD ?= $BUILD");
    push (@c_arch,  "" );
    push (@c_arch,  "# target OS" );
    push (@c_arch,  "OS = " . $OS );
    push (@c_arch,  "OSINC = " . $OSINC );
    push (@c_arch,  "" );
    push (@c_arch,  "# prefix string for system libraries" );
    push (@c_arch,  "LPFX = " . $LPFX );
    push (@c_arch,  "" );

    push (@c_arch,  "# suffix strings for system libraries" );
    push (@c_arch,  "LIBX = $LIBX" );
    push (@c_arch,  "VERSION_LIBX = \$(LIBX).\$(VERSION)" );
    push (@c_arch,  "MAJMIN_LIBX = \$(LIBX).\$(MAJMIN)" );
    push (@c_arch,  "MAJVERS_LIBX = \$(LIBX).\$(MAJVERS)" );
    push (@c_arch,  "" );
    
    push (@c_arch,  "SHLX = $SHLX" );
    push (@c_arch,  "VERSION_SHLX = \$(SHLX).\$(VERSION)" );
    push (@c_arch,  "MAJMIN_SHLX = \$(SHLX).\$(MAJMIN)" );
    push (@c_arch,  "MAJVERS_SHLX = \$(SHLX).\$(MAJVERS)" );
    push (@c_arch,  "" );

    push (@c_arch,  "# suffix strings for system object files" );
    push (@c_arch,  "OBJX = $OBJX" );
    push (@c_arch,  "LOBX = $LOBX" );
    push (@c_arch,  "" );

    push (@c_arch,  "# suffix string for system executable" );
    push (@c_arch,  "EXEX = $EXEX" );
    push (@c_arch,  "VERSION_EXEX = \$(EXEX).\$(VERSION)" );
    push (@c_arch,  "MAJMIN_EXEX = \$(EXEX).\$(MAJMIN)" );
    push (@c_arch,  "MAJVERS_EXEX = \$(EXEX).\$(MAJVERS)" );
    push (@c_arch,  "" );

    push (@c_arch,  "# system architecture and wordsize" );
    if ( $ARCH eq $MARCH ) {
        push (@c_arch,  "ARCH = " . $ARCH );
    } else {
        push (@c_arch,  "ARCH = " . $ARCH . " # ( " . $MARCH . " )" );
    }
    push (@c_arch,  "BITS = " . $BITS );
    push (@c_arch,  "" );

    push (@c_arch,  "# tools" );
    push (@c_arch,  "# tools" );
    push (@c_arch,  "CC   = " . $CC ) if ($CC);
    push (@c_arch,  "CP   = " . $CP ) if ($CP);
    push (@c_arch,  "AR   = " . $AR ) if ($AR);
    push (@c_arch,  "ARX  = " . $ARX ) if ($ARX);
    push (@c_arch,  "ARLS = " . $ARLS ) if ($ARLS);
    push (@c_arch,  "LD   = " . $LD ) if ($LD);
    push (@c_arch,  "LP   = " . $LP ) if ($LP);
    push (@c_arch,  "JAVAC  = " . $JAVAC ) if ($JAVAC);
    push (@c_arch,  "JAVAH  = " . $JAVAH ) if ($JAVAH);
    push (@c_arch,  "JAR  = " . $JAR ) if ($JAR);
    push (@c_arch,  "" );
    push (@c_arch,  "" );

    push (@c_arch,  "# tool options" );
    push (@c_arch,  "# tool options" );
    if ( $BUILD eq "dbg" ) {
        push (@c_arch,  "DBG     = $DBG" );
        push (@c_arch,  "OPT     = ");
    } else {
        push (@c_arch,  "DBG     = -DNDEBUG" ) if ($PKG{LNG} eq 'C');
        push (@c_arch,  "OPT     = $OPT" ) if ($OPT);
    }
    push (@c_arch,  "PIC     = $PIC" ) if ($PIC);
    if ($PKG{LNG} eq 'C') {
        push (@c_arch,
            "SONAME  = -Wl,-soname=\$(subst \$(VERSION),\$(MAJVERS),\$(\@F))");
        push (@c_arch,  "SRCINC  = $INC. $INC\$(SRCDIR)" );
    } elsif ($PKG{LNG} eq 'JAVA') {
        push (@c_arch,  "SRCINC  = -sourcepath \$(INCPATHS)" );
    }
    push (@c_arch,  "INCDIRS = \$(SRCINC) $INC\$(TOP)" ) if ($PIC);
    if ($PKG{LNG} eq 'C') {
        push (@c_arch,  "CFLAGS  = \$(DBG) \$(OPT) \$(INCDIRS) $MD" );
    }
    push (@c_arch,  "CLSPATH = -classpath \$(CLSDIR)" );
    push (@c_arch,  "" );
    push (@c_arch,  "" );

    # version information

    my ($VERSION, $MAJMIN, $MAJVERS);

    if ($FULL_VERSION =~ /(\d+)\.(\d+)\.(\d+)-?\w*\d*/) {
        $VERSION = "$1.$2.$3";
        $MAJMIN = "$1.$2";
        $MAJVERS = $1;
    } else {
        die $VERSION;
    }

    push (@c_arch,  "# NGS API and library version" );
    push (@c_arch,  "VERSION = " . $VERSION );
    push (@c_arch,  "MAJMIN = " . $MAJMIN );
    push (@c_arch,  "MAJVERS = " . $MAJVERS );
    push (@c_arch,  "" );

    # determine output path
    if ($PKG{LNG} eq 'C') {
    #    $TARGDIR = $TARGDIR . "/" . $ARCH;
    }
    push (@c_arch,  "# output path" );
    push (@c_arch,  "TARGDIR ?= " . $TARGDIR );
    push (@c_arch,  "" );

    # determine include install path
    # determine library install path

    # other things
    push (@c_arch,  "# derived paths" );
    push (@c_arch,  "MODPATH  ?= \$(subst \$(TOP)/,,\$(CURDIR))" );
    push (@c_arch,  "SRCDIR   ?= \$(TOP)/\$(MODPATH)" );
    push (@c_arch,  "MAKEFILE ?= \$(abspath \$(firstword \$(MAKEFILE_LIST)))" );
    push (@c_arch,  "BINDIR    = \$(TARGDIR)/bin" );
    if ($PKG{LNG} eq 'C') {
        push (@c_arch,  "LIBDIR    = \$(TARGDIR)/lib" );
    } elsif ($PKG{LNG} eq 'JAVA') {
        push (@c_arch,  "LIBDIR    = \$(TARGDIR)/jar" );
    }
    push (@c_arch,  "ILIBDIR   = \$(TARGDIR)/ilib" );
    push (@c_arch,  "OBJDIR    = \$(TARGDIR)/obj/\$(MODPATH)" );
    push (@c_arch,  "CLSDIR    = \$(TARGDIR)/cls" );

    if ($PKG{LNG} eq 'JAVA') {
        push (@c_arch, "INCPATHS = "
            . "\$(SRCDIR):\$(SRCDIR)/itf:\$(TOP)/gov/nih/nlm/ncbi/ngs" );
    }

    push (@c_arch,  "" );

    push (@c_arch,  "# exports" );
    push (@c_arch,  "export TOP" );
    push (@c_arch,  "export MODPATH" );
    push (@c_arch,  "export SRCDIR" );
    push (@c_arch,  "export MAKEFILE" );
    push (@c_arch,  "" );

    push (@c_arch,  "# auto-compilation rules" );
    if ($PKG{LNG} eq 'C') {
        push (@c_arch,  "\$(OBJDIR)/%.\$(OBJX): %.c" );
        push (@c_arch,  "\t\$(CC) -o \$@ \$< \$(CFLAGS)" );
        push (@c_arch,  "\$(OBJDIR)/%.\$(LOBX): %.c" );
        push (@c_arch,  "\t\$(CC) -o \$@ \$< \$(PIC) \$(CFLAGS)" );
    }
    push (@c_arch,  "\$(OBJDIR)/%.\$(OBJX): %.cpp" );
    push (@c_arch,  "\t\$(CP) -o \$@ \$< \$(CFLAGS)" );
    push (@c_arch,  "\$(OBJDIR)/%.\$(LOBX): %.cpp" );
    push (@c_arch,  "\t\$(CP) -o \$@ \$< \$(PIC) \$(CFLAGS)" );
    push (@c_arch,  "" );

    # this is part of Makefile
    push (@c_arch,  "VPATH = \$(SRCDIR)" );
    push (@c_arch,  "" );

    # we know how to find jni headers
    if ($PKG{LNG} eq 'JAVA' and $OPT{'with-ngs-sdk-src'}) {
        push(@c_arch, "JNIPATH = $OPT{'with-ngs-sdk-src'}/language/java");
    }

    push (@c_arch,  "# directory rules" );
    if ($PKG{LNG} eq 'C') {
        push (@c_arch,  "\$(BINDIR) \$(LIBDIR) \$(ILIBDIR) \$(OBJDIR):\n"
                     . "\tmkdir -p \$@" );
    } elsif ($PKG{LNG} eq 'JAVA') {
        # test if we have jni header path
        push (@c_arch,  "\$(LIBDIR) \$(CLSDIR):\n\tmkdir -p \$@" );
    }
    push (@c_arch,  "" );

    push (@c_arch,  "# not real targets" );
    push (@c_arch,  ".PHONY: default clean install all std \$(TARGETS)" );
    push (@c_arch,  "" );

    push (@c_arch,  "# dependencies" );
    if ($PKG{LNG} eq 'C') {
        push (@c_arch,  "include \$(wildcard \$(OBJDIR)/*.d)" );
    } elsif ($PKG{LNG} eq 'JAVA') {
        push (@c_arch,  "include \$(wildcard \$(CLSDIR)/*.d)" );
    }
    push (@c_arch, @dependencies);
    push (@c_arch,  "" );

    push (@c_arch, '# installation rules');
    push (@c_arch,
        '$(INST_LIBDIR)$(BITS)/%.$(VERSION_LIBX): $(LIBDIR)/%.$(VERSION_LIBX)');
    push (@c_arch, '');
    push (@c_arch, '');
    push (@c_arch, '');
    push (@c_arch, '');
    push (@c_arch, '');
    push (@c_arch, '');
    push (@c_arch, '');
    push (@c_arch, '');
    push (@c_arch, '');
    push (@c_arch, '');
    push (@c_arch, '');
    push (@c_arch, '');
    push (@c_arch, '');
    push (@c_arch, '');
    push (@c_arch, '');
    push (@c_arch, '');
    push (@c_arch, '');
    push (@c_arch, '');
    push (@c_arch, '');
    push (@c_arch, '');
    push (@c_arch, '');
    push (@c_arch, '');
    push (@c_arch, '');
    push (@c_arch, '');
    push (@c_arch, '');
    push (@c_arch, '');
    push (@c_arch, '');
    push (@c_arch, '');
    push (@c_arch, '');
    push (@c_arch, '');
    push (@c_arch, '');
    push (@c_arch, '');
    push (@c_arch, '');
    push (@c_arch, '');
    push (@c_arch, '');
    push (@c_arch, '');
    push (@c_arch, '');
    push (@c_arch, '');
    push (@c_arch, '');
    push (@c_arch, '');
    push (@c_arch, '');
    push (@c_arch, '');
    push (@c_arch, '');
    push (@c_arch, '');
}

if (! $OPT{'status'} ) {
    if ($OS eq 'win') {
        my $OUT = File::Spec->catdir(CONFIG_OUT(), 'Makefile.config.win');
        println "configure: creating '$OUT'";
        open OUT, ">$OUT" or die "cannot open $OUT to write";
        my $name = PACKAGE_NAMW();
        my $outdir = $name . '_OUTDIR';
        my $root = $name . '_ROOT';

        print OUT <<EndText;
<Project xmlns="http://schemas.microsoft.com/developer/msbuild/2003">  
  <PropertyGroup Label="Globals">
    <$outdir>$TARGDIR/\</$outdir>
EndText
        foreach my $href (@REQ) {
            my %a = %$href;
            my $NGS_SDK_PREFIX = '';
            $NGS_SDK_PREFIX = $a{found_itf} if ($a{found_itf});
            if ($a{name} eq 'ngs-sdk') {
                my $root = "$a{namew}_ROOT";
                print OUT "    <$root>$NGS_SDK_PREFIX\/</$root>\n";
                last;
            }
        }
        print OUT <<EndText;
    <$root>$Bin/\</$root>
  </PropertyGroup>
</Project>
EndText
        close OUT;
    } else {
        println "configure: creating 'Makefile.config'" unless ($AUTORUN);
        my $CONFIG_OUT = CONFIG_OUT();
        my $out = File::Spec->catdir($CONFIG_OUT, 'Makefile.config');
        open OUT, ">$out" or die "cannot open $out to write";
        print OUT "### AUTO-GENERATED FILE ###\n";
        print OUT "\n";
        print OUT "OS_ARCH = \$(shell perl \$(TOP)/os-arch.perl)\n";
        print OUT "include \$(TOP)/$CONFIG_OUT/Makefile.config.\$(OS_ARCH)\n";
        close OUT;

        println "configure: creating '$OUT_MAKEFILE'" unless ($AUTORUN);
        open OUT, ">$OUT_MAKEFILE" or die "cannot open $OUT_MAKEFILE to write";
        print OUT "$_\n" foreach (@c_arch);
        close OUT;
    }
}

if (0 && ! $AUTORUN) {
    my $OUT = File::Spec->catdir(CONFIG_OUT(), 'user.status');
    println "configure: creating '$OUT'";
    open OUT, ">$OUT" or die "cannot open $OUT to write";
    print OUT "arch=$OPT{arch}\n" if ($OPT{arch});
    print OUT "BUILD=$BUILD\n";
    print OUT "bindir=$OPT{bindir}\n" if ($OPT{bindir});
    print OUT "libdir=$OPT{libdir}\n" if ($OPT{libdir});
    print OUT "includedir=$OPT{includedir}\n" if ($OPT{includedir});
    print OUT "sharedir=$OPT{sharedir}\n" if ($OPT{sharedir});
    print OUT "javadir=$OPT{javadir}\n" if ($OPT{javadir});
    print OUT "pythondir=$OPT{pythondir}\n" if ($OPT{pythondir});
    foreach my $href (@REQ) {
        my %a = %$href;
        print OUT "$a{option}=$OPT{$a{option}}\n" if ($OPT{$a{option}});
        if ($a{boption} && $OPT{$a{boption}}) {
            print OUT "$a{boption}=$OPT{$a{boption}}\n";
        }
    }
    close OUT;
}

unless (1 || $AUTORUN || $OS eq 'win') {
    my $OUT = File::Spec->catdir(CONFIG_OUT(), 'Makefile.userconfig');
    println "configure: creating '$OUT'";
    open OUT, ">$OUT" or die "cannot open $OUT to write";
    print OUT "### AUTO-GENERATED FILE ###\n\n";

#   print OUT "# build type\n" . "BUILD = $BUILD\n\n";
    if (0 && $OPT{'outputdir'}) {
        print OUT "# output path\n"
                . "TARGDIR = $TARGDIR/$PACKAGE\n"
                . "\n";
    }
    print OUT "# required packages\n";
#   print OUT "NGS_SDK_PREFIX = $NGS_SDK_PREFIX\n" if ($NGS_SDK_PREFIX);
    print OUT "\n";
    print OUT "# install paths\n";
    print OUT "INST_BINDIR = $OPT{bindir}\n" if ($OPT{bindir});
    print OUT "INST_LIBDIR = $OPT{libdir}\n" if ($OPT{libdir});
    print OUT "INST_INCDIR = $OPT{includedir}\n" if ($OPT{includedir});
    print OUT "INST_SCHEMADIR = $OPT{'shemadir'}" if ($OPT{'shemadir'});
    print OUT "INST_SHAREDIR = $OPT{'sharedir'}" if ($OPT{'sharedir'});
    print OUT "INST_JAVADIR = $OPT{'javadir'}" if ($OPT{'javadir'});
    print OUT "INST_PYTHONDIR = $OPT{'pythondir'}" if ($OPT{'pythondir'});
    print OUT "\n";
    close OUT;
}

status() if ($OS ne 'win');

sub status {
    my ($load) = @_;
    if ($load) {
        ($OS, $ARCH, $OSTYPE, $MARCH, @ARCHITECTURES) = OsArch();
        my $MAKEFILE
            = File::Spec->catdir(CONFIG_OUT(), "$OUT_MAKEFILE.$OS.$ARCH");
        println "\t\tloading $MAKEFILE" if ($OPT{'debug'});
        unless (-e $MAKEFILE) {
            print STDERR "configure: error: run ./configure [OPTIONS] first.\n";
            exit 1;
        }
        open F, $MAKEFILE or die "cannot open $MAKEFILE";
        foreach (<F>) {
            chomp;
            if (/BUILD = (.+)/) {
                $BUILD_TYPE = $1;
            } elsif (/BUILD \?= /) {
                $BUILD_TYPE = $_ unless ($BUILD_TYPE);
            }
            elsif (/TARGDIR = /) {
                $TARGDIR = $_;
                println "\t\tgot $_" if ($OPT{'debug'});
            } elsif (/TARGDIR \?= (.+)/) {
                $TARGDIR = $1 unless ($TARGDIR);
                println "\t\tgot $_" if ($OPT{'debug'});
            }
            elsif (/INST_INCDIR = (.+)/) {
                $OPT{includedir} = $1;
            }
            elsif (/INST_BINDIR = (.+)/) {
                $OPT{bindir} = $1;
            }
            elsif (/INST_LIBDIR = (.+)/) {
                $OPT{libdir} = $1;
            }
        }
    }

    println "build type: $BUILD_TYPE";
    println "build output path: $TARGDIR" if ($OS ne 'win');

#   print "prefix: ";    print $OPT{'prefix'} if ($OS ne 'win');    println;
#   print "eprefix: ";    print $OPT{'eprefix'} if ($OPT{'eprefix'});   println;

    print "includedir: ";
    print $OPT{'includedir'} if ($OPT{'includedir'});
    println;

    print "bindir: ";
    print $OPT{'bindir'} if ($OPT{'bindir'});
    println;

    print "libdir: ";
    print $OPT{'libdir'} if ($OPT{'libdir'});
    println;

    println "schemadir: $OPT{'shemadir'}" if ($OPT{'shemadir'});
    println "sharedir: $OPT{'sharedir'}" if ($OPT{'sharedir'});
    println "javadir: $OPT{'javadir'}" if ($OPT{'javadir'});
    println "pythondir: $OPT{'pythondir'}" if ($OPT{'pythondir'});

    println;
}

sub expand {
    my ($filename) = @_;
    return unless ($filename);
    if ($filename =~ /^~/) {
        $filename =~ s{ ^ ~ ( [^/]* ) }
                      { $1
                            ? (getpwnam($1))[7]
                            : ( $ENV{HOME} || $ENV{USERPROFILE} || $ENV{LOGDIR}
                                || (getpwuid($<))[7]
                              )
                      }ex;
    }
    $filename = abs_path($filename);
}

sub find_include_in_dir {
    my ($try, $include) = @_;
    print "\tinclude files in $try... " unless ($AUTORUN);
    unless (-d $try) {
        println 'no' unless ($AUTORUN);
        return;
    } else {
        if (-e "$try/$include") {
            println 'yes' unless ($AUTORUN);
            return $try;
        } elsif (-e "$try/interfaces/$include") {
            println 'yes' unless ($AUTORUN);
            return "$try/interfaces";
        } else {
            println 'no' unless ($AUTORUN);
            return;
        }
    }
}

sub find_lib_in_dir {
    my ($try, $lib, $ilib, $include) = @_;
    if ($include) {
        print "\t$try " unless ($AUTORUN);
    } else {
        print "\tlibrary files in $try... " unless ($AUTORUN);
    }
    unless (-d $try) {
        println 'no' unless ($AUTORUN);
        println "\t\tnot found $try" if ($OPT{'debug'});
        return;
    } else {
        println "\n\t\tfound $try" if ($OPT{'debug'});
        my $builddir = File::Spec->catdir($try, $OS, $TOOLS, $ARCH, $BUILD);
        my $libdir = File::Spec->catdir($builddir, 'lib');
        my $ilibdir = File::Spec->catdir($builddir, 'ilib');
        my $f = File::Spec->catdir($libdir, $lib);
        println "\t\tchecking $f" if ($OPT{'debug'});
        unless (-e $f) {
            $libdir = File::Spec->catdir($try, 'lib' . $BITS);
            undef $ilibdir;
            $f = File::Spec->catdir($libdir, $lib);
            println "\t\tchecking $f" if ($OPT{'debug'});
        } elsif ($ilib) {
            $f = File::Spec->catdir($ilibdir, $ilib);
            println "\t\tchecking $f" if ($OPT{'debug'});
            unless (-e $f) {
                println 'no' unless ($AUTORUN);
                return;
            }
        }
        unless (-e $f) {
            println 'no' unless ($AUTORUN);
            return;
        } elsif ($ilib && ! $ilibdir) {
            println "\n\t\tfound $f but no ilib/" if ($OPT{'debug'});
            println 'no' unless ($AUTORUN);
            return;
        }
        if (! $include) {
            println 'yes' unless ($AUTORUN);
            return ($libdir, $ilibdir);
        } elsif (-e "$try/$include") {
            println 'yes' unless ($AUTORUN);
            return ($libdir, $ilibdir, $try);
        } elsif (-e "$try/include/$include") {
            println 'yes' unless ($AUTORUN);
            return ($libdir, $ilibdir, "$try/include");
        } else {
            println 'no' unless ($AUTORUN);
            return;
        }
    }
}

################################################################################

sub check {
    die "No DEPENDS" unless defined &DEPENDS;

    die "No CONFIG_OUT"   unless CONFIG_OUT();
    die "No PACKAGE"      unless PACKAGE();
    die "No PACKAGE_NAME" unless PACKAGE_NAME();
    die "No PACKAGE_NAMW" unless PACKAGE_NAMW();
    die "No PACKAGE_TYPE" unless PACKAGE_TYPE();
    die "No VERSION"      unless VERSION();

    my %PKG = PKG();

    die "No LNG"   unless $PKG{LNG};
    die "No OUT"   unless $PKG{OUT};
    die "No PATH"  unless $PKG{PATH};
    die "No UPATH" unless $PKG{UPATH};

    foreach my $href (REQ()) {
        die "No name" unless $href->{name};

        die "No $href->{name}:bldpath" unless $href->{bldpath};
        die "No $href->{name}:ilib"    unless $href->{ilib};
        die "No $href->{name}:include" unless $href->{include};
        die "No $href->{name}:lib"     unless $href->{lib};
        die "No $href->{name}:namew"   unless $href->{namew};
        die "No $href->{name}:option"  unless $href->{option};
        die "No $href->{name}:pkgpath" unless $href->{pkgpath};
        die "No $href->{name}:srcpath" unless $href->{srcpath};
        die "No $href->{name}:type"    unless $href->{type};
        die "No $href->{name}:usrpath" unless $href->{usrpath};
    }
}

################################################################################

sub help {
#  --prefix=PREFIX         install architecture-independent files in PREFIX
    print <<EndText;
`configure' configures $PACKAGE_NAME to adapt to many kinds of systems.

Usage: ./configure [OPTION]...

Defaults for the options are specified in brackets.

Configuration:
  -h, --help              display this help and exit
EndText

    if ($^O ne 'MSWin32') {
        print <<EndText;
Installation directories:
  --prefix=PREFIX         install all files in PREFIX
                          [$package_default_prefix]

EndText

        my $other_prefix = $PKG{UPATH};
        if ($PACKAGE eq 'sra-tools') {
            print <<EndText;
  --shemadir=DIR          install schema files in DIR
                          [$schema_default_dir]

EndText
        }

        print "By default, \`make install' will install all the files in\n";
    
        if (PACKAGE_TYPE() eq 'B') {
            print "\`$package_default_prefix/bin', ";
        } else {
            print "\`$package_default_prefix/include', ";
        }
        println "\`$package_default_prefix/lib' etc.";

        print <<EndText;
You can specify an installation prefix other than \`$package_default_prefix'
using \`--prefix', for instance \`--prefix=$other_prefix'.
EndText
    }

    print <<EndText;

For better control, use the options below.

EndText

    my ($required, $optional);
    foreach my $href (@REQ) {
        if ($href->{type} eq 'SI') {
            ++$optional;
        } else {
            ++$required;
        }
    }

    if ($required) {
        print "Required Packages:\n";
        foreach my $href (@REQ) {
            next if ($href->{type} eq 'SI');
            my %a = %$href;
            if ($a{type} =~ /S/) {
                println "  --$a{option}=DIR    search for $a{name} package";
                println "                                 source files in DIR";
            } else {
            println "  --$a{option}=DIR      search for $a{name} package in DIR"
            }
            if ($a{boption}) {
                println "  --$a{boption}=DIR      search for $a{name} package";
                println "                                 build output in DIR";
            }
        }
        println;
    }
    
    if ($optional) {
        print "Optional Packages:\n";
        foreach my $href (@REQ) {
            next unless ($href->{type} eq 'SI');
            my %a = %$href;
            println "  --$a{option}=DIR    search for $a{name} package";
            println "                                source files in DIR";
        }
        println;
    }

    print <<EndText if ($^O ne 'MSWin32');
Build tuning:
  --with-debug
  --without-debug
  --arch=name             specify the name of the target architecture

  --outputdir=DIR         generate build output into DIR directory
                          [$OUTDIR]

EndText

    println "Miscellaneous:";
    if ($^O ne 'MSWin32') {
        println
            "  --status                print current configuration information"
    }
    println "  --clean                 remove all configuration results";
    println "  --debug                 print lots of debugging information";
    println;
}

=pod
################################################################################
=cut
