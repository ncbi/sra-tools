################################################################################

use strict;

require 'install.prl';

use Config;
use Cwd        "abs_path";
use File::Copy "copy";
use File::Copy::Recursive qw(dircopy);
use File::Path   "make_path";
use FindBin    qw($Bin);
use Getopt::Long "GetOptions";

my ($OS, $MAKING, %INSTALLED_LIBS);
{
    my $file = 'os.prl';
    if (-e $file) {
        require $file;
        $OS = OS();
    } else {
        ++$MAKING;
    }
}

my %HAVE = HAVE();
BINS() if ($HAVE{BINS});
if ($HAVE{LIBS}) {
    ++$HAVE{INCLUDES};
    LIBS();
}
if ($HAVE{INCLUDES} || $HAVE{USR_INCLUDES}) {
    die "no INCLUDES" unless INCLUDES();
}
die "no CONFIG_OUT" unless CONFIG_OUT();

my @bits;
my @options = ( 'debug', 'examplesdir=s', 'force', 'help',
                'includedir=s', 'no-create', 'prefix=s', 'root=s', );
push @options, 'oldincludedir=s' if ($HAVE{USR_INCLUDES});
if ($HAVE{JAR}) {
    push @options, 'jardir=s';
    if (-e "$Bin/../jar") {
        ++$HAVE{LIBS};
        $_{JARDIR} = expand_path("$Bin/../jar");
    }
} elsif ($HAVE{PYTHON} && ! $MAKING) {
    ++$HAVE{LIBS};
}
if (! $MAKING && ($HAVE{JAR} || $HAVE{PYTHON})) {
    ++$HAVE{TWO_LIBS};
    push @options, 'ngslibdir=s', 'vdblibdir=s';
}
push @options, 'bindir=s'                     if ($HAVE{BINS});
push @options, 'bits=s' => \@bits, 'libdir=s' if ($HAVE{LIBS});

my %OPT;
unless (GetOptions(\%OPT, @options)) {
    print "install: error\n";
    exit 1;
}
@bits = split(/,/,join(',',@bits));
foreach (@bits) {
    unless (/^32$/ || /^64$/) {
        print "install: error: bad bits option argument value\n";
        exit 1;
    }
}
if ($#bits > 0) {
    foreach (qw(bindir libdir ngslibdir vdblibdir)) {
        if ($OPT{$_}) {
            print "install: error: cannot supply multiple bits arguments "
                . "when $_ argument is provided\n";
            exit 1;
        }
    }
}

$OPT{root} = expand_path($OPT{root}) if ($OPT{root});

prepare();

my $LINUX_ROOT;
++$LINUX_ROOT if (linux_root());
my $ROOT = '';
if ($OPT{root}) {
    $ROOT = "$OPT{root}/root";
    ++$LINUX_ROOT;
    foreach ("$ROOT/usr/include", "$ROOT/etc/profile.d") {
        unless (-e $_) {
            print "mkdir -p $_... ";
            eval { make_path($_) };
            if ($@) {
                print "failure: $@\n";
                exit 1;
            }
            print "ok\n";
        }
    }
}

my $oldincludedir = "$ROOT/usr/include";

my $EXAMPLES_DIR = "$Bin/../examples";

@_ = CONFIGURE();

if ($OPT{help}) {
    help();
    exit 0;
}

foreach (qw(BITS INCDIR
 INST_INCDIR INST_JARDIR INST_LIBDIR INST_NGSLIBDIR INST_SHAREDIR INST_VDBLIBDIR
 LIBX LPFX MAJVERS MAJVERS_SHLX OS OTHER_PREFIX
 PACKAGE_NAME PREFIX SHLX VERSION VERSION_LIBX VERSION_SHLX))
{
    unless ($_{$_}) {
        next if (/^INST_JARDIR$/    && ! $HAVE{JAR});
        next if (/^INST_NGSLIBDIR$/ && ! $HAVE{TWO_LIBS});
        next if (/^INST_SHAREDIR$/  && ! $HAVE{EXAMPLES});
        next if (/^INST_VDBLIBDIR$/ && ! $HAVE{TWO_LIBS});
        fatal_config("$_ not found");
    }
}
unless ($_{LIBDIR32} || $_{LIBDIR64} || ($HAVE{PYTHON} && $MAKING)) {
    fatal_config('LIBDIR not found');
}
 
if ($OPT{prefix}) {
    $OPT{prefix} = expand_path($OPT{prefix});
    $_{INST_BINDIR  } = "$OPT{prefix}/bin";
    $_{INST_LIBDIR  } = "$OPT{prefix}/lib";
    $_{INST_NGSLIBDIR} = $_{INST_VDBLIBDIR} = $_{INST_LIBDIR};
    $_{INST_INCDIR  } = "$OPT{prefix}/include";
    $_{INST_JARDIR  } = "$OPT{prefix}/jar";
    $_{INST_SHAREDIR} = "$OPT{prefix}/share";
}
$_{INST_SHAREDIR} = expand_path($OPT{examplesdir  }) if ($OPT{examplesdir  });
$_{INST_INCDIR  } = expand_path($OPT{includedir   }) if ($OPT{includedir   });
$_{INST_JARDIR  } = expand_path($OPT{jardir       }) if ($OPT{jardir       });
$_{BIN_TARGET   } = expand_path($OPT{bindir       }) if ($OPT{bindir       });
$oldincludedir    = expand_path($OPT{oldincludedir}) if ($OPT{oldincludedir});
if ($OPT{libdir}) {
    $_{INST_NGSLIBDIR} = $_{LIB_TARGET} = expand_path($OPT{libdir}) ;
    $_{INST_VDBLIBDIR} = $_{LIB_TARGET};
}
$_{INST_NGSLIBDIR}= expand_path($OPT{ngslibdir}) if ($OPT{ngslibdir});
$_{INST_VDBLIBDIR}= expand_path($OPT{vdblibdir}) if ($OPT{vdblibdir});

