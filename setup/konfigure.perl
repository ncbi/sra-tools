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

use Cwd 'abs_path';
use File::Basename 'dirname';
use lib dirname( abs_path $0 );

sub println  { print @_; print "\n" }

my ($filename, $directories, $suffix) = fileparse($0);
if ($directories ne "./") {
    println "configure: error: $filename should be run as ./$filename";
    exit 1;
}

require 'package.prl';
require 'os-arch.prl';

use Cwd qw(abs_path getcwd);
use File::Basename 'fileparse';
use File::Spec 'catdir';
use FindBin qw($Bin);
use Getopt::Long "GetOptions";

chdir '..' or die "cannot cd to package root";

check();

my $LOCAL_BUILD_OUT
    = -e File::Spec->catdir($ENV{HOME}, 'tmp', 'local-build-out');

my ($CONFIGURED, $RECONFIGURE) = ('');
if (@ARGV) {
    foreach (@ARGV) {
        $CONFIGURED .= "\t" if ($CONFIGURED);
        $CONFIGURED .= "'$_'";
    }
} elsif (-f 'reconfigure') {
    ++$RECONFIGURE unless ($LOCAL_BUILD_OUT);
}

my %PKG = PKG();

my $PACKAGE_NAME = PACKAGE_NAME();
my $OUT_MAKEFILE = 'Makefile.config';
my $INS_MAKEFILE = 'Makefile.config.install';

my $PACKAGE = PACKAGE();

my $HOME = $ENV{HOME} || $ENV{USERPROFILE}
    || $ENV{LOGDIR} || getcwd || (getpwuid($<))[7] || abs_path('.');

$PKG{UPATH} = expand($PKG{UPATH});

my $package_default_prefix = $PKG{PATH};
my $schema_default_dir = $PKG{SCHEMA_PATH} if ($PKG{SCHEMA_PATH});

my @REQ = REQ();

my @options = ( 'build-prefix=s',
                'clean',
                'debug',
                'help',
                'prefix=s',
                'reconfigure',
                'status',
                'with-debug',
                'without-debug', );
{
    my ($OS, $ARCH, $OSTYPE, $MARCH, @ARCHITECTURES) = OsArch();
    push @options, 'arch=s'    if (@ARCHITECTURES);
}
push @options, 'source=s' if ($PKG{LNG} eq 'JAVA');
push @options, 'enable-static' if (PACKAGE_TYPE() eq 'B');
foreach my $href (@REQ) {
    my %a = %$href;
    push @options, "$a{option}=s"  if ($a {option});
    push @options, "$a{boption}=s" if ($a{boption});
    $href->{usrpath} = '' unless ($href->{usrpath});
    $href->{usrpath} = expand($href->{usrpath});
}
push @options, "shemadir" if ($PKG{SCHEMA_PATH});

my %OPT;
die "configure: error" unless (GetOptions(\%OPT, @options));
++$OPT{'reconfigure'} if ($RECONFIGURE);

