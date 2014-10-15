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

sub println  { print @_; print "\n" }

my ($filename, $directories, $suffix) = fileparse($0);
if ($directories ne "./") {
    println "configure: error: $filename should be run as ./$filename";
    exit 1;
}

require 'package.pm';
require 'os-arch.pm';

use Cwd qw (abs_path getcwd);
use File::Basename 'fileparse';
use File::Spec 'catdir';
use FindBin qw($Bin);
use Getopt::Long 'GetOptions';

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
                "build=s",
                "clean",
                "debug",
                "help",
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
$OPT{'prefix'} = $package_default_prefix unless ($OPT{'prefix'});

my $AUTORUN = $OPT{status};
print "checking system type... " unless ($AUTORUN);
my ($OS, $ARCH, $OSTYPE, $MARCH, @ARCHITECTURES) = OsArch();
println $OSTYPE unless ($AUTORUN);

{
    $OPT{'prefix'} = expand($OPT{'prefix'});
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
        $OPT{javadir} = File::Spec->catdir($eprefix, 'jar');
    }
    if ($PKG{EXAMP} && ! $OPT{sharedir} && $OS ne 'win') {
        $OPT{sharedir} = File::Spec->catdir($eprefix, 'share');
    }
}

# initial values
my $TARGDIR = File::Spec->catdir($OUTDIR, $PACKAGE);
$TARGDIR = expand($OPT{'build'}) if ($OPT{'build'});

my $BUILD = 'rel';

# parse command line
$BUILD = 'dbg' if ($OPT{'with-debug'});
$BUILD = 'rel' if ($OPT{'without-debug'});

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

print "checking $PACKAGE_NAME version... " unless ($AUTORUN);
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

#use File::Temp "mktemp";$_=mktemp("123.XXXX");print "$_\n";`touch $_`
foreach (DEPENDS()) {
    my ($i, $l) = find_lib($_);
    if (defined $i || $l) {
        my $d = 'HAVE_' . uc($_) . ' = 1';
        push @dependencies, $d;
        println "\t\t$d" if ($OPT{'debug'});
    }
    if ($i) {
        my $d = uc($_) . "_INCDIR = $i";
        push @dependencies, $d;
        println "\t\t$d" if ($OPT{'debug'});
    }
    if ($l) {
        my $d = uc($_) . "_LIBDIR = $l";
        push @dependencies, $d;
        println "\t\t$d" if ($OPT{'debug'});
    }
}