if ($OPT{'no-create'} && $_{OS} eq 'linux') {
    if ($LINUX_ROOT) {
        print "root user\n\n";
    } else {
        print "non root user\n\n";
    }
}

my $failures = 0;
my $bFailure = 1;

push @bits, $_{BITS} unless (@bits);
foreach (@bits) {
    $_{BITS} = $_;

    print "installing $_{PACKAGE_NAME} ($_{VERSION}) package";
    print " for $_{OS}-$_{BITS}" if ($HAVE{BINS} || $HAVE{LIBS});
    print "...\n";

    if ($HAVE{BINS}) {
        $_{BINDIR} = $_{"BINDIR$_{BITS}"};
        unless ($_{BINDIR}) {
            print "install: error: $_{BITS}-bit version is not available\n\n";
            next;
        }
    }
    if ($HAVE{LIBS} || $HAVE{PYTHON}) {
# ($_{LIBDIR} for python points where ngs-sdk and ncbi-vdb dynamic libraries
# can be found to correctly set up LD_LIBRARY_PATH
        $_{LIBDIR} = $_{"LIBDIR$_{BITS}"};
        unless ($_{LIBDIR}) {
            print "install: error: $_{BITS}-bit version is not available\n\n";
            next;
        }
    }
    if ($HAVE{JAR} && ! $_{JARDIR}) {
        $_{JARDIR} = $_{"LIBDIR$_{BITS}"};
        unless ($_{JARDIR}) {
            if ($_{BITS} == 64) {
                $_{JARDIR} = $_{LIBDIR32};
            } else {
                $_{JARDIR} = $_{LIBDIR64};
            }
            unless ($_{JARDIR}) {
                print "install: error: jar file was not cannot found\n";
                exit 1;
            }
        }
    }
    $bFailure = 0;

    if ($OPT{'no-create'}) {
        print     "includedir : '$_{INST_INCDIR  }'\n" if ($HAVE{INCLUDES  });
        print     "libdir     : '$_{INST_LIBDIR}$_{BITS}'\n" if ($HAVE{LIBS});
        print     "jardir     : '$_{INST_JARDIR  }'\n" if ($HAVE{JAR       });
        print     "examplesdir: '$_{INST_SHAREDIR}'\n" if ($HAVE{EXAMPLES  });;
        if ($LINUX_ROOT) {
            print "oldincludedir: '$oldincludedir'\n"  if ($HAVE{USR_INCLUDES});
        }
        print "\n";
        next;
    }

    $_{BIN_TARGET} = "$_{INST_BINDIR}$_{BITS}" unless ($OPT{bindir});
    $_{LIB_TARGET} = "$_{INST_LIBDIR}$_{BITS}" unless ($OPT{libdir});

    $File::Copy::Recursive::CPRFComp = 1;

    $failures += copybins    () if ($HAVE{BINS});
    $failures += copylibs    () if ($HAVE{LIBS});
    $failures += copyincludes() if ($HAVE{INCLUDES});
    $failures += copyjars    () if ($HAVE{JAR});
    $failures += copyconfig  () if ($HAVE{CONFIG});

    if ($HAVE{JAR}) {
        $File::Copy::Recursive::CPRFComp = 0;
        $failures += copydocs() ;
        $File::Copy::Recursive::CPRFComp = 1;
    }

    $failures += copyexamples() if ($HAVE{EXAMPLES});
    $failures += finishinstall() unless ($failures);

    unless ($failures) {
        print "\nsuccessfully installed $_{PACKAGE_NAME} ($_{VERSION}) package";
    } else {
        print "\nfailed to install $_{PACKAGE_NAME} ($_{VERSION}) package";
    }
    print " for $_{OS}-$_{BITS}" if ($HAVE{BINS} || $HAVE{LIBS});
    print ".\n\n";
}

$failures = 1 if (!$failures && $bFailure);

exit $failures;

################################################################################

sub copybins {
    unless ($_{BIN_TARGET}) {
        print "error: cannot install executables: no BIN_TARGET\n";
        return 1;
    }
    my $s = $_{BINDIR};
    my $d = $_{BIN_TARGET};
    print "installing executables to $d...";
    unless (-e $s) {
        print " failure\n";
        print "install: error: '$s' is not found.\n";
        return 1;
    }
    print "\nchecking $d... ";
    unless (-e $d) {
        print "not found\n";
        print "mkdir -p $d... ";
        eval { make_path($d) };
        if ($@) {
            print "failure\ninstall: error: cannot mkdir $d\n";
            return 1;
        } else {
            print "success\n";
        }
    } else {
        print "exists\n";
    }
    print "\t\tcd $d\n" if ($OPT{debug});
    chdir $d or die "cannot cd $d";
    my $failures = 0;
    foreach (BINS()) {
        print "installing '$_'..." if ($OPT{debug});
        my $df = "$_$_{VERSION_EXEX}";
        my $sf = "$s/$df";
        print "\n\t\t$sf -> $df\n\t" if ($OPT{debug});
        unless (-e $sf) {
            print " skipped\n" if ($OPT{debug});
            next;
        }
        if ((! $OPT{force}) && (-e $df) && (-M $df < -M $sf)) {
            print " found\n" if ($OPT{debug});
        } else {
            unless (copy($sf, $df)) {
                print "failure\n";
                print "install: error: cannot copy '$sf' '$df'.\n";
                ++$failures;
                next;
            }
            my $mode = 0755;
            printf "\tchmod %o $df\n\t", $mode if ($OPT{debug});
            unless (chmod($mode, $df)) {
                print " failure\n" if ($OPT{debug});
                print "install: error: cannot chmod '$df': $!\n";
                ++$failures;
                next;
            }
            unless (symlinks($_, $df, 'bin')) {
                print " success\n" if ($OPT{debug});
            } else {
                print " failure\n" if ($OPT{debug});
                ++$failures;
            }
        }
    }
    return $failures;
}