if ($OPT{'reconfigure'}) {
    unless (eval 'use Getopt::Long qw(GetOptionsFromString); 1') {
        print <<EndText;
configure: error: your perl does not support Getopt::Long::GetOptionsFromString
                  reconfigure option is not avaliable.
Run "sh ./reconfigure" instead.
EndText
        exit 1;
    }
    println "reconfiguring...";
    open F, 'reconfigure' or die 'cannot open reconfigure';
    $_ = <F>;
    chomp;
    unless (m|^\./configure\s*(.*)$|) {
        println 'configure: error: cannot reconfigure';
        println 'run "./configure --clean" then run "./configure [OPTIONS]"';
        exit 1;
    }

    my $ARG = $1;
    println "running \"./configure $ARG\"...";
    undef %OPT;
    die "configure: error" unless (GetOptionsFromString($ARG, \%OPT, @options));
    $CONFIGURED = $ARG if ($#ARGV == -1 && $RECONFIGURE);
    ++$OPT{reconfigure};
}

$OPT{'local-build-out'} = $LOCAL_BUILD_OUT;
my $OUTDIR = File::Spec->catdir($HOME, $PKG{OUT});
if ($OPT{'local-build-out'}) {
    my $o = expand_path(File::Spec->catdir($Bin, $PKG{LOCOUT}));
    $OUTDIR = $o if ($o);
}

if ($OPT{'help'}) {
    help();
    exit 0;
} elsif ($OPT{'clean'}) {
    {
        foreach ('reconfigure', glob(CONFIG_OUT() . '/Makefile.config*'),
            File::Spec->catdir(CONFIG_OUT(), 'Makefile.userconfig'),
            File::Spec->catdir(CONFIG_OUT(), 'user.status'))
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
    exit 0;
} elsif ($OPT{'status'}) {
    status(1);
    exit 0;
}

foreach (@ARGV) {
    @_ = split('=');
    next if ($#_ != 1);
    $OPT{$_[0]} = $_[1] if ($_[0] eq 'CXX' || $_[0] eq 'LDFLAGS');
}

println "Configuring $PACKAGE_NAME package";

$OPT{'prefix'} = $package_default_prefix unless ($OPT{'prefix'});

my $AUTORUN = $OPT{status};
print "checking system type... " unless ($AUTORUN);
my ($OS, $ARCH, $OSTYPE, $MARCH, @ARCHITECTURES) = OsArch();
println $OSTYPE unless ($AUTORUN);

unless ($OSTYPE =~ /linux/i || $OSTYPE =~ /darwin/i || $OSTYPE eq 'win') {
    println "configure: error: unsupported system '$OSTYPE'";
    exit 1;
}

my $OS_DISTRIBUTOR = '';
if ($OS eq 'linux') {
    print "checking OS distributor... " unless ($AUTORUN);
    $OS_DISTRIBUTOR = `lsb_release -si 2> /dev/null`;
    if ( $? != 0 ) {
        $_ = `cat /etc/redhat-release 2> /dev/null`;
        @_ = split ( / /  );
        $OS_DISTRIBUTOR = $_[0] if ( $_[0] );
    }
    $OS_DISTRIBUTOR = '' unless ( $OS_DISTRIBUTOR );
    chomp $OS_DISTRIBUTOR;
    println $OS_DISTRIBUTOR unless ($AUTORUN);
}

print "checking machine architecture... " unless ($AUTORUN);
println $MARCH unless ($AUTORUN);
unless ($MARCH =~ /x86_64/i || $MARCH =~ /i?86/i) {
    println "configure: error: unsupported architecture '$OSTYPE'";
    exit 1;
}

{
    $OPT{'prefix'} = expand_path($OPT{'prefix'});
    my $prefix = $OPT{'prefix'};
    $OPT{'eprefix'} = $prefix unless ($OPT{'eprefix'} || $OS eq 'win');
    my $eprefix = $OPT{'eprefix'};
    unless ($OPT{'bindir'} || $OS eq 'win') {
        $OPT{'bindir'} = File::Spec->catdir($eprefix, 'bin') ;
    }
    unless ($OPT{'libdir'} || $OS eq 'win') {
        $OPT{'libdir'} = File::Spec->catdir($eprefix, 'lib');
    }
    unless ($OPT{'includedir'} || $OS eq 'win') {
        $OPT{'includedir'} = File::Spec->catdir($eprefix, 'include');
    }
    if ($PKG{LNG} eq 'PYTHON' && ! $OPT{'pythondir'} && $OS ne 'win') {
        $OPT{'pythondir'} = $eprefix;
    }
    if ($PKG{LNG} eq 'JAVA' && ! $OPT{'javadir'} && $OS ne 'win') {
        $OPT{'javadir'} = File::Spec->catdir($eprefix, 'jar');
    }
    if ($PKG{EXAMP} && ! $OPT{'sharedir'} && $OS ne 'win') {
        $OPT{'sharedir'} = File::Spec->catdir($eprefix, 'share');
    }
}

# initial values
my $TARGDIR = File::Spec->catdir($OUTDIR, $PACKAGE);
if ($OPT{'build-prefix'}) {
    $TARGDIR = $OPT{'build-prefix'} = expand_path($OPT{'build-prefix'});
    unless ($TARGDIR =~ /$PACKAGE$/) {
        $TARGDIR = File::Spec->catdir($TARGDIR, $PACKAGE);
    }
}
my $BUILD_PREFIX = $TARGDIR;

my $BUILD = 'rel';

# parse command line
$BUILD = 'dbg' if ($OPT{'with-debug'});
$BUILD = 'rel' if ($OPT{'without-debug'});

my $BUILD_TYPE = "release";
$BUILD_TYPE = "debug" if ( $BUILD eq "dbg" );

$OPT{arch} = $ARCH if (@ARCHITECTURES && ! $OPT{arch});

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
        while (1) {
            open F, ">Makefile.config.$OS.arch" or last;
            print F "$ARCH\n";
            close F;
            last;
        }
        println "build architecture: $ARCH" unless ($AUTORUN);
    } else {
        delete $OPT{arch};
    }
}

$OUT_MAKEFILE = File::Spec->catdir(CONFIG_OUT(), "$OUT_MAKEFILE.$OS.$ARCH");
$INS_MAKEFILE = File::Spec->catdir(CONFIG_OUT(), "$INS_MAKEFILE.$OS.$ARCH.prl");

my $TOOLS = "";
$TOOLS = "jdk" if ($PKG{LNG} eq 'JAVA');

# determine architecture

print "checking for supported architecture... " unless ($AUTORUN);

my $BITS;

if ($MARCH =~ /x86_64/i) {
    $BITS = 64;
} elsif ($MARCH eq 'fat86') {
    $BITS = '32_64';
} elsif ($MARCH =~ /i?86/i) {
    $BITS = 32;
} else {
    die "unrecognized Architecture '$ARCH'";
}
println "$MARCH ($BITS bits) is supported" unless ($AUTORUN);

# determine OS and related norms
my ($LPFX, $OBJX, $LOBX, $LIBX, $SHLX, $EXEX, $OSINC, $PYTHON);

print "checking for supported OS... " unless ($AUTORUN);
if ($OSTYPE =~ /linux/i) {
    $LPFX = 'lib';
    $OBJX = 'o';
    $LOBX = 'pic.o';
    $LIBX = 'a';
    $SHLX = 'so';
    $EXEX = '';
    $OSINC = 'unix';
    $TOOLS = 'gcc' unless ($TOOLS);
    $PYTHON = 'python';
} elsif ($OSTYPE =~ /darwin/i) {
    $LPFX = 'lib';
    $OBJX = 'o';
    $LOBX = 'pic.o';
    $LIBX = 'a';
    $SHLX = 'dylib';
    $EXEX = '';
    $OSINC = 'unix';
    $TOOLS = 'clang' unless ($TOOLS);
    $PYTHON = 'python';
} elsif ($OSTYPE eq 'win') {
    $TOOLS = 'vc++';
} else {
    die "unrecognized OS '$OSTYPE'";
}

println "$OSTYPE ($OS) is supported" unless ($AUTORUN);

# tool chain
my ($CPP, $CC, $CP, $AR, $ARX, $ARLS, $LD, $LP, $MAKE_MANIFEST);
my ($JAVAC, $JAVAH, $JAR);
my ($ARCH_FL, $DBG, $OPT, $PIC, $INC, $MD, $LDFLAGS) = ('');

print "checking for supported tool chain... " unless ($AUTORUN);

$CPP     = $OPT{CXX    } if ($OPT{CXX    });
$LDFLAGS = $OPT{LDFLAGS} if ($OPT{LDFLAGS});

if ($TOOLS =~ /gcc$/) {
    $CPP  = 'g++' unless ($CPP);
    $CC   = "$TOOLS -c";
    $CP   = "$CPP -c";
    $AR   = 'ar rc';
    $ARX  = 'ar x';
    $ARLS = 'ar t';
    $LD   = $TOOLS;
    $LP   = $CPP;

    $DBG = '-g -DDEBUG';
    $OPT = '-O3';
    $PIC = '-fPIC';
    $INC = '-I';
    $MD  = '-MD';
} elsif ($TOOLS eq 'clang') {
    $CPP  = 'clang++' unless ($CPP);
    $CC   = 'clang -c';
    my $versionMin = '-mmacosx-version-min=10.10';
    $CP   = "$CPP -c $versionMin";
    if ($BITS ne '32_64') {
        $ARCH_FL = '-arch i386' if ($BITS == 32);
        $OPT = '-O3';
        $AR      = 'ar rc';
        $LD      = "clang $ARCH_FL";
        $LP      = "$CPP $versionMin $ARCH_FL";
    } else {
        $MAKE_MANIFEST = '( echo "$^" > $@/manifest )';
        $ARCH_FL       = '-arch i386 -arch x86_64';
        $OPT    = '-O3';
        $AR     = 'libtool -static -o';
        $LD     = "clang -Wl,-arch_multiple $ARCH_FL -Wl,-all_load";
        $LP     = "$CPP $versionMin -Wl,-arch_multiple $ARCH_FL -Wl,-all_load";
    }
    $ARX  = 'ar x';
    $ARLS = 'ar t';

    $DBG = '-g -DDEBUG';
    $PIC = '-fPIC';
    $INC = '-I';
    $MD  = '-MD';
} elsif ($TOOLS eq 'jdk') {
    $JAVAC = 'javac';
    if ($OPT{source}) {
        $JAVAC .= ' -target ' . $OPT{source} . ' -source ' . $OPT{source};
    }
    $JAVAH = 'javah';
    $JAR   = 'jar cf';

    $DBG = '-g';
} elsif ($TOOLS eq 'vc++') {
} else {
    die "unrecognized tool chain '$TOOLS'";
}
println "$TOOLS tool chain is supported" unless ($AUTORUN);

if ($OS ne 'win' && $PKG{LNG} ne 'JAVA') {
    $TARGDIR = File::Spec->catdir($TARGDIR, $OS, $TOOLS, $ARCH, $BUILD);
}

if ($CPP) {
    unless (check_tool__h($CPP)) {
        println "configure: error: '$CPP' cannot be found";
        exit 1;
    }
}

if ($JAVAC) {
    unless (check_tool_h($JAVAC)) {
        println "configure: error: '$JAVAC' cannot be found";
        exit 1;
    }
}

print 'checking for Python 3... ' unless $AUTORUN;
if ($PYTHON) {
    my $p3;
    for my $dir (File::Spec->path()) {
        $p3 = File::Spec->join($dir, 'python3');
        my $pX = substr($p3, 0, -1);
        if (-x $pX && `$pX --version 2>&1` =~ /^\s*Python\s+(\d+)/i && $1 == 3) {
            $p3 = $pX;
            last;
        }
        if (-x $p3 && `$p3 --version 2>&1` =~ /^\s*Python\s+(\d+)/i && $1 == 3) {
            last;
        }
        undef $p3;
    }
    if ($p3) {
        $PYTHON = $p3;
        println $PYTHON unless $AUTORUN;
    }
    else {
        undef $PYTHON;
        println 'no' unless $AUTORUN;
    }
}
else {
    println 'skipped' unless $AUTORUN;
}

my $NO_ARRAY_BOUNDS_WARNING = '';
if ($TOOLS =~ /gcc$/ && check_no_array_bounds()) {
    $NO_ARRAY_BOUNDS_WARNING = '-Wno-array-bounds';
}

my $STATIC_LIBSTDCPP = '';
if ($TOOLS =~ /gcc$/) {
    $STATIC_LIBSTDCPP = check_static_libstdcpp();
}

if ( $PKG{REQ} ) {
    foreach ( @{ $PKG{REQ} } ) {
        unless (check_tool__h($_)) {
            println "configure: error: '$_' cannot be found";
            exit 1;
        }
    }
}

my @dependencies;

if ( $PKG{OPT} ) {
    foreach ( @{ $PKG{OPT} } ) {
        if ( /^qmake$/ ) {
            my $qmake = check_qmake();
            push @dependencies, "QMAKE_BIN = $qmake";
        } else { die; }
    }
}

my %DEPEND_OPTIONS;
foreach my $href (DEPENDS()) {
    $_ = $href->{name};
    my $I = $href->{Include};
    my @L;
    my $o = "with-$_-prefix";
    ++$DEPEND_OPTIONS{$o};
    if ($OPT{$o}) {
        $OPT{$o} = expand_path($OPT{$o});
        $I = File::Spec->catdir($OPT{$o}, 'include');
        if (/^xml2$/) {
            my $t = File::Spec->catdir($I, 'libxml2');
            $I = $t if (-e $t);
        }
        push ( @L, File::Spec->catdir($OPT{$o}, 'lib') );
        push ( @L, File::Spec->catdir($OPT{$o}, 'lib64') );
    }
    my ($i, $l) = find_lib($_, $I, @L);
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
    $href->{   bldpath} = expand($href->{   bldpath}) if ($href->{   bldpath});
    $href->{locbldpath} = expand($href->{locbldpath}) if ($href->{locbldpath});

    # found directories
    my
      ($found_itf, $found_bin, $found_lib, $found_ilib, $found_jar, $found_src);

    my %a = %$href;
    next if ($a{option} && $DEPEND_OPTIONS{$a{option}});
    my $is_optional = optional($a{type});
    my $quasi_optional = $a{type} =~ /Q/;
    my $need_source = $a{type} =~ /S/;
    my $need_bin = $a{type} =~ /E/;
    my $need_build = $a{type} =~ /B/;
    my $need_lib = $a{type} =~ /L|D/;
    my $need_itf = ! ($a{type} =~ /D/ || $a{type} =~ /E/ || $a{type} =~ /J/);
    $need_itf = 1 if ($a{type} =~ /I/);
    my $need_jar = $a{type} =~ /J/;

    my ($bin, $inc, $lib, $ilib, $src)
        = ($a{bin}, $a{include}, $a{lib}, undef, $a{src}); # file names to check
    $lib = '' unless ($lib);
    $lib = expand($lib);

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
    my $tolib = $need_itf || $need_lib;
    my $tojar = $need_jar;
    foreach my $option ($a{option}, $a{boption}) {
        next unless ($option);
        if ($OPT{$option}) {
            my $try = expand_path($OPT{$option});
            if ($tojar && ! $found_jar && -f $try) {
                println "\tjar... $try" unless ($AUTORUN);
                $found_jar = $try;
            }
            elsif ($tolib) {
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
                my ($fi, $fl, $fil, $fs)
                    = find_in_dir($try, $i, $l, $il, undef, undef, $src);
                if ($fi || $fl || $fil) {
                    $found_itf  = $fi  if (! $found_itf  && $fi);
                    $found_lib  = $fl  if (! $found_lib  && $fl);
                    $found_ilib = $fil if (! $found_ilib && $fil);
                    $found_src  = $fs  if (! $found_src  && $fs);
                } elsif (! ($try =~ /$a{name}$/)) {
                    $try = File::Spec->catdir($try, $a{name});
                    ($fi, $fl, $fil, $fs)
                        = find_in_dir($try, $i, $l, $il, undef, undef, $src);
                    $found_itf  = $fi  if (! $found_itf  && $fi);
                    $found_lib  = $fl  if (! $found_lib  && $fl);
                    $found_ilib = $fil if (! $found_ilib && $fil);
                    $found_src  = $fs  if (! $found_src  && $fs);
                }
            } elsif ($need_bin) {
                my (undef, $fl, $fil)
                    = find_in_dir($try, undef, $lib, $ilib, undef, $bin);
                $found_bin = $fl if ($fl);
            }
        }
    }
    if (! $found_itf && ! $has_option{sources} && $a{srcpath}) {
        my $try = $a{srcpath};
        ($found_itf, undef, undef, $found_src)
            = find_in_dir($try, $inc, undef, undef, undef, undef, $src);
    }
    if (! $has_option{prefix}) {
        my $try = $a{pkgpath};
        if (($need_itf && ! $found_itf) || ($need_lib && ! $found_lib)) {
            my ($fi, $fl) = find_in_dir($try, $inc, $lib);
            $found_itf  = $fi  if (! $found_itf  && $fi);
            $found_lib  = $fl  if (! $found_lib  && $fl);
        }

        if ($need_jar && ! $found_jar) {
            (undef, $found_jar) = find_in_dir($try, undef, undef, undef, $lib);
        }

        $try = $a{usrpath};
        if (($need_itf && ! $found_itf) || ($need_lib && ! $found_lib)) {
            my ($fi, $fl) = find_in_dir($try, $inc, $lib);
            $found_itf  = $fi  if (! $found_itf  && $fi);
            $found_lib  = $fl  if (! $found_lib  && $fl);
        }

        if ($need_jar && ! $found_jar) {
            (undef, $found_jar) = find_in_dir($try, undef, undef, undef, $lib);
        }
    }
    if (! $has_option{build}) {
        if ($a{bldpath}) {
            my $tolib = $need_build || ($need_lib && ! $found_lib);
            my $tobin = $need_bin && ! $found_bin;
            my $tojar = $need_jar && ! $found_jar;
            if ($tolib || $tobin || $tojar) {
                my ($fl, $fil, $found);
                if ($OPT{'build-prefix'}) {
                    my $try = $OPT{'build-prefix'};
                    if ($tolib) {
                        (undef, $fl, $fil)
                            = find_in_dir($try, undef, $lib, $ilib);
                        if ($fl || $fil) {
                            $found_lib  = $fl  if (! $found_lib  && $fl);
                            $found_ilib = $fil if (! $found_ilib && $fil);
                            ++$found;
                        }
                    }
                    if ($tojar) {
                        (undef, $found_jar)
                            = find_in_dir($try, undef, undef, undef, $lib);
                    }
                    if (! ($try =~ /$a{name}$/)) {
                        $try = File::Spec->catdir($try, $a{name});
                        if ($tolib && ! $found) {
                            (undef, $fl, $fil)
                                = find_in_dir($try, undef, $lib, $ilib);
                            if ($fl || $fil) {
                                $found_lib  = $fl  if (! $found_lib  && $fl);
                                $found_ilib = $fil if (! $found_ilib && $fil);
                                ++$found;
                            }
                        }
                        if ($need_jar && ! $found_jar) {
                            (undef, $found_jar)
                                = find_in_dir($try, undef, undef, undef, $lib);
                        }
                    }
                }
                unless ($found || $fl || $fil) {
                    my $try = $a{bldpath};
                    $try = $a{locbldpath} if ($OPT{'local-build-out'});
                    if ($tolib && ! $found) {
                        (undef, $fl, $fil)
                            = find_in_dir($try, undef, $lib, $ilib);
                        my $resetLib = ! $found_lib;
                        if (! $found_ilib && $fil) {
                            $found_ilib = $fil;
                            ++$resetLib;
                        }
                        $found_lib  = $fl  if ($resetLib && $fl);
                    }
                    if ($tobin && ! $found) {
                        (undef, $fl, $fil) =
                            find_in_dir($try, undef, $lib, $ilib, undef, $bin);
                        $found_bin = $fl if ($fl);
                    }
                    if ($need_jar && ! $found_jar) {
                        (undef, $found_jar)
                            = find_in_dir($try, undef, undef, undef, $lib);
                    }
                }
            }
        }
    }
    if (($need_itf && ! $found_itf) || ($need_lib && ! $found_lib) ||
        ($need_jar && ! $found_jar) || ($ilib && ! $found_ilib) ||
        ($need_bin && ! $found_bin))
    {
        if ($is_optional) {
            println "configure: optional $a{name} package not found: skipped.";
        } elsif ($quasi_optional && $found_itf && ($need_lib && ! $found_lib)) {
            println "configure: $a{name} package: "
                . "found interface files but not libraries.";
            $found_itf = abs_path($found_itf);
            push(@dependencies, "$a{aname}_INCDIR = $found_itf");
            println "includes: $found_itf";
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
        if ($found_itf) {
            $found_itf = abs_path($found_itf);
            push(@dependencies, "$a{aname}_INCDIR = $found_itf");
            println "includes: $found_itf";
        }
        if ($found_src) {
            $found_src = abs_path($found_src);
            push(@dependencies, "$a{aname}_SRCDIR = $found_src");
            println "sources: $found_src";
        }
        if ($found_lib) {
            $found_lib = abs_path($found_lib);
            if ($a{aname} eq 'NGS' || $a{aname} eq 'VDB') {
                if ($OPT{PYTHON_LIB_PATH}) {
                    $OPT{PYTHON_LIB_PATH} .= ':';
                } else {
                    $OPT{PYTHON_LIB_PATH} = '';
                }
                $OPT{PYTHON_LIB_PATH} .= $found_lib;
            }
            push(@dependencies, "$a{aname}_LIBDIR = $found_lib");
            println "libraries: $found_lib";
        }
        if ($ilib && $found_ilib) {
            $found_ilib = abs_path($found_ilib);
            push(@dependencies, "$a{aname}_ILIBDIR = $found_ilib");
            println "ilibraries: $found_ilib";
        }
        if ($found_bin) {
            $found_bin = abs_path($found_bin);
            push(@dependencies, "$a{aname}_BINDIR = $found_bin");
            println "bin: $found_bin";
        }
        if ($found_jar) {
            $found_jar = abs_path($found_jar);
            push(@dependencies, "$a{aname}_JAR = $found_jar");
            println "jar: $found_jar";
        }
    }
}

my ($E_BINDIR, $E_LIBDIR, $E_VERSION_LIBX, $E_MAJVERS_LIBX,
                          $E_VERSION_EXEX, $E_MAJVERS_EXEX)
    = (''    , '');

println unless ($AUTORUN);

if ($OS ne 'win' && ! $OPT{'status'}) {
    if ($OSTYPE =~ /darwin/i && CONFIG_OUT() ne '.') {
        my $COMP = File::Spec->catdir(CONFIG_OUT(), 'COMP.mac');
        println "configure: creating '$COMP' ($TOOLS)" unless ($AUTORUN);
        open F, ">$COMP" or die "cannot open $COMP to write";
        print F "$TOOLS\n";
        close F;
    }

    if ($TOOLS =~ /gcc$/) {
        my $EXECMDF = File::Spec->catdir(CONFIG_OUT(), 'ld.linux.exe_cmd.sh');
        println "configure: creating '$EXECMDF'" unless ($AUTORUN);
        open F, ">$EXECMDF" or die "cannot open $EXECMDF to write";
        print F "EXE_CMD=\"\$LD $STATIC_LIBSTDCPP -static-libgcc\"\n";
        close F;
    }

    # create Makefile.config
    println "configure: creating '$OUT_MAKEFILE'" unless ($AUTORUN);
    open my $F, ">$OUT_MAKEFILE" or die "cannot open $OUT_MAKEFILE to write";

    print $F <<EndText;
### AUTO-GENERATED FILE ###

# configuration command

CONFIGURED = $CONFIGURED

OS_ARCH = \$(shell perl \$(TOP)/setup/os-arch.perl)

# install paths
EndText

    L($F, "INST_BINDIR = $OPT{'bindir'}"      ) if ($OPT{'bindir'});
    L($F, "INST_LIBDIR = $OPT{'libdir'}"      ) if ($OPT{'libdir'});
    L($F, "INST_INCDIR = $OPT{'includedir'}"  ) if ($OPT{'includedir'});
    L($F, "INST_SCHEMADIR = $OPT{'shemadir'}" ) if ($OPT{'shemadir'});
    L($F, "INST_SHAREDIR = $OPT{'sharedir'}"  ) if ($OPT{'sharedir'});
    L($F, "INST_JARDIR = $OPT{'javadir'}"     ) if ($OPT{'javadir'});
    L($F, "INST_PYTHONDIR = $OPT{'pythondir'}") if ($OPT{'pythondir'});

    my ($E_VERSION_SHLX, $VERSION_SHLX,
        $E_MAJVERS_SHLX , $MAJMIN_SHLX, $MAJVERS_SHLX);
    if ($OSTYPE =~ /darwin/i) {
        $E_VERSION_SHLX =  '$VERSION.$SHLX';
        $VERSION_SHLX = '$(VERSION).$(SHLX)';
        $MAJMIN_SHLX  = '$(MAJMIN).$(SHLX)';
        $E_MAJVERS_SHLX = '$MAJVERS.$SHLX';
        $MAJVERS_SHLX = '$(MAJVERS).$(SHLX)';
    } else {
        $E_VERSION_SHLX =  '$SHLX.$VERSION';
        $VERSION_SHLX = '$(SHLX).$(VERSION)';
        $MAJMIN_SHLX  = '$(SHLX).$(MAJMIN)';
        $E_MAJVERS_SHLX =  '$SHLX.$MAJVERS';
        $MAJVERS_SHLX = '$(SHLX).$(MAJVERS)';
    }

    $E_VERSION_LIBX = '$LIBX.$VERSION';
    $E_MAJVERS_LIBX = '$LIBX.$MAJVERS';

    L($F);
    L($F, "# build type");

    if ($OPT{'enable-static'}) {
        L($F, "WANTS_STATIC = 1");
    }

    $E_VERSION_EXEX = '$EXEX.$VERSION';
    $E_MAJVERS_EXEX = '$LIBX.$MAJVERS';

    print $F <<EndText;
BUILD = $BUILD

# target OS
OS    = $OS
OSINC = $OSINC
OS_DISTRIBUTOR = $OS_DISTRIBUTOR

# prefix string for system libraries
LPFX = $LPFX

# suffix strings for system libraries
LIBX = $LIBX
VERSION_LIBX = \$(LIBX).\$(VERSION)
MAJMIN_LIBX  = \$(LIBX).\$(MAJMIN)
MAJVERS_LIBX = \$(LIBX).\$(MAJVERS)

SHLX         = $SHLX
VERSION_SHLX = $VERSION_SHLX
MAJMIN_SHLX  = $MAJMIN_SHLX
MAJVERS_SHLX = $MAJVERS_SHLX

# suffix strings for system object files
OBJX = $OBJX
LOBX = $LOBX

# suffix string for system executable
EXEX         = $EXEX
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

    L($F, "CC            = $CC"           ) if ($CC);
    L($F, "CPP           = $CPP"          ) if ($CPP);
    L($F, "CP            = $CP"           ) if ($CP);
    L($F, "AR            = $AR"           ) if ($AR);
    L($F, "ARX           = $ARX"          ) if ($ARX);
    L($F, "ARLS          = $ARLS"         ) if ($ARLS);
    L($F, "LD            = $LD"           ) if ($LD);
    L($F, "LP            = $LP"           ) if ($LP);
    L($F, "PYTHON        = $PYTHON"       ) if ($PYTHON);
    L($F, "JAVAC         = $JAVAC"        ) if ($JAVAC);
    L($F, "JAVAH         = $JAVAH"        ) if ($JAVAH);
    L($F, "JAR           = $JAR"          ) if ($JAR);
    L($F, "MAKE_MANIFEST = $MAKE_MANIFEST") if ($MAKE_MANIFEST);
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
   L($F, 'SONAME  = -install_name ' .
               '$(INST_LIBDIR)$(BITS)/$(subst $(VERSION),$(MAJVERS),$(@F)) \\');
   L($F, '    -compatibility_version $(MAJMIN) -current_version $(VERSION) \\');
   L($F, '    -flat_namespace -undefined suppress');
        } else {
      L($F, 'SONAME = -Wl,-soname=$(subst $(VERSION),$(MAJVERS),$(@F))');
     }
     L($F, "SRCINC  = $INC. $INC\$(SRCDIR)");
    } elsif ($PKG{LNG} eq 'JAVA') {
        L($F, 'SRCINC  = -sourcepath $(INCPATHS)');
    }
    if ($PIC) {
        if (PACKAGE_NAMW() eq 'NGS') {
            L($F, "INCDIRS = \$(SRCINC) $INC\$(TOP) "
                .        "$INC\$(TOP)/ngs/\$(OSINC)/\$(ARCH)")
        } elsif (PACKAGE_NAMW() eq 'NGS_BAM') {
            L($F, "INCDIRS = \$(SRCINC) $INC\$(TOP) "
                . "$INC\$(NGS_INCDIR)/ngs/\$(OSINC)/\$(ARCH)")
        } else {
            L($F, "INCDIRS = \$(SRCINC) $INC\$(TOP)")
        }
    }
    if ($PKG{LNG} eq 'C') {
        L($F, "CFLAGS  = \$(DBG) \$(OPT) \$(INCDIRS) $MD $ARCH_FL");
    }
    L($F, "LDFLAGS = $LDFLAGS") if ($LDFLAGS);

    L($F, 'CLSPATH = -classpath $(CLSDIR)');
    L($F, "NO_ARRAY_BOUNDS_WARNING = $NO_ARRAY_BOUNDS_WARNING");
    L($F);

# $PACKAGE_NAME and library version
# \$(VERSION) is defined in a separate file which is updated every release
    L($F, "include \$(TOP)/" . CONFIG_OUT() . "/Makefile.vers" );

    print $F <<EndText;

empty :=
space := \$(empty) \$(empty)
MAJMIN  = \$(subst \$(space),.,\$(wordlist 1,2,\$(subst .,\$(space),\$(VERSION))))
MAJVERS = \$(firstword \$(subst .,\$(space),\$(VERSION)))

# output path
BUILD_PREFIX = $BUILD_PREFIX
TARGDIR = $TARGDIR

# derived paths
MODPATH  ?= \$(subst \$(TOP)/,,\$(CURDIR))
SRCDIR   ?= \$(TOP)/\$(MODPATH)
MAKEFILE ?= \$(abspath \$(firstword \$(MAKEFILE_LIST)))
BINDIR    = \$(TARGDIR)/bin
EndText

    if ($PKG{LNG} eq 'C') {
        $E_BINDIR        = '$TARGDIR/bin';
        $E_LIBDIR        = '$TARGDIR/lib';
        L($F, 'LIBDIR    = $(TARGDIR)/lib');
    } elsif ($PKG{LNG} eq 'JAVA') {
        $E_LIBDIR        = '$TARGDIR/jar';
        L($F, 'LIBDIR    = $(TARGDIR)/jar');
    }

    L($F, 'ILIBDIR   = $(TARGDIR)/ilib');
    if ($PKG{NOMODPATH}) {
        L($F, 'OBJDIR    = $(TARGDIR)/obj');
    } else {
        L($F, 'OBJDIR    = $(TARGDIR)/obj/$(MODPATH)');
    }
    L($F, 'CLSDIR    = $(TARGDIR)/cls');

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
    T($F, '$(CP) -std=c++11 -o $@ $< $(CFLAGS)');
    L($F, '$(OBJDIR)/%.$(LOBX): %.cpp');
    T($F, '$(CP) -std=c++11 -o $@ $< $(PIC) $(CFLAGS)');
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

    # pass HAVE_XML2 to build scripts
    L($F, 'ifeq (,$(HAVE_XML2))');
    L($F, '    HAVE_XML2=0');
    L($F, 'endif');
    L($F, 'CONFIGURE_FOUND_XML2=$(HAVE_XML2)');
    L($F, 'export CONFIGURE_FOUND_XML2');
    L($F);

    if ($OS eq 'linux' || $OS eq 'mac') {
        L($F, '# installation rules');
        L($F,
        '$(INST_LIBDIR)$(BITS)/%.$(VERSION_LIBX): $(LIBDIR)/%.$(VERSION_LIBX)');
        T($F, '@ echo -n "installing \'$(@F)\'... "');
        T($F, '@ if cp $^ $@ && chmod 644 $@;                         \\');
        T($F, '  then                                                 \\');
        T($F, '      rm -f $(patsubst %$(VERSION),%$(MAJVERS),$@) '
                     . '$(patsubst %$(VERSION_LIBX),%$(LIBX),$@) '
                     . '$(patsubst %.$(VERSION_LIBX),%-static.$(LIBX),$@); \\');
        T($F, '      ln -s $(@F) $(patsubst %$(VERSION),%$(MAJVERS),$@);   \\');
        T($F, '      ln -s $(patsubst %$(VERSION),%$(MAJVERS),$(@F)) '
                      . '$(patsubst %$(VERSION_LIBX),%$(LIBX),$@); \\');
        T($F, '      ln -s $(patsubst %$(VERSION_LIBX),%$(LIBX),$(@F)) ' .
   '$(INST_LIBDIR)$(BITS)/$(patsubst %.$(VERSION_LIBX),%-static.$(LIBX),$(@F));'
                                                              . ' \\');
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
        if ($OS ne 'mac') {
          T($F, '      rm -f $(patsubst %$(VERSION),%$(MAJVERS),$@) '
                      . '$(patsubst %$(VERSION_SHLX),%$(SHLX),$@);    \\');
        }
        if ($OS eq 'linux') {
          T($F, '      ln -s $(@F) $(patsubst %$(VERSION),%$(MAJVERS),$@); \\');
        } elsif ($OS eq 'mac') {
          T($F, '      ln -sf $(@F) '
                   . '$(patsubst %$(VERSION_SHLX),%$(MAJVERS).$(SHLX),$@); \\');
        } else {
          die;
        }
        T($F, '      ln -sf $(patsubst %$(VERSION),%$(MAJVERS),$(@F)) '
                      . '$(patsubst %$(VERSION_SHLX),%$(SHLX),$@); \\');
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
        T($F, '      rm -f $(patsubst %$(VERSION),%$(MAJVERS),$@) '
                      . '$(patsubst %$(VERSION_EXEX),%$(EXEX),$@);     \\');
        T($F, '      ln -s $(@F) $(patsubst %$(VERSION),%$(MAJVERS),$@);   \\');
        T($F, '      ln -s $(patsubst %$(VERSION),%$(MAJVERS),$(@F)) '
                      . '$(patsubst %$(VERSION_EXEX),%$(EXEX),$@); \\');
        T($F, '      echo success;                                    \\');
        T($F, '  else                                                 \\');
        T($F, '      echo failure;                                    \\');
        T($F, '      false;                                           \\');
        T($F, '  fi');
    }
    close $F;

	# creation of Makefile.config.install is disabled, since nobody uses it now 
	# and I need to remove versions from prl scripts
	if (0) {
	    # create Makefile.config.install
	    println "configure: creating '$INS_MAKEFILE'" unless ($AUTORUN);
	    open $F, ">$INS_MAKEFILE" or die "cannot open $INS_MAKEFILE to write";
	
	    $OPT{'javadir' } = '' unless ($OPT{'javadir' });
	    $OPT{'sharedir'} = '' unless ($OPT{'sharedir'});
	
	    print $F "sub CONFIGURE {\n";
	    print $F "    \$_{PACKAGE_NAME } = '$PACKAGE_NAME';\n";
	    print $F "    \$_{VERSION      } = '\$VERSION';\n";
	    print $F "    \$_{LNG          } = '$PKG{LNG}';\n";
	    print $F "    \$_{OS           } = '$OS';\n";
	    print $F "    \$_{BITS         } =  $BITS;\n";
	    print $F "    \$_{MAJVERS      } =  \$MAJVERS;\n";
	    print $F "    \$_{LPFX         } = '$LPFX';\n";
	    print $F "    \$_{LIBX         } = '$LIBX';\n";
	    print $F "    \$_{MAJVERS_LIBX } = '" . expand($E_MAJVERS_LIBX) . "';\n";
	    print $F "    \$_{VERSION_LIBX } = '" . expand($E_VERSION_LIBX) . "';\n";
	    print $F "    \$_{SHLX         } = '$SHLX';\n";
	    print $F "    \$_{MAJVERS_SHLX } = '" . expand($E_MAJVERS_SHLX) . "';\n";
	    print $F "    \$_{VERSION_SHLX } = '" . expand($E_VERSION_SHLX) . "';\n";
	    print $F "    \$_{VERSION_EXEX } = '" . expand($E_VERSION_EXEX) . "';\n";
	    print $F "    \$_{MAJVERS_EXEX } = '" . expand($E_MAJVERS_EXEX) . "';\n";
	    print $F "    \$_{INCDIR       } = '" . expand("$Bin/.."      ) . "';\n";
	    if ($PKG{LNG} ne 'PYTHON') {
	        print $F "  \$_{BINDIR$BITS} = '" . expand($E_BINDIR      ) . "';\n";
	        print $F "  \$_{LIBDIR$BITS} = '" . expand($E_LIBDIR      ) . "';\n";
	    } elsif ($OPT{PYTHON_LIB_PATH}) {
	        print $F "  \$_{LIBDIR$BITS} = '$OPT{PYTHON_LIB_PATH}';\n";
	    }
	    print $F "    \$_{OTHER_PREFIX } = '$PKG{UPATH}';\n";
	    print $F "    \$_{PREFIX       } = '$OPT{'prefix'}';\n";
	    print $F "    \$_{INST_INCDIR  } = '$OPT{'includedir'}';\n";
	    print $F "    \$_{INST_BINDIR  } = '$OPT{'bindir'}';\n";
	    print $F "    \$_{INST_LIBDIR  } = '$OPT{'libdir'}';\n";
	    print $F "    \$_{INST_JARDIR  } = '$OPT{'javadir'}';\n";
	    print $F "    \$_{INST_SHAREDIR} = '$OPT{'sharedir'}';\n";
	    print $F "\n";
	    print $F "    \@_\n";
	    print $F "}\n";
	    print $F "\n";
	    print $F "1\n";
	
	    close $F;
    }
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
                my $root = "$a{aname}_ROOT";
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
        print COUT "OS_ARCH = \$(shell perl \$(TOP)/setup/os-arch.perl)\n";
        print COUT "include \$(TOP)/$CONFIG_OUT/Makefile.config.\$(OS_ARCH)\n";
        close COUT;
    }
}