foreach my $href (@REQ) {
    $href->{bldpath} =~ s/(\$\w+)/$1/eeg;
    my ($found_itf, $found_lib, $found_ilib);        # found directories
    my %a = %$href;
    my $is_optional = $a{type} eq 'SI';
    my $need_source = $a{type} =~ /S/;
    my $need_build = $a{type} =~ /B/;
    my $need_lib = $a{type} =~ /L/;
    
    my ($inc, $lib, $ilib) = ($a{include}, $a{lib}); # file names to check
    $lib =~ s/(\$\w+)/$1/eeg;

    if ($need_build) {
        $ilib = $a{ilib};
        ++$need_lib;
    }
    unless ($AUTORUN) {
        if ($need_source && $need_build) {
            println
               "checking for $a{name} package source files and build results..."
        } elsif ($need_source) {
            println "checking for $a{name} package source files...";
        } else {
            println "checking for $a{name} package...";
        }
    }
    my %has_option;
    foreach my $option ($a{option}, $a{boption}) {
        next unless ($option);
        if ($OPT{$option}) {
            my $try = expand($OPT{$option});
            my ($i, $l, $il) = ($inc, $lib, $ilib);
            if ($option =~ /-build$/) {
                undef $i;
                ++$has_option{build};
            } elsif ($option =~ /-prefix$/) {
                undef $il;
                ++$has_option{prefix};
            } elsif ($option =~ /-sources$/) {
                undef $l;
                undef $il;
                ++$has_option{sources};
            }
            my ($fi, $fl, $fil) = find_in_dir($try, $i, $l, $il);
            $found_itf  = $fi  if (! $found_itf  && $fi);
            $found_lib  = $fl  if (! $found_lib  && $fl);
            $found_ilib = $fil if (! $found_ilib && $fil);
        }
    }
    unless ($found_itf || $has_option{sources}) {
        my $try = $a{srcpath};
        ($found_itf) = find_in_dir($try, $inc);
    }
    if (! $has_option{prefix}) {
        if (! $found_itf || ($need_lib && ! $found_lib)) {
            my $try = $a{pkgpath};
            my ($fi, $fl) = find_in_dir($try, $inc, $lib);
            $found_itf  = $fi  if (! $found_itf  && $fi);
            $found_lib  = $fl  if (! $found_lib  && $fl);
        }

        if (! $found_itf || ($need_lib && ! $found_lib)) {
            my $try = $a{usrpath};
            my ($fi, $fl) = find_in_dir($try, $inc, $lib);
            $found_itf  = $fi  if (! $found_itf  && $fi);
            $found_lib  = $fl  if (! $found_lib  && $fl);
        }
    }
    if (! $has_option{build}) {
        if ($need_build || ($need_lib && ! $found_lib)) {
            my $try = $a{bldpath};
            my (undef, $fl, $fil) = find_in_dir($try, undef, $lib, $ilib);
            $found_lib  = $fl  if (! $found_lib  && $fl);
            $found_ilib = $fil if (! $found_ilib && $fil);
        }
    }
    if (! $found_itf || ($need_lib && ! $found_lib) || ($ilib && ! $found_ilib))
    {
        if ($is_optional) {
            println "configure: optional $a{name} package not found: skipped.";
        } else {
            if ($OPT{'debug'}) {
                $_ = "$a{name}: includes: ";
                $found_itf = '' unless $found_itf;
                $_ .= $found_itf;
                unless ($need_lib) {
                    $_ .= "; libs: not needed";
                } else {
                    $found_lib = '' unless $found_lib;
                    $_ .= "; libs: " . $found_lib;
                }
                unless ($ilib) {
                    $_ .= "; ilibs: not needed";
                } else {
                    $found_ilib = '' unless $found_ilib;
                    $_ .= "; ilibs: " . $found_ilib;
                }
                println "\t\t$_";
            }
            println "configure: error: required $a{name} package not found.";
            exit 1;
        }
    } else {
        $found_itf = abs_path($found_itf);
        push(@dependencies, "$a{namew}_INCDIR = $found_itf");
        if ($found_lib) {
            $found_lib = abs_path($found_lib);
            push(@dependencies, "$a{namew}_LIBDIR = $found_lib");
        }
        if ($ilib && $found_ilib) {
            $found_ilib = abs_path($found_ilib);
            push(@dependencies, "$a{namew}_ILIBDIR = $found_ilib");
        }
    }
}