sub copyconfig {
    my $d;
    if ($LINUX_ROOT) {
        $d = "$ROOT/etc";
    }
    elsif ($HAVE{BINS}) {
        $d = $_{BIN_TARGET};
        unless ($d) {
            print
               "error: cannot install configuration files: no BIN_TARGET\n";
            return 1;
        }
    } else {
        $d = $_{LIB_TARGET};
        unless ($d) {
            print
               "error: cannot install configuration files: no LIB_TARGET\n";
            return 1;
        }
    }
    $d = File::Spec->catdir($d, 'ncbi');
    my $kfg = File::Spec->catdir($Bin, '..', 'libs/kfg/default.kfg');
    unless (-e $kfg) {
        $kfg = File::Spec->catdir($Bin, '..', 'tools/vdb-copy/vdb-copy.kfg');
    }
    unless (-e $kfg) {
        if ($_{BINDIR}) {
            $kfg = File::Spec->catdir($_{BINDIR}, 'ncbi', 'vdb-copy.kfg');
        } elsif ($_{LIBDIR}) {
            $kfg = File::Spec->catdir($_{LIBDIR}, 'ncbi', 'default.kfg');
            unless (-e $kfg) {
                print
                  "error: cannot install configuration files: no default.kfg\n";
                return 1;
            }
        }
    }
    print "installing configuration files to $d... ";
    print "\nchecking $d... ";
    unless (-e $d) {
        print "not found\n";
        print "mkdir -p $d... ";
        eval { make_path($d) };
        if ($@) {
            print "failure\ninstall: error: cannot mkdir $d\n";
            return 1;
        } else {
            print "success\n";
        }
    } else {
        print "exists\n";
    }
    my $df = File::Spec->catdir($d, 'ncbi-vdb.kfg');
    print "\t\t$kfg -> $df\n" if ($OPT{debug});
    unless (copy($kfg, $df)) {
        print "install: error: cannot copy '$kfg' '$df'.\n";
        return 1;
    } else {
        print "success\n";
        return 0;
    }
}

sub copylibs {
    die unless ($HAVE{LIBS});

    my $s = $_{LIBDIR};
    my $d = $_{LIB_TARGET};

    print "installing libraries to $d... ";

    unless (-e $s) {
        print "\tfailure\n";
        print "install: error: '$s' is not found.\n";
        return 1;
    }

    if ($HAVE{TWO_LIBS}) {
        my $ngs = $_{INST_NGSLIBDIR};
        if ($ngs && ! ($OPT{prefix} && $OPT{libdir} && $OPT{ngslibdir})) {
            $ngs .= $_{BITS};
        }
        my $vdb = $_{INST_VDBLIBDIR};
        if ($vdb && ! ($OPT{prefix} && $OPT{libdir} && $OPT{vdblibdir})) {
            $vdb .= $_{BITS};
        }
        if ($ngs || $vdb) {
            unless ($ngs && $vdb) {
                $ngs = $d unless ($ngs);
                $vdb = $d unless ($vdb);
            }
            $INSTALLED_LIBS{'ngs-sdk' } = $ngs;
            $INSTALLED_LIBS{'ncbi-vdb'} = $vdb;
        }
    }
    $INSTALLED_LIBS{0} = $d unless (%INSTALLED_LIBS);

    foreach (keys %INSTALLED_LIBS) {
        my $d = $INSTALLED_LIBS{$_};
        print "\nchecking $d... ";
        unless (-e $d) {
            print "not found\n";
            print "mkdir -p $d... ";
            eval { make_path($d) };
            if ($@) {
                print "failure\ninstall: error: cannot mkdir $d\n";
                return 1;
            } else {
                print "success\n";
            }
        } else {
            print "exists\n";
        }
    }

    return $MAKING ? copybldlibs($s, $d) : copydir($s, %INSTALLED_LIBS);
}