unless ($OPT{'reconfigure'}) {
    println "configure: creating 'reconfigure'" unless ($AUTORUN);
    $CONFIGURED =~ s/\t/ /g;
    open my $F, '>reconfigure' or die 'cannot open reconfigure to write';
    print $F "./configure $CONFIGURED\n";
    close $F;
   # my $perm = (stat $fh)[2] & 07777;
#   print "==================================================== $perm\n";
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
            } elsif (/BUILD_PREFIX = /) {
                $BUILD_PREFIX = $_;
            } elsif (/^CC += (.+)/) {
                $CC = $1;
            } elsif (/CONFIGURED = (.*)/) {
                $CONFIGURED = $1;
            } elsif (/CPP += (.+)/) {
                $CPP = $1;
            } elsif (/LDFLAGS += (.+)/) {
                $LDFLAGS = $1;
            } elsif (/TARGDIR = /) {
                $TARGDIR = $_;
                println "\t\tgot $_" if ($OPT{'debug'});
            } elsif (/TARGDIR \?= (.+)/) {
                $TARGDIR = $1 unless ($TARGDIR);
                println "\t\tgot $_" if ($OPT{'debug'});
            }
            elsif (/INST_INCDIR = (.+)/) {
                $OPT{'includedir'} = $1;
            }
            elsif (/INST_BINDIR = (.+)/) {
                $OPT{'bindir'} = $1;
            }
            elsif (/INST_LIBDIR = (.+)/) {
                $OPT{'libdir'} = $1;
            }
        }
    }

    println "build type: $BUILD_TYPE";
    println "build prefix: $BUILD_PREFIX" if ($OS ne 'win');
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

    println "CC = $CC"   if ($CC );
    println "CPP = $CPP" if ($CPP);
    println "LDFLAGS = $LDFLAGS" if ($LDFLAGS);

    $CONFIGURED =~ s/\t/ /g;
    println "configured with: \"$CONFIGURED\"";
}