if ($OS ne 'win' && ! $OPT{'status'}) {
    # create Makefile.config
    println "configure: creating '$OUT_MAKEFILE'" unless ($AUTORUN);
    open my $F, ">$OUT_MAKEFILE" or die "cannot open $OUT_MAKEFILE to write";

    print $F <<EndText;
### AUTO-GENERATED FILE ###

OS_ARCH = \$(shell perl \$(TOP)/os-arch.perl)

# install paths
EndText

    L($F, "INST_BINDIR = $OPT{bindir}"        ) if ($OPT{bindir});
    L($F, "INST_LIBDIR = $OPT{libdir}"        ) if ($OPT{libdir});
    L($F, "INST_INCDIR = $OPT{includedir}"    ) if ($OPT{includedir});
    L($F, "INST_SCHEMADIR = $OPT{'shemadir'}" ) if ($OPT{'shemadir'});
    L($F, "INST_SHAREDIR = $OPT{'sharedir'}"  ) if ($OPT{'sharedir'});
    L($F, "INST_JARDIR = $OPT{'javadir'}"     ) if ($OPT{'javadir'});
    L($F, "INST_PYTHONDIR = $OPT{'pythondir'}") if ($OPT{'pythondir'});

    my ($VERSION_SHLX, $MAJMIN_SHLX, $MAJVERS_SHLX);
    if ($OSTYPE =~ /darwin/i) {
        $VERSION_SHLX = '$(VERSION).$(SHLX)';
        $MAJMIN_SHLX  = '$(MAJMIN).$(SHLX)';
        $MAJVERS_SHLX = '$(MAJVERS).$(SHLX)';
    } else {
        $VERSION_SHLX = '$(SHLX).$(VERSION)';
        $MAJMIN_SHLX  = '$(SHLX).$(MAJMIN)';
        $MAJVERS_SHLX = '$(SHLX).$(MAJVERS)';
    }


    print $F <<EndText;

# build type
BUILD = $BUILD

# target OS
OS    = $OS
OSINC = $OSINC

# prefix string for system libraries
LPFX = $LPFX

# suffix strings for system libraries
LIBX = $LIBX
VERSION_LIBX = \$(LIBX).\$(VERSION)
MAJMIN_LIBX  = \$(LIBX).\$(MAJMIN)
MAJVERS_LIBX = \$(LIBX).\$(MAJVERS)

SHLX = $SHLX
VERSION_SHLX = $VERSION_SHLX
MAJMIN_SHLX  = $MAJMIN_SHLX
MAJVERS_SHLX = $MAJVERS_SHLX

# suffix strings for system object files
OBJX = $OBJX
LOBX = $LOBX

# suffix string for system executable
EXEX = $EXEX
VERSION_EXEX = \$(EXEX).\$(VERSION)
MAJMIN_EXEX  = \$(EXEX).\$(MAJMIN)
MAJVERS_EXEX = \$(EXEX).\$(MAJVERS)

# system architecture and wordsize
ARCH = $ARCH
EndText

    L($F, "# ARCH = $ARCH ( $MARCH )") if ($ARCH ne $MARCH);

    print $F <<EndText;
BITS = $BITS

# tools
EndText

    L($F, "CC    = $CC"   ) if ($CC);
    L($F, "CP    = $CP"   ) if ($CP);
    L($F, "AR    = $AR"   ) if ($AR);
    L($F, "ARX   = $ARX"  ) if ($ARX);
    L($F, "ARLS  = $ARLS" ) if ($ARLS);
    L($F, "LD    = $LD"   ) if ($LD);
    L($F, "LP    = $LP"   ) if ($LP);
    L($F, "JAVAC = $JAVAC") if ($JAVAC);
    L($F, "JAVAH = $JAVAH") if ($JAVAH);
    L($F, "JAR   = $JAR"  ) if ($JAR);
    L($F);

    L($F, '# tool options');
    if ($BUILD eq "dbg") {
        L($F, "DBG     = $DBG");
        L($F, "OPT     = ");
    } else {
        L($F, "DBG     = -DNDEBUG") if ($PKG{LNG} eq 'C');
        L($F, "OPT     = $OPT"    ) if ($OPT);
    }
    L($F, "PIC     = $PIC") if ($PIC);
    if ($PKG{LNG} eq 'C') {
        if ($TOOLS =~ /clang/i) {
      L($F, 'SONAME = -install_name ' .
               '$(INST_LIBDIR)$(BITS)/$(subst $(VERSION),$(MAJVERS),$(@F)) \\');
      L($F, ' -compatibility_version $(MAJMIN) -current_version $(VERSION) \\');
      L($F, ' -flat_namespace -undefined suppress');
        } else {
      L($F, 'SONAME = -Wl,-soname=$(subst $(VERSION),$(MAJVERS),$(@F))');
     }
     L($F, "SRCINC  = $INC. $INC\$(SRCDIR)");
    } elsif ($PKG{LNG} eq 'JAVA') {
        L($F, 'SRCINC  = -sourcepath $(INCPATHS)');
    }
    L($F, "INCDIRS = \$(SRCINC) $INC\$(TOP)") if ($PIC);
    if ($PKG{LNG} eq 'C') {
        L($F, "CFLAGS  = \$(DBG) \$(OPT) \$(INCDIRS) $MD");
    }

    L($F, 'CLSPATH = -classpath $(CLSDIR)');
    L($F);

    # version information

    my ($VERSION, $MAJMIN, $MAJVERS);

    if ($FULL_VERSION =~ /(\d+)\.(\d+)\.(\d+)-?\w*\d*/) {
        $VERSION = "$1.$2.$3";
        $MAJMIN = "$1.$2";
        $MAJVERS = $1;
    } else {
        die $VERSION;
    }

    print $F <<EndText;
# $PACKAGE_NAME and library version
VERSION = $VERSION
MAJMIN  = $MAJMIN
MAJVERS = $MAJVERS

# output path
TARGDIR = $TARGDIR

# derived paths
MODPATH  ?= \$(subst \$(TOP)/,,\$(CURDIR))
SRCDIR   ?= \$(TOP)/\$(MODPATH)
MAKEFILE ?= \$(abspath \$(firstword \$(MAKEFILE_LIST)))
BINDIR    = \$(TARGDIR)/bin
EndText

    if ($PKG{LNG} eq 'C') {
        L($F, 'LIBDIR    = $(TARGDIR)/lib');
    } elsif ($PKG{LNG} eq 'JAVA') {
        L($F, 'LIBDIR    = $(TARGDIR)/jar');
    }

    print $F <<EndText;
ILIBDIR   = \$(TARGDIR)/ilib
OBJDIR    = \$(TARGDIR)/obj/\$(MODPATH)
CLSDIR    = \$(TARGDIR)/cls
EndText

    if ($PKG{LNG} eq 'JAVA') {
        L($F,
            "INCPATHS = \$(SRCDIR):\$(SRCDIR)/itf:\$(TOP)/gov/nih/nlm/ncbi/ngs")
    }

    print $F <<EndText;

# exports
export TOP
export MODPATH
export SRCDIR
export MAKEFILE

# auto-compilation rules
EndText

    if ($PKG{LNG} eq 'C') {
        L($F, '$(OBJDIR)/%.$(OBJX): %.c');
        T($F, '$(CC) -o $@ $< $(CFLAGS)');
        L($F, '$(OBJDIR)/%.$(LOBX): %.c');
        T($F, '$(CC) -o $@ $< $(PIC) $(CFLAGS)');
    }
    L($F, '$(OBJDIR)/%.$(OBJX): %.cpp');
    T($F, '$(CP) -o $@ $< $(CFLAGS)');
    L($F, '$(OBJDIR)/%.$(LOBX): %.cpp');
    T($F, '$(CP) -o $@ $< $(PIC) $(CFLAGS)');
    L($F);

    # this is part of Makefile
    L($F, 'VPATH = $(SRCDIR)');
    L($F);

    # we know how to find jni headers
    if ($PKG{LNG} eq 'JAVA' and $OPT{'with-ngs-sdk-src'}) {
        L($F, "JNIPATH = $OPT{'with-ngs-sdk-src'}/language/java");
    }

    L($F, '# directory rules');
    if ($PKG{LNG} eq 'C') {
        L($F, '$(BINDIR) $(LIBDIR) $(ILIBDIR) '
            . '$(OBJDIR) $(INST_LIBDIR) $(INST_LIBDIR)$(BITS):');
        T($F, 'mkdir -p $@');
    } elsif ($PKG{LNG} eq 'JAVA') {
        # test if we have jni header path
        L($F, '$(LIBDIR) $(CLSDIR) $(INST_JARDIR):');
        T($F, 'mkdir -p $@');
    }
    L($F);

    L($F, '# not real targets');
    L($F, '.PHONY: default clean install all std $(TARGETS)');
    L($F);

    L($F, '# dependencies');
    if ($PKG{LNG} eq 'C') {
        L($F, 'include $(wildcard $(OBJDIR)/*.d)');
    } elsif ($PKG{LNG} eq 'JAVA') {
        L($F, 'include $(wildcard $(CLSDIR)/*.d)');
    }
    L($F, $_) foreach (@dependencies);
    L($F);

    if ($OS eq 'linux' || $OS eq 'mac') {
        L($F, '# installation rules');
        L($F,
        '$(INST_LIBDIR)$(BITS)/%.$(VERSION_LIBX): $(LIBDIR)/%.$(VERSION_LIBX)');
        T($F, '@ echo -n "installing \'$(@F)\'... "');
        T($F, '@ if cp $^ $@ && chmod 644 $@;                         \\');
        T($F, '  then                                                 \\');
      if ($OS eq 'mac') {
        T($F, '      rm -f $(subst $(VERSION),$(MAJVERS),$@) '
                      . '$(subst $(VERSION_LIBX),$(LIBX),$@); '
                      . '$(subst .$(VERSION_LIBX),-static.$(LIBX),$@); \\');
      } else {
        T($F, '      rm -f $(subst $(VERSION),$(MAJVERS),$@) '
                      . '$(subst $(VERSION_LIBX),$(LIBX),$@);         \\');
      }
        T($F, '      ln -s $(@F) $(subst $(VERSION),$(MAJVERS),$@);   \\');
        T($F, '      ln -s $(subst $(VERSION),$(MAJVERS),$(@F)) '
                      . '$(subst $(VERSION_LIBX),$(LIBX),$@); \\');
        T($F, '      echo success;                                    \\');
        T($F, '  else                                                 \\');
        T($F, '      echo failure;                                    \\');
        T($F, '      false;                                           \\');
        T($F, '  fi');
        L($F);

        L($F,
        '$(INST_LIBDIR)$(BITS)/%.$(VERSION_SHLX): $(LIBDIR)/%.$(VERSION_SHLX)');
        T($F, '@ echo -n "installing \'$(@F)\'... "');
        T($F, '@ if cp $^ $@ && chmod 755 $@;                         \\');
        T($F, '  then                                                 \\');
        T($F, '      rm -f $(subst $(VERSION),$(MAJVERS),$@) '
                      . '$(subst $(VERSION_SHLX),$(SHLX),$@);    \\');
        T($F, '      ln -s $(@F) $(subst $(VERSION),$(MAJVERS),$@);   \\');
        T($F, '      ln -s $(subst $(VERSION),$(MAJVERS),$(@F)) '
                      . '$(subst $(VERSION_SHLX),$(SHLX),$@); \\');
      if ($OS ne 'mac') {
        T($F, '      cp -v $(LIBDIR)/$(subst $(VERSION_SHLX),'
                    . '$(VERSION_LIBX),$(@F)) $(INST_LIBDIR)$(BITS)/; \\');
        T($F, '      ln -vfs $(subst $(VERSION_SHLX),$(VERSION_LIBX), $(@F)) ' .
       '$(INST_LIBDIR)$(BITS)/$(subst .$(VERSION_SHLX),-static.\$(LIBX),$(@F));'
                                                              . ' \\');
      }
        T($F, '      echo success;                                    \\');
        T($F, '  else                                                 \\');
        T($F, '      echo failure;                                    \\');
        T($F, '      false;                                           \\');
        T($F, '  fi');
        L($F);

        L($F, '$(INST_BINDIR)/%$(VERSION_EXEX): $(BINDIR)/%$(VERSION_EXEX)');
        T($F, '@ echo -n "installing \'$(@F)\'... "');
        T($F, '@ if cp $^ $@ && chmod 755 $@;                         \\');
        T($F, '  then                                                 \\');
        T($F, '      rm -f $(subst $(VERSION),$(MAJVERS),$@) '
                      . '$(subst $(VERSION_EXEX),$(EXEX),$@);     \\');
        T($F, '      ln -s $(@F) $(subst $(VERSION),$(MAJVERS),$@);   \\');
        T($F, '      ln -s $(subst $(VERSION),$(MAJVERS),$(@F)) '
                      . '$(subst $(VERSION_EXEX),$(EXEX),$@); \\');
        T($F, '      echo success;                                    \\');
        T($F, '  else                                                 \\');
        T($F, '      echo failure;                                    \\');
        T($F, '      false;                                           \\');
        T($F, '  fi');
    }
    close $F;
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
        open COUT, ">$out" or die "cannot open $out to write";
        print COUT "### AUTO-GENERATED FILE ###\n";
        print COUT "\n";
        print COUT "OS_ARCH = \$(shell perl \$(TOP)/os-arch.perl)\n";
        print COUT "include \$(TOP)/$CONFIG_OUT/Makefile.config.\$(OS_ARCH)\n";
        close COUT;
    }
}