sub copybldlibs {
    my ($s, $d) = @_;

    print "\t\tcd $d\n" if ($OPT{debug});
    chdir $d or die "cannot cd $d";

    my $failures = 0;

    my %LIBRARIES_TO_INSTALL = LIBS();
    foreach (keys %LIBRARIES_TO_INSTALL) {
        print "installing '$_'... ";

        my $nb = "$_{LPFX}$_";
        my $nv = "$nb.";
        my $lib = 'dll';
        if ($LIBRARIES_TO_INSTALL{$_} eq 'SHL') {
            $nv .= $_{VERSION_SHLX};
        } elsif ($LIBRARIES_TO_INSTALL{$_} eq 'LIB') {
            $nv .= $_{VERSION_LIBX};
            $lib = 'lib';
        } else {
            die "bad library type";
        }

        my $sf = "$s/$nv";
        my $df = "$d/$nv";

        print "\n\t\t$sf -> $df\n\t" if ($OPT{debug});

        unless (-e $sf) {
            print "failure\n";
            print "install: error: '$sf' is not found.\n";
            ++$failures;
            next;
        }

        if ((! $OPT{force}) && (-e $df) && (-M $df < -M $sf)) {
            print "found\n";
        } else {
            unless (copy($sf, $df)) {
                print "failure\n";
                print "install: error: cannot copy '$sf' '$df'.\n";
                ++$failures;
                next;
            }
            my $mode = 0644;
            $mode = 0755 if ($lib eq 'dll');
            printf "\tchmod %o $df\n\t", $mode if ($OPT{debug});
            unless (chmod($mode, $df)) {
                print "failure\n";
                print "install: error: cannot chmod '$df': $!\n";
                ++$failures;
                next;
            }
            unless (symlinks($nb, $nv, $lib)) {
                print "success\n";
            } else {
                print "failure\n";
                ++$failures;
            }
        }
    }

    return $failures;
}