sub expand { $_[0] =~ s/(\$\w+)/$1/eeg; $_[0]; }

sub expand_path {
    my ($filename) = @_;
    return unless ($filename);

    if ($filename =~ /^~/) {
        if ($filename =~ m|^~([^/]*)|) {
            if ($1 && ! getpwnam($1)) {
                print "configure: error: bad path: '$filename'\n";
                exit 1;
            }
        }

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
    my ($dir, $include, $lib, $ilib, $jar, $bin, $src) = @_;
    unless (-d $dir) {
        println "\t\tnot found $dir" if ($OPT{'debug'});
        return;
    }
    my ($found_inc, $found_lib, $found_ilib, $found_src);
    if ($include) {
        print "\tincludes... " unless ($AUTORUN);
        if (-e "$dir/$include") {
            println $dir unless ($AUTORUN);
            $found_inc = $dir;
        } elsif (-e "$dir/include/$include") {
            println $dir unless ($AUTORUN);
            $found_inc = "$dir/include";
        } elsif (-e "$dir/interfaces/$include") {
            println $dir unless ($AUTORUN);
            $found_inc = "$dir/interfaces";
        } else {
            print "$dir: " if ($OPT{'debug'});
            println 'no' unless ($AUTORUN);
        }
    }
    if ($lib || $ilib) {
        print "\tlibraries... " unless ($AUTORUN);
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
                        println $ilibdir;
                        $found_ilib = $ilibdir;
                    } else {
                        println 'no' unless ($AUTORUN);
                        return;
                    }
                } else {
                    println $libdir;
                }
                ++$found;
            }
            if (! $found) {
                my $libdir = File::Spec->catdir($dir, 'lib' . $BITS);
                my $f = File::Spec->catdir($libdir, $lib);
                print "\tchecking $f\n\t" if ($OPT{'debug'});
                if (-e $f) {
                    println $libdir;
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
                            println $ilibdir;
                            $found_ilib = $ilibdir;
                        } else {
                            println 'no' unless ($AUTORUN);
                            return;
                        }
                    } else {
                        println $libdir;
                    }
                    ++$found;
                } else {
                    println 'no' unless ($AUTORUN);
                }
            }
        }
        if ($found_lib && $ilib && ! $found_ilib) {
            println "\n\t\tfound $found_lib but no ilib/" if ($OPT{'debug'});
            print "\t" if ($OPT{'debug'});
            println 'no' unless ($AUTORUN);
            undef $found_lib;
        }
    }
    if ($bin) {
        print "\t... " unless ($AUTORUN);
        my $builddir = File::Spec->catdir($dir, $OS, $TOOLS, $ARCH, $BUILD);
        my $bdir  = File::Spec->catdir($builddir, 'bin');
        my $f = File::Spec->catdir($bdir, $bin);
        print "\n\t\tchecking $f\n\t" if ($OPT{'debug'});
        if (-e $f) {
            $found_lib = $bdir;
            println $bdir;
        } else {
            println 'no' unless ($AUTORUN);
        }
    }
    if ($jar) {
        print "\tjar... " unless ($AUTORUN);
        my $try = "$dir/jar/$jar";
        if (-e "$try") {
            println $try unless ($AUTORUN);
            $found_lib = $try;
        }
    }
    if ($src) {
        print "\tsrc... " unless ($AUTORUN);
        my $try = "$dir/$src";
        if (-e "$try") {
            println $dir unless ($AUTORUN);
            $found_src = $dir;
        }
    }
    return ($found_inc, $found_lib, $found_ilib, $found_src);
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