status() if ($OS ne 'win');

unlink 'a.out';

sub L { $_[1] = '' unless ($_[1]); print { $_[0] }   "$_[1]\n" }
sub T {                            print { $_[0] } "\t$_[1]\n" }

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
    my $a = abs_path($filename);
    $filename = $a if ($a);
    $filename;
}

sub find_in_dir {
    my ($dir, $include, $lib, $ilib) = @_;
    print "\t$dir... " unless ($AUTORUN);
    unless (-d $dir) {
        println "no" unless ($AUTORUN);
        println "\t\tnot found $dir" if ($OPT{'debug'});
        return;
    }
    print "[found] " if ($OPT{'debug'});
    my ($found_inc, $found_lib, $found_ilib);
    my $nl = 1;
    if ($include) {
        print "includes... " unless ($AUTORUN);
        if (-e "$dir/$include") {
            println 'yes' unless ($AUTORUN);
            $found_inc = $dir;
        } elsif (-e "$dir/include/$include") {
            println 'yes' unless ($AUTORUN);
            $found_inc = "$dir/include";
        } elsif (-e "$dir/interfaces/$include") {
            println 'yes' unless ($AUTORUN);
            $found_inc = "$dir/interfaces";
        } else {
            println 'no' unless ($AUTORUN);
        }
        $nl = 0;
    }
    if ($lib || $ilib) {
        print "\n\t" if ($nl && !$AUTORUN);
        print "libraries... " unless ($AUTORUN);
        if ($lib) {
            my $builddir = File::Spec->catdir($dir, $OS, $TOOLS, $ARCH, $BUILD);
            my $libdir  = File::Spec->catdir($builddir, 'lib');
            my $ilibdir = File::Spec->catdir($builddir, 'ilib');
            my $f = File::Spec->catdir($libdir, $lib);
            print "\n\t\tchecking $f\n\t" if ($OPT{'debug'});
            my $found;
            if (-e $f) {
                $found_lib = $libdir;
                if ($ilib) {
                    my $f = File::Spec->catdir($ilibdir, $ilib);
                    print "\tchecking $f\n\t" if ($OPT{'debug'});
                    if (-e $f) {
                        println 'yes';
                        $found_ilib = $ilibdir;
                    } else {
                        println 'no' unless ($AUTORUN);
                        return;
                    }
                } else {
                    println 'yes';
                }
                ++$found;
            }
            if (! $found) {
                my $libdir = File::Spec->catdir($dir, 'lib' . $BITS);
                my $f = File::Spec->catdir($libdir, $lib);
                print "\tchecking $f\n\t" if ($OPT{'debug'});
                if (-e $f) {
                    println 'yes';
                    $found_lib = $libdir;
                    ++$found;
                }
            }
            if (! $found) {
                my $builddir = File::Spec->catdir
                    ($dir, $OS, $TOOLS, $ARCH, reverse_build($BUILD));
                my $libdir  = File::Spec->catdir($builddir, 'lib');
                my $ilibdir = File::Spec->catdir($builddir, 'ilib');
                my $f = File::Spec->catdir($libdir, $lib);
                print "\tchecking $f\n\t" if ($OPT{'debug'});
                if (-e $f) {
                    $found_lib = $libdir;
                    if ($ilib) {
                        my $f = File::Spec->catdir($ilibdir, $ilib);
                        print "\tchecking $f\n\t" if ($OPT{'debug'});
                        if (-e $f) {
                            println 'yes';
                            $found_ilib = $ilibdir;
                        } else {
                            println 'no' unless ($AUTORUN);
                            return;
                        }
                    } else {
                        println 'yes';
                    }
                    ++$found;
                }
            }
        }
        if ($found_lib && $ilib && ! $found_ilib) {
            println "\n\t\tfound $found_lib but no ilib/" if ($OPT{'debug'});
            print "\t" if ($OPT{'debug'});
            println 'no' unless ($AUTORUN);
            undef $found_lib;
        }
        ++$nl;
    }
    return ($found_inc, $found_lib, $found_ilib);
}