sub symlinks {
    my ($nb, $nv, $type) = @_;

    my @l;
    if ($type eq 'lib') {
        push @l, "$nb-static.$_{LIBX}";
        push @l, "$nb.$_{LIBX}";
        push @l, "$nb.$_{MAJVERS_LIBX}";
    } elsif ($type eq 'dll') {
        push @l, "$nb.$_{SHLX}";
        push @l, "$nb.$_{MAJVERS_SHLX}";
    } elsif ($type eq 'bin' || $type eq 'jar') {
        push @l, $nb;
        push @l, "$nb.$_{MAJVERS}";
    } else {
        print "failure\n";
        print "install: error: unknown symlink type '$type'\n";
        return 1;
    }

    my $failures = 0;

    for (my $i = 0; $i <= $#l; ++$i) {
        my $file = $l[$i];
        if (-e $file) {
            print "\trm $file\n\t" if ($OPT{debug});
            unless (unlink $file) {
                print "failure\n";
                print "install: error: cannot rm '$file': $!\n";
                ++$failures;
                next;
            }
        }

        my $o = $nv;
        $o = $l[$i + 1] if ($i < $#l);

        print "\tln -s $o $file\n\t" if ($OPT{debug});
        unless (symlink $o, $file) {
            print "failure\n";
            print "install: error: cannot symlink '$o' '$file': $!\n";
            ++$failures;
            next;
        }
    }

    return $failures;
}

sub copydir {
    my ($s, %d) = @_;

    my $failures = 0;

    foreach my $pattern(keys %d) {
        my $d = $d{$pattern};
        print "\t\tcd $d\n" if ($OPT{debug});
        chdir $d or die "cannot cd $d";

        opendir(D, $s) or die "cannot opendir $s: $!";

        while (readdir D) {
            next if (/^\.{1,2}$/);
            next if ($pattern && ! /$pattern/);

            my $n = "$s/$_";

            if (-l $n) {
                print "\t\t$_ (symlink)... " if ($OPT{debug});
                my $l = readlink $n;
                if ((-e $_) && (!unlink $_)) {
                    print "error: cannot remove $l: $!\n";
                    ++$failures;
                    next;
                }
                unless (symlink($l, $_)) {
                    print "error: cannot create symlink from $_ to $l: $!\n";
                    ++$failures;
                    next;
                }
                print "success\n" if ($OPT{debug});
            } else {
                print "\t\t$_... " if ($OPT{debug});
                if ((-e $_) && (!unlink $_)) {
                    print "error: cannot remove $_: $!\n";
                    ++$failures;
                    next;
                }
                unless (copy($n, $_)) {
                    print "error: cannot copy '$n' to '$_': $!\n";
                    ++$failures;
                    next;
                }
                print "success\n" if ($OPT{debug});
            }
        }

        closedir D;
    }

    return $failures;
}

sub includes_out {
    my $out = '';
    eval { $out = INCLUDES_OUT(); };
    $out = File::Spec->catdir($_{INST_INCDIR}, $out);
    $out;
}

sub copyincludes {
    print "installing includes to $_{INST_INCDIR}... ";

    my $s = "$_{INCDIR}/" . INCLUDES();
    unless (-e $s) {
        print "\tfailure\n";
        print "install: error: '$s' is not found.\n";
        return 1;
    }

    my $out = includes_out();
    my $d = $out;
    $d = $_{INST_INCDIR} unless ($d);

    unless (-e $d) {
        print "\n\t\tmkdir -p $d" if ($OPT{debug});
        eval { make_path($d) };
        if ($@) {
            print "\tfailure\ninstall: error: cannot mkdir $d\n";
            return 1;
        }
    }

    if ($out && -f $s) {
        print "\n\t\tcp $s $d\n\t" if ($OPT{debug});
        unless (copy($s, $d)) {
            print "failure\n";
            return 1;
        }
    } else {
        print "\n\t\tcp -r $s $d\n\t" if ($OPT{debug});
        unless (dircopy($s, $d)) {
            print "\tfailure\ninstall: error: cannot copy '$s' 'd'";
            return 1;
        }
    }

    print "success\n";
    return 0;
}

sub copyjars {
    my $s = $_{JARDIR};
    my $d = $_{INST_JARDIR};

    print "installing jar files to $d... ";

    unless (-e $s) {
        print "\tfailure\n";
        print "install: error: '$s' is not found.\n";
        return 1;
    }

    print "\nchecking $d... ";
    unless (-e $d) {
        print "not found\n";
        print "mkdir -p $d... ";
        eval { make_path($d) };
        if ($@) {
            print "failure\ninstall: error: cannot mkdir $d\n";
            return 1;
        } else {
            print "success\n";
        }
    } else {
        print "exists\n";
    }

    return $MAKING ? copybldjars($s, $d) : copydir($s, 0 => $d);
}

sub copybldjars {
    my ($s, $d) = @_;
    my $n = 'ngs-java.jar';
    $s .= "/$n";

    unless (-e $s) {
        print "\tfailure\n";
        print "install: error: '$s' is not found.\n";
        return 1;
    }

    my $nd = "$n.$_{VERSION}";
    print "installing '$n'... ";

    print "\t\tcd $d\n" if ($OPT{debug});
    chdir $d or die "cannot cd $d";

    $d .= "/$nd";

    print "\n\t\t$s -> $d\n\t" if ($OPT{debug});

    if ((! $OPT{force}) && (-e $d) && (-M $d < -M $s)) {
        print "found\n";
    } else {
        unless (copy($s, $d)) {
            print "failure\n";
            print "install: error: cannot copy '$s' '$d'.\n";
            return 1;
        }
        my $mode = 0644;
        printf "\tchmod %o $d\n\t", $mode if ($OPT{debug});
        unless (chmod($mode, $d)) {
            print "failure\n";
            print "install: error: cannot chmod '$d': $!\n";
            return 1;
        }
        unless (symlinks($n, $nd, 'jar')) {
            print "success\n";
        } else {
            print "failure\n";
            return 1;
        }
    }

    return 0;
}

sub copydocs {
    my $s = "$_{JARDIR}/javadoc";
    $s = expand_path("$Bin/../doc") unless ($MAKING);
    my $d = "$_{INST_SHAREDIR}/doc";

    print "installing html documents to $d... ";

    unless (-e $s) {
        print "\tfailure\n";
        print "install: error: '$s' is not found.\n";
        return 1;
    }

    print "\nchecking $d... ";
    unless (-e $d) {
        print "not found\n";
        print "mkdir -p $d... ";
        eval { make_path($d) };
        if ($@) {
            print "failure\ninstall: error: cannot mkdir $d\n";
            return 1;
        } else {
            print "success\n";
        }
    } else {
        print "exists\n";
    }

    print "\t\t$s -> $d\n\t" if ($OPT{debug});
    unless (dircopy($s, $d)) {
        print "\tfailure\ninstall: error: cannot copy '$s' to '$d'";
        return 1;
    }

    print "success\n";
    return 0;
}

sub copyexamples {
    my $failures = 0;
    my $CPRFComp = $File::Copy::Recursive::CPRFComp;
    my $sd = $EXAMPLES_DIR;
    return 0 unless (-e $sd);

    my $d = $_{INST_SHAREDIR};
    unless ($d) {
        print "install: error: cannot install examples\n";
        return 0;
    }

    if ($HAVE{JAR}) {
        $d .= '/examples-java';
    } elsif ($HAVE{PYTHON}) {
        $File::Copy::Recursive::CPRFComp = 0;
        $d .= '/examples-python';
    }

    print "installing examples to $d... ";

    my $s = $sd;
    $s = "$sd/examples" if ($HAVE{JAR} && $MAKING);

    unless (-e $s) {
        print "\tfailure\n";
        print "install: error: '$s' is not found.\n";
        ++$failures;
    }

    unless ($failures) {
        print "\nchecking $d... ";
        unless (-e $d) {
            print "not found\n";
            print "mkdir -p $d... ";
            eval { make_path($d) };
            if ($@) {
                print "failure\ninstall: error: cannot mkdir $d\n";
                ++$failures;
            } else {
                print "success\n";
            }
        } else {
            print "exists\n";
        }
    }

    unless ($failures) {
        print "\t\t$s -> $d\n\t" if ($OPT{debug});
        if ($HAVE{JAR} && ! $MAKING) {
            if (copydir($s, 0 => $d)) {
                ++$failures;
            }
        } else {
            unless (dircopy($s, $d)) {
                print "\tfailure\ninstall: error: cannot copy '$s' to '$d'";
                ++$failures;
            }
        }
    }

    unless ($failures) {
        if ($HAVE{JAR} && $MAKING) {
            $sd = "$sd/Makefile";
            $d = "$d/Makefile";
            print "\t$sd -> $d\n\t" if ($OPT{debug});
            unless (-e $sd) {
                print "\tfailure\n";
                print "install: error: '$sd' is not found.\n";
                ++$failures;
            }
            unless ($failures) {
                if (-e $d) {
                unless (unlink $d) {
                    print "failure\n";
                    print "install: error: cannot rm '$d': $!\n";
                    ++$failures;
                }
            }
            unless ($failures) {
                unless (copy($sd, $d)) {
                    print "error: cannot copy '$sd' to '$d': $!\n";
                    ++$failures;
                }
            }
        }
      }
    }

    print "success\n" unless ($failures);

    $File::Copy::Recursive::CPRFComp = $CPRFComp;

    return $failures;
}

sub finishinstall {
    my $failures = 0;

    $_{JAR_TARGET} = "$_{INST_JARDIR}/ngs-java.jar";

    my @libs;
    if (%INSTALLED_LIBS) {
        my %libs;
        ++$libs{$INSTALLED_LIBS{$_}} foreach (keys %INSTALLED_LIBS);
        push @libs, $_ foreach (keys %libs);
    } else {
        push @libs, $_{LIB_TARGET};
    }
    my $libs;
    foreach (@libs) {
        $libs .= ":" if ($libs);
        $libs .= $_;
    }

    if ($HAVE{PYTHON}) {
        chdir "$Bin/.." or die "cannot cd '$Bin/..'";
        my $cmd = "python setup.py install";
        $cmd .= ' --user' unless (linux_root());
        print `$cmd`;
        if ($?) {
            ++$failures;
        } else {
            unless ($libs) {
                print "internal python failure\n";
                ++$failures;
            } elsif ($HAVE{LIBS}) {
                print <<EndText;
Please add $libs to your LD_LIBRARY_PATH, e.g.:
      export LD_LIBRARY_PATH=$libs:\$LD_LIBRARY_PATH
EndText
            }
        }
    } elsif ($LINUX_ROOT) {
        print "\t\tlinux root\n" if ($OPT{debug});

        if ($HAVE{USR_INCLUDES}) {
            unless (-e $oldincludedir) {
                print "install: error: '$oldincludedir' does not exist\n";
                ++$failures;
            } else {
                my $o = includes_out();
                if ($o) {
                    eval { INCLUDES_OUT(); };
                    if ($@) {
                        print "install: cannot find INCLUDES_OUT\n";
                        ++$failures;
                    } else {
                        my $INCLUDE_SYMLINK
                            = "$oldincludedir/" . INCLUDES_OUT();
                        print "updating $INCLUDE_SYMLINK... ";
                        unlink $INCLUDE_SYMLINK;
                        if ($OPT{debug}) {
                            print "\n\t\tln -s $o $INCLUDE_SYMLINK... ";
                        }
                        unless (symlink $o, $INCLUDE_SYMLINK) {
                            print "failure\n";
                            print "install: error: " .
                                "cannot symlink '$o' '$INCLUDE_SYMLINK': $!\n";
                            ++$failures;
                        } else {
                            print "success\n";
                        }
                    }
                } else {
                    my $INCLUDE_SYMLINK = "$oldincludedir/" . INCLUDES();
                    print "updating $INCLUDE_SYMLINK... ";
                    unlink $INCLUDE_SYMLINK;
                    my $o = "$_{INST_INCDIR}/" . INCLUDES();
                    unless (symlink $o, $INCLUDE_SYMLINK) {
                        print "failure\n";
                        print "install: error: "
                            . "cannot symlink '$o' '$INCLUDE_SYMLINK': $!\n";
                        ++$failures;
                    } else {
                        print "success\n";
                    }
                }
            }
        }

        my $NAME = PACKAGE_NAME();
        if ($HAVE{BINS} || $HAVE{JAR}
            || ($HAVE{LIBS}
                && ($HAVE{DLLS} || $NAME eq 'NGS-SDK' || $NAME eq 'NGS-BAM')
               )
            )
        {
            my $profile = "$ROOT/etc/profile.d";
            my $PROFILE_FILE = "$profile/" . lc(PACKAGE_NAME());
            unless (-e $profile) {
                print "install: error: '$profile' does not exist\n";
                ++$failures;
            } else {
                print "updating $PROFILE_FILE.[c]sh... ";

                my $f = "$PROFILE_FILE.sh";
                if (!open F, ">$f") {
                    print "failure\n";
                    print "install: error: cannot open '$f': $!\n";
                    ++$failures;
                } else {
                    print F "#version $_{VERSION}\n\n";

                    if ($HAVE{LIBS}) {
                        unless (@libs) {
                            print "internal root libraries failure\n";
                            ++$failures;
                        } else {
                            if ($HAVE{DLLS}) {
                                foreach (@libs) {
                                    print F <<EndText;
if ! echo \$LD_LIBRARY_PATH | /bin/grep -q $_
then export LD_LIBRARY_PATH=$_:\$LD_LIBRARY_PATH
fi

EndText
                                }
                            }
                            if ($NAME eq 'NGS-SDK') {
                                print F "export NGS_LIBDIR=$_{LIB_TARGET}\n";
                            } elsif ($NAME eq 'NGS-BAM') {
                                print F
                                      "\nexport NGS_BAM_LIBDIR=$_{LIB_TARGET}\n"
                            }
                        }
                    }
                    if ($HAVE{JAR}) {
                        print F <<EndText;
if ! echo \$CLASSPATH | /bin/grep -q $_{JAR_TARGET}
then export CLASSPATH=$_{JAR_TARGET}:\$CLASSPATH
fi
EndText
                    }
                    if ($HAVE{BINS}) {
                        print F <<EndText;
if ! echo \$PATH | /bin/grep -q $_{INST_BINDIR}
then export PATH=$_{INST_BINDIR}:\$PATH
fi
EndText
                    }
                    close F;
                    unless (chmod(0644, $f)) {
                        print "failure\n";
                        print "install: error: cannot chmod '$f': $!\n";
                        ++$failures;
                    }
                }
            }

            my $f = "$PROFILE_FILE.csh";
            if (!open F, ">$f") {
                print "failure\n";
                print "install: error: cannot open '$f': $!\n";
                ++$failures;
            } else {
                print F "#version $_{VERSION}\n\n";

                if ($HAVE{LIBS}) {
                    unless (@libs) {
                        print "internal libraries failure\n";
                        ++$failures;
                    } else {
                        if ($HAVE{DLLS}) {
                            foreach (@libs) {
                                print F <<EndText;
echo \$LD_LIBRARY_PATH | /bin/grep -q $_
if ( \$status ) setenv LD_LIBRARY_PATH $_:\$LD_LIBRARY_PATH

EndText
                            }
                        }
                    }
                    if (PACKAGE_NAME() eq 'NGS-BAM') {
                        print F "setenv NGS_BAM_LIBDIR $_{LIB_TARGET}\n";
                    } elsif (PACKAGE_NAME() eq 'NGS-SDK') {
                        print F "setenv NGS_LIBDIR $_{LIB_TARGET}\n";
                    } elsif (PACKAGE_NAME() eq 'NCBI-VDB') {
                        print F "setenv NCBI_VDB_LIBDIR $_{LIB_TARGET}\n";
                    }
                }
                if ($HAVE{JAR}) {
                    print F <<EndText;
echo \$CLASSPATH | /bin/grep -q $_{JAR_TARGET}
if ( \$status ) setenv CLASSPATH $_{JAR_TARGET}:\$CLASSPATH
EndText
                }
                if ($HAVE{BINS}) {
                    print F <<EndText;
echo \$PATH | /bin/grep -q $_{INST_BINDIR}
if ( \$status ) setenv PATH $_{INST_BINDIR}:\$PATH
EndText
                }
                close F;
                unless (chmod(0644, $f)) {
                    print "failure\n";
                    print "install: error: cannot chmod '$f': $!\n";
                    ++$failures;
                }
            }
#	@ #TODO: check version of the files above
            print "success\n" unless ($failures);
        }

        unless ($failures) {
            if ($HAVE{LIBS}) {
                if (PACKAGE_NAME() eq 'NGS-BAM') {
                    print "\n";
                    print "Use \$NGS_BAM_LIBDIR in your link commands, e.g.:\n";
                    print "      ld -L\$NGS_BAM_LIBDIR -lngs-bam ...\n";
                } elsif (PACKAGE_NAME() eq 'NGS-SDK') {
                    print "\nUse \$NGS_LIBDIR in your link commands, e.g.:\n";
                    print "      ld -L\$NGS_LIBDIR -lngs-sdk ...\n";
                } elsif (PACKAGE_NAME() eq 'NCBI-VDB') {
                    print "\n"
                       . "Use \$NCBI_VDB_LIBDIR in your link commands, e.g.:\n";
                    print "      ld -L\$NCBI_VDB_LIBDIR -lncbi-vdb ...\n";
                }
            }
        }
    } else {
        print "\t\tnot linux root\n" if ($OPT{debug});
        if ($HAVE{LIBS}) {
            unless ($libs) {
                print "internal libraries failure\n";
                ++$failures;
            } else {
                print "\n";
                print <<EndText if ($HAVE{DLLS});
Please add $libs to your LD_LIBRARY_PATH, e.g.:
      export LD_LIBRARY_PATH=$libs:\$LD_LIBRARY_PATH
EndText
                if (PACKAGE_NAME() eq 'NGS-SDK') {
                    print "Use $libs in your link commands, e.g.:\n"
                        . "export NGS_LIBDIR=$libs\n"
                        . "ld -L\$NGS_LIBDIR -lngs-sdk ...\n";
                } elsif (PACKAGE_NAME() eq 'NGS-BAM') {
                    print "Use $libs in your link commands, e.g.:\n"
                        . "export NGS_BAM_LIBDIR=$libs\n"
                        . "ld -L\$NGS_BAM_LIBDIR -lngs-bam ...\n";
                }
            }
        }
        if ($HAVE{JAR}) {
            print <<EndText;

Please add $_{JAR_TARGET} to your CLASSPATH, i.e.:
      export CLASSPATH=$_{JAR_TARGET}:\$CLASSPATH
EndText
        }
    }

    return $failures;
}

sub expand_path {
    my ($filename) = @_;
    return unless ($filename);

    if ($filename =~ /^~/) {
        if ($filename =~ m|^~([^/]*)|) {
            if ($1 && ! getpwnam($1)) {
                print "install: error: bad path: '$filename'\n";
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

sub help {
    $_{LIB_TARGET} = "$_{INST_LIBDIR}$_{BITS}";

    print <<EndText;
'install' installs $_{PACKAGE_NAME} $_{VERSION} package.

Usage: ./install [OPTION]...

Defaults for the options are specified in brackets.

Configuration:
  -h, --help              display this help and exit
  -n, --no-create         do not run installation

EndText

    if ($HAVE{TWO_LIBS}) {
        my $p = lc(PACKAGE_NAME());
        print "By default, `./install' will install all the files in\n";
        print "`/usr/local/ngs/$p/jar', " if ($HAVE{JAR});
        print <<EndText;
`/usr/local/ngs/$p/share',
`/usr/local/ngs/ngs-sdk/lib$_{BITS}', `/usr/local/ncbi/ncbi-vdb/lib$_{BITS}'.
You can spefify other installation directories using the options below.

Fine tuning of the installation directories:
EndText
        if ($HAVE{JAR}) {
            print
                "  --jardir=DIR         jar files [/usr/local/ngs/$p/jar]\n"
        }
        print <<EndText;
  --ngslibdir=DIR      ngs-sdk libraries [/usr/local/ngs/ngs-sdk/lib$_{BITS}]
  --vdblibdir=DIR      ncbi-vdb libraries [/usr/local/ncbi/ncbi-vdb/lib$_{BITS}]
  --examplesdir=DIR    example files [/usr/local/ngs/$p/share]

  --libdir=DIR         install all libraries in the same directory
  --prefix=DIR         install files in PREFIX/lib$_{BITS}, PREFIX/share etc.
EndText
    } else {
        print <<EndText;
Installation directories:
  --prefix=PREFIX         install all files in PREFIX
                          [$_{PREFIX}]

By default, `./install' will install all the files in
EndText

        if ($HAVE{INCLUDES}) {
            print
"`$_{PREFIX}/include', `$_{PREFIX}/lib$_{BITS}' etc.  You can specify\n"
        } elsif ($HAVE{JAR}) {
            print "`$_{PREFIX}/jar', `$_{PREFIX}/share' etc.  You can specify\n"
        } elsif ($MAKING) {
            print "`$_{PREFIX}/share' etc.  You can specify\n"
        } else {
            print
"`$_{PREFIX}/lib$_{BITS}' `$_{PREFIX}/share' etc.  You can specify\n"
        }

        print <<EndText;
an installation prefix other than `$_{PREFIX}' using `--prefix',
for instance `--prefix=$_{OTHER_PREFIX}'.
For better control, use the options below.

Fine tuning of the installation directories:
EndText

        if ($HAVE{BINS}) {
            print "  --bindir=DIR            executables [PREFIX/bin]\n";
        }
        if ($HAVE{JAR}) {
            print "  --jardir=DIR            jar files [PREFIX/jar]\n";
        }
        if ($HAVE{LIBS}) {
            print
"  --libdir=DIR            object code libraries [PREFIX/lib$_{BITS}]\n"
        }
        if ($HAVE{INCLUDES}) {
            print "  --includedir=DIR        C header files [PREFIX/include]\n";
        }
        if ($HAVE{USR_INCLUDES}) {
            print
"  --oldincludedir=DIR     C header files for non-gcc [$oldincludedir]\n"
        }

        if (-e $EXAMPLES_DIR) {
            print "  --examplesdir=DIR       example files [PREFIX/share]\n";
        }
    }

    if ($HAVE{LIBS}) {
        print <<EndText;

System types:
  --bits=[32|64]          use a 32- or 64-bit data model
EndText
    }

    print "\nReport bugs to sra-tools\@ncbi.nlm.nih.gov\n";
}

sub prepare {
    if ($MAKING) {
        my $os_arch = `perl -w $Bin/os-arch.perl`;
        unless ($os_arch) {
            print "install: error\n";
            exit 1;
        }
        chomp $os_arch;
        my $config =
            "$Bin/../" . CONFIG_OUT() . "/Makefile.config.install.$os_arch.prl";
        fatal_config("$config not found") unless (-e "$config");

        eval { require $config; };
        fatal_config($@) if ($@);
    } else {
        my $a = $Config{archname64};
        $_ = lc PACKAGE_NAME();
        my $root = '';
        $root = $OPT{root} if ($OPT{root});
        my $code = 
            'sub CONFIGURE { ' .
            '   $_{OS           } = $OS; ' .
            '   $_{VERSION      } = "1.0.0"; ' .
            '   $_{MAJVERS      } = "1"; ' .
            '   $_{LPFX         } = "lib"; ' .
            '   $_{LIBX         } = "a"; ' .
            '   $_{MAJVERS_LIBX } = "a.1"; ' .
            '   $_{VERSION_LIBX } = "a.1.0.0"; ' .
            '   $_{SHLX         } = "so"; ' .
            '   $_{OTHER_PREFIX } = \'$HOME/ngs/' . $_ . '\'; ' .
            '   $_{PREFIX       } = "' . "$root/usr/local/ngs/$_" . '"; ' .
            '   $_{INST_INCDIR  } = "$_{PREFIX}/include"; ' .
            '   $_{INST_LIBDIR  } = "$_{PREFIX}/lib"; ' .
            '   $_{INST_JARDIR  } = "$_{PREFIX}/jar"; ' .
            '   $_{INST_SHAREDIR} = "$_{PREFIX}/share"; ' .
            '   $_{INCDIR       } = "$Bin/../include"; ' .
            '   $_{LIBDIR64     } = "$Bin/../lib64"; ' .
            '   $_{LIBDIR32     } = "$Bin/../lib32"; ';
        if ($HAVE{TWO_LIBS}) {
            $code .=
               '$_{INST_NGSLIBDIR} = "' . "$root/usr/local/ngs/ngs-sdk/lib\";"
             . '$_{INST_VDBLIBDIR} = "' . "$root/usr/local/ncbi/ncbi-vdb/lib\";"
        }
        $code .= ' $_{PACKAGE_NAME} = "' . PACKAGE_NAME() . '"; ';

        if (defined $Config{archname64}) {
            $code .= ' $_{BITS} = 64; ';
        } else {
            $code .= ' $_{BITS} = 32; ';
        }

        $code .= 
            '   $_{MAJVERS_SHLX } = "so.1"; ' .
            '   $_{VERSION_SHLX } = "so.1.0.0"; ' ;

        $code .= 
            '   @_ ' .
            '}';

        eval $code;

        die $@ if ($@);
    }
}

sub linux_root { $^O eq 'linux' && `id -u` == 0 }

sub fatal_config {
    if ($OPT{debug}) {
        print "\t\t";
        print "@_";
        print "\n";
    }

    print "install: error: run ./configure [OPTIONS] first.\n";

    exit 1;
}

################################################################################