sub check_tool_h  { return check_tool(@_,  '-help'); }
sub check_tool__h { return check_tool(@_, '--help'); }

sub check_tool {
    my ($tool, $o) = @_;
    print "checking for $tool... ";
    my $cmd = "$tool $o";
    print "\n\t\trunning $cmd\n\t" if ($OPT{'debug'});
    my $out = `$cmd 2>&1`;
    if ($? == 0) {
        println "yes";
        return 1;
    } else {
        println "no";
        return 0;
    }
}

sub check_qmake {
    print "checking for QMake... ";

    my $tool = 'qmake';
    print "\n\t\trunning $tool... " if ($OPT{'debug'});
    my $out = `$tool -v 2>&1`;
    if ($? == 0) {
        my $out = `( $tool -v | grep QMake ) 2>&1`;
        if ($? == 0) {
            print "$out " if ($OPT{'debug'});
            println $tool;
            return $tool;
        }

        println "wrong qmake" if ($OPT{'debug'});

        print "\t\tchecking $ENV{PATH}...\n" if ($OPT{'debug'});
        foreach ( split(/:/, $ENV{PATH})) {
            my $cmd = "$_/$tool";
            print "\t\trunning $cmd... " if ($OPT{'debug'});
            my $out = `( $cmd -v | grep QMake ) 2>&1`;
            if ($? == 0) {
                print "$out " if ($OPT{'debug'});
                if ( $out =~ /QMake/ ) {
                    println $cmd;
                    return $cmd;
                }
            }

            println "no" if ($OPT{'debug'});
        }
    }

    if ( $OS eq 'linux' ) {
        if ( $OS_DISTRIBUTOR eq 'CentOS' ) {
            foreach ( glob ( "$ENV{HOME}/Qt/*/gcc_64" ) ) {
                $tool =  "$_/bin/qmake";
                print "\n\t\tchecking $tool... " if ($OPT{'debug'});
                my $out = `( $tool -v | grep QMake ) 2>&1`;
                if ($? == 0) {
                    print "$out " if ($OPT{'debug'});
                    println $tool;
                    return $tool;
                }
            }

            $tool = '/usr/lib64/qt5/bin/qmake';

        } elsif ( $OS_DISTRIBUTOR eq 'Ubuntu' ) {
            foreach ( glob ( "$ENV{HOME}/Qt*/*/gcc_64" ) ) {
                $tool =  "$_/bin/qmake";
                print "\n\t\tchecking $tool... " if ($OPT{'debug'});
                my $out = `( $tool -v | grep QMake ) 2>&1`;
                if ($? == 0) {
                    print "$out " if ($OPT{'debug'});
                    println $tool;
                    return $tool;
                }
            }

            $tool = '';
        }
    } elsif ( $OS eq 'mac' ) {
        $tool = '/Applications/QT/5.10.1/clang_64/bin/qmake';
    }

    if ( $tool ) {
        print "\n\t\tchecking $tool... " if ($OPT{'debug'});
        my $out = `( $tool -v | grep QMake ) 2>&1`;
        if ($? == 0) {
            print "$out " if ($OPT{'debug'});
            println $tool;
            return $tool;
        }
    }

    println "no";
    return '';
}