sub reverse_build {
    ($_) = @_;
    if ($_ eq 'rel') {
        return 'dbg';
    } elsif ($_ eq 'dbg') {
        return 'rel';
    } else {
        die $_;
    }
}

################################################################################

sub find_lib {
    my ($l) = @_;

    print "checking for $l library... ";

    while (1) {
        my ($i, $library, $log);

        if ($l eq 'hdf5') {
            $library = '-lhdf5';
            $log = '#include <hdf5.h>            \n main() { H5close     (); }';
        } elsif ($l eq 'xml2') {
            $i = '/usr/include/libxml2';
            $library = '-lxml2';
            $log = '#include <libxml/xmlreader.h>\n main() { xmlInitParser();}';
        } elsif ($l eq 'magic') {
            $i = '/usr/include';
            $library = '-lmagic';
            $log = '#include <magic.h>           \n main() { magic_open (0); }';
        } else {
            println 'unknown: skipped';
            return;
        }

        if ($i && ! -d $i) {
            println 'no';
            return;
        }

        my $cmd = $log;
        $cmd =~ s/\\n/\n/g;

        my $gcc = "| gcc -xc " . ($i ? "-I$i" : '') . " - $library";
        $gcc .= ' 2> /dev/null' unless ($OPT{'debug'});

        open GCC, $gcc or last;
        print "\n\t\trunning echo -e '$log' $gcc\n" if ($OPT{'debug'});
        print GCC "$cmd" or last;
        my $ok = close GCC;
        print "\t" if ($OPT{'debug'});
        println $ok ? 'yes' : 'no';

        unlink 'a.out';

        return if (!$ok);

        $i = '' unless ($i);
        return ($i);
    }

    println 'cannot run gcc: skipped';
}

################################################################################

sub check {
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

    foreach (DEPENDS()) {}

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
        if ($PACKAGE eq 'sra-tools' && 0) {
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
                println
                    "  --$a{option}=DIR      search for $a{name} package in DIR"
            }
            if ($a{boption}) {
                println "  --$a{boption}=DIR      search for $a{name} package";
                println "                                 build output in DIR";
            }
            println;
        }
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

  --build=DIR             generate build output into DIR directory
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