sub check_static_libstdcpp {
    my $option = '-static-libstdc++';

    print "checking whether $CPP accepts $option... ";

    my $log = 'int main() {}\n';
    my $cmd = $log;
    $cmd =~ s/\\n/\n/g;
    my $gcc = "echo -e '$log' | $CPP -xc $option - 2>&1";
    print "\n\t\trunning $gcc\n" if ($OPT{'debug'});
    my $out = `$gcc`;
    my $ok = $? == 0;
    if ( $ok && $out ) {
        $ok = 0 if ( $out =~ /unrecognized option '-static-libstdc\+\+'/ );
    }
    print "$out\t" if ($OPT{'debug'});
    println $ok ? 'yes' : 'no';

    unlink 'a.out';

    return '' if (!$ok);

    return $option;
}

sub check_no_array_bounds {
    check_compiler('O', '-Wno-array-bounds');
}

sub find_lib {
    check_compiler('L', @_);
}

sub check_compiler {
    my ($t, $n, $I, @l) = @_;
    my $tool = $TOOLS;

    if ($t eq 'L') {
        print "checking for $n library... ";
    } elsif ($t eq 'O') {
        if ($tool && ($tool =~ /gcc$/ || $tool =~ /g\+\+$/)) {
            print "checking whether $tool accepts $n... ";
        } else {
            return;
        }
    } else {
        die "Unknown check_compiler option: '$t'";
    }

    unless ($tool) {
        println "warning: unknown tool";
        return;
    }

    while (1) {
        my ($flags, $library, $log) = ('', '');

        if ($t eq 'O') {
            $flags = $n;
            $log = '                      int main() {                     }\n'
        } elsif ($n eq 'hdf5') {
            $library = '-Wl,-Bstatic -lhdf5 -Wl,-Bdynamic -ldl -lm -lz';
            $log = '#include <hdf5.h>  \n int main() { H5close         (); }\n'
        } elsif ($n eq 'fuse') {
            $flags = '-D_FILE_OFFSET_BITS=64';
            $library = '-lfuse';
            $log = '#include <fuse.h>  \n int main() { fuse_get_context(); }\n'
        } elsif ($n eq 'magic') {
            $library = '-lmagic';
            $log = '#include <magic.h> \n int main() { magic_open     (0); }\n'
        } elsif ($n eq 'xml2') {
            $library  = '-lxml2';
            $library .=       ' -liconv' if ($OS eq 'mac');
            $log = '#include <libxml/xmlreader.h>\n' .
                                         'int main() { xmlInitParser  ( ); }\n'
        } else {
            println 'unknown: skipped';
            return;
        }

        if ($I && ! -d $I) {
            print "'$I': " if ($OPT{'debug'});
            println 'no';
            return;
        }

        for ( my $i = 0; $i <= $#l; ++ $i ) {
            print "'$l[$i]': " if ($OPT{'debug'});
            if ( $l [ $i ] ) {
                if ( -d $l [ $i ] ) {
                    last;
                } elsif ( $i ==  $#l ) {
                    println 'no';
                    return;
                }
            }
        }

        my $cmd = $log;
        $cmd =~ s/\\n/\n/g;

        push ( @l, '' ) unless ( @l );
        for my $i ( 0 .. $#l ) {
            my $l = $l [ $i ];
            if ( $l && ! -d $l ) {
                if ( $i == $#l ) {
                    println 'no';
                    return;
                } else {
                    next;
                }
            }
            my $gcc = "| $tool -xc $flags " . ($I ? "-I$I " : ' ')
                                      . ($l ? "-L$l " : ' ') . "- $library";
            $gcc .= ' 2> /dev/null' unless ($OPT{'debug'});

            open GCC, $gcc or last;
            print "\n\t\trunning echo -e '$log' $gcc\n" if ($OPT{'debug'});
            print GCC "$cmd" or last;
            my $ok = close GCC;
            print "\t" if ($OPT{'debug'});
            if ( $ok ) {
                println 'yes';
            } else {
                println 'no' if ( $i == $#l );
            }

            unlink 'a.out';

            return if ( ! $ok && ( $i == $#l ) );

            return 1 if ($t eq 'O');

            return ($I, $l) if ( $ok) ;
        }
    }

    println "cannot run $tool: skipped";
}

################################################################################

sub check {
    die "No CONFIG_OUT"   unless CONFIG_OUT();
    die "No PACKAGE"      unless PACKAGE();
    die "No PACKAGE_NAME" unless PACKAGE_NAME();
    die "No PACKAGE_NAMW" unless PACKAGE_NAMW();
    die "No PACKAGE_TYPE" unless PACKAGE_TYPE();

    my %PKG = PKG();

    die "No LNG"    unless $PKG{LNG};
    die "No LOCOUT" unless $PKG{LOCOUT};
    die "No OUT"    unless $PKG{OUT};
    die "No PATH"   unless $PKG{PATH};
    die "No UPATH"  unless $PKG{UPATH};

    foreach my $href (DEPENDS()) { die "No DEPENDS::name" unless $href->{name} }

    foreach my $href (REQ()) {
        die         "No REQ::name" unless $href->{name};

        die         "No $href->{name}:option"  unless $href->{option}
                                                   || $href->{boption};

        die         "No $href->{name}:type"    unless $href->{type};
        unless ($href->{type} =~ /I/) {
          unless ($href->{type} =~ /E/) {
            die     "No $href->{name}:lib"     unless $href->{lib};
          }
            die     "No $href->{name}:pkgpath" unless $href->{pkgpath};
            die     "No $href->{name}:usrpath" unless $href->{usrpath};
        }

        die         "No $href->{name}:origin"  unless $href->{origin};
        if ($href->{origin} eq 'I') {
            die     "No $href->{name}:aname"   unless $href->{aname};
            unless ($href->{type} =~ /D/ || $href->{type} =~ /E/
                                         || $href->{type} =~ /J/)
            {
                die "No $href->{name}:include" unless $href->{include};
                die "No $href->{name}:srcpath" unless $href->{srcpath};
            }
            unless ($href->{type} =~ /I/) {
                die "No $href->{name}:bldpath"    unless $href->{bldpath   };
                die "No $href->{name}:locbldpath" unless $href->{locbldpath};
            }
            if ($href->{type} =~ /B/) {
                die "No $href->{name}:ilib"    unless $href->{ilib};
            }
        }
    }
}

################################################################################

sub optional { $_[0] =~ /O/ }

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
        } elsif (PACKAGE_TYPE() eq 'L') {
            print "\`$package_default_prefix/include', ";
        }
        if (PACKAGE_TYPE() eq 'P') {
            println "\`$package_default_prefix/share' etc.";
        } else {
            println "\`$package_default_prefix/lib' etc.";
        }

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
        if (optional($href->{type})) {
            ++$optional;
        } else {
            ++$required;
        }
    }

    if ($required) {
        print "Required Packages:\n";
        foreach my $href (@REQ) {
            next if (optional($href->{type}));
            my %a = %$href;
            if ($a{type} =~ /S/) {
                println "  --$a{option}=DIR    search for $a{name} package";
                println "                                 source files in DIR";
            } else {
                unless ($a{type} =~ /E/) {
                  println
                    "  --$a{option}=DIR      search for $a{name} package in DIR"
                }
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
            next unless (optional($href->{type}));
            my %a = %$href;
            if ($a{option} && $a{option} =~ /-sources$/) {
                println "  --$a{option}=DIR    search for $a{name} package";
                println "                                source files in DIR";
            } elsif ($a{boption} && $a{boption} =~ /-build$/) {
                println "  --$a{boption}=DIR     search for $a{name} package";
                println "                                 build output in DIR";
            } else {
                println "  --$a{option}=DIR    search for $a{name} files in DIR"
            }
        }
        println;
    }

    print <<EndText if (PACKAGE_TYPE() eq 'B');
Optional Features:
  --enable-static         build static executable [default=no]

EndText

    my ($OS, $ARCH, $OSTYPE, $MARCH, @ARCHITECTURES) = OsArch();

    if ($^O ne 'MSWin32') {
        print "Build tuning:\n";
        if ($PKG{LNG} ne 'JAVA') {
            print <<EndText;
  --with-debug
  --without-debug
EndText
        }

        if (@ARCHITECTURES) {
            print
"  --arch=name             specify the name of the target architecture\n";
        }

        if ($PKG{LNG} eq 'JAVA') {
            print <<EndText;
  --source=release        provide source compatibility with specified release,
                          generate class files for specified VM version.
                          e.g. `--source=1.6'
EndText
        } else {
            print "\n";
        }

        print <<EndText;
  --build-prefix=DIR      generate build output into DIR directory
                          [$OUTDIR]

EndText
    }

    println 'Miscellaneous:';
    println '  --reconfigure           rerun `configure\'';
    println '                          using the same command-line arguments';
    if ($^O ne 'MSWin32') {
        println
            '  --status                print current configuration information'
    }
    print <<EndText;
  --clean                 remove all configuration results
  --debug                 print lots of debugging information

If `configure' was already run running `configure' without options
will rerun `configure' using the same command-line arguments.

Report bugs to sra-tools\@ncbi.nlm.nih.gov
EndText
}

################################################################################
