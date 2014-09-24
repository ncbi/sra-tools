#!/usr/bin/perl -w

################################################################################
# N.B. Run "perl configuration-assistant.perl" if you see a message like:
# configuration-assistant.perl: /usr/bin/perl: bad interpreter: No such file or directory
################################################################################
my $VERSION = '2.3.5';
################################################################################

use strict;

use Cwd "getcwd";
use Fcntl ':mode';
use File::Basename qw(basename dirname);
use File::Path "mkpath";
use File::Spec;
use Getopt::Long "GetOptions";
use IO::Handle;
use Getopt::Long "GetOptions";

sub println { print @_; print "\n"; }

STDOUT->autoflush(1);

my $R = 1;
my $W = 2;
my $X = 4;
my $CREATED = 8;
my $FOUND = 16;
my $RWX = $FOUND + $R + $W + $X;

println "==========================================";
println "Welcome to the SRA Toolkit Configuration Script.";
println "SRA toolkit documentation:";
println "http://www.ncbi.nlm.nih.gov/Traces/sra/std";
println "==========================================\n";

my %options;
Help(1) unless (GetOptions(\%options, 'fix', 'help', 'version', 'wordy'));
Help(0) if ($options{help});
Version() if ($options{version});

$options{references} = 1 if ($#ARGV >= 0);

printOS();
println "cwd = '" . getcwd() . "'\n";

my $DECRYPTION_PKG;
{
    my ($fastq_dump, $fastq_dir) = FindBin("fastq-dump", "optional");
    my ($sam_dump, $sam_dir) = FindBin("sam-dump", "optional");
    if (! $fastq_dump && ! $sam_dump) {
        println "presuming to be run in decryption package";
        $DECRYPTION_PKG = 1;
    }
}

my ($VDB_CONFIG, $BIN_DIR) = FindBin("vdb-config");

my %kfg;
Kfg(\%kfg, 'read', 'Reading configuration');
umask 0002;
my $fixed = FixKfg(\%kfg);
if ($fixed) {
    undef %kfg;
    Kfg(\%kfg, 'read', 'Checking configuration');
}
#DoKryptoCfg(\%kfg);

################################################################################
# FUNCTIONS #
################################################################################

sub Ask {
    my ($prompt) = @_;

    print "$prompt: ";

    my $in = <STDIN>;
    unless ($in) {
        println;
        return "";
    }

    chomp $in;
    return $in;
}

sub AskYn { return AskYN($_[0], 'yes');}
sub AskyN { return AskYN($_[0], 'no' );}

sub AskYN {
    my ($q, $yes) = @_;

    $yes = '' if ($yes eq 'no');

    print "$q ";

    if ($yes) {
        print "[Y/n] ";
    } else {
        print "[y/N] ";
    }

    my $in = <STDIN>;
    chomp $in;
    if ($in) {
        return $in eq 'Y' || $in eq 'y'
          || $in eq 'YES' || $in eq 'yes' || $in eq 'Yes';
    } else {
        return $yes;
    }
}

sub AskRefseqChange {
    my ($refseq) = @_;

    die unless ($refseq);

    my $force;

    while (1) {
        my $read = -r $refseq && -x $refseq;
        println "Your repository directory is $refseq.";
        my $dflt;
        if ($read) {
            println "It is read-only. You cannot add new sequences to it.";
            $dflt = "1";
        } else {
            println "You cannot read it.";
            $dflt = "2";
        }
        println "Make your choice:";
        println "1) Use existing repository";
        println "2) Use a different repository";
        println "3) exit the script";
        print "Your selection? [$dflt] ";

        my $in = <STDIN>;
        chomp $in;
        $in = $dflt unless ($in);

        if ($in eq "1") {
            unless ($read) {
                Fatal(
"Ask the owner of $refseq to allow you access $refseq directory\n" .
"Otherwise the dumpers will not be able to find reference files"
                );
            }
            return ($refseq, 'READ^ONLY');
        }

        if ($in eq "2") {
            last;
        }

        exit 0 if ($in eq "3");
    }

    my ($path, $perm) = MakeRefseqDir();

    return ($path, '');
}

sub Home {
    if ($ENV{HOME}) {
        return $ENV{HOME};
    } elsif ($ENV{USERPROFILE}) {
        return $ENV{USERPROFILE};
    } else {
        return '';
    }
}

sub MakeRefseqDir {
    my $deflt;
    if ($^O eq 'cygwin' and $ENV{USERPROFILE}) {
        $deflt = $ENV{USERPROFILE};
        $deflt =~ tr|\\|/|;
        $deflt =~ s|^([a-zA-Z]):/|/$1/|;
        if ($1) {
            $deflt = "/cygdrive$deflt";
        }
    } else {
        $deflt = Home();
    }
    unless ($deflt) {
        $deflt = getcwd();
    }
    $deflt = "." unless($deflt);
    $deflt = File::Spec->catdir($deflt, "ncbi", "refseq");

    while (1) {
        my $path;
        print "Specify installation directory for reference objects";
        if ($deflt) {
            print " [$deflt]";
        }
        print ": ";

        my $in = <STDIN>;
        unless ($in) {
            println;
            $path = "";
        }
        chomp $in;
        if ($in) {
            $path = $in;
        } elsif ($deflt) {
            $path = $deflt;
        }
        exit 1 unless ($path);
        my ($prm_org, $perm) = CheckDir($path, "create_missing");
        my $prm = $prm_org;
        my $created = $prm & $CREATED;
        $prm &= $RWX;
        if ($prm == $RWX || ($^O eq 'cygwin' and $created == $CREATED)) {
            return ($path, $prm_org);
        } elsif ($prm & $FOUND) {
            println "'$path' does not seem to be writable.";
            println "You will not be able to add new sequences to it.";
            if (AskyN("Do you want to use it?")) {
                return ($path, $prm_org);
            }
        }
    }
}

sub Posixify {
    ($_) = @_;

    # convert to POSIX path
    s|^/cygdrive/|/|;
    tr|\\|/|;
    s|^([a-zA-Z]):/|/$1/|;

    return $_;
}

sub printOS {
    my $fail;
    if ($^O eq 'MSWin32') {
        $fail = system('ver');
    }
    else {
        $fail = system('uname -a');
        if ($fail) {
            $fail = system('ver');        
        }
    }
    if ($fail) {
        println "Operating system: '$^O'";
    }
}

sub Version {
      my $bin = basename($0);
      print << "END";
$bin version $VERSION
END

      exit 0;
}

sub Help {
    my ($exit) = @_;
    $exit = 0 unless ($exit);
      my $bin = basename($0);
      print << "END";
$bin version $VERSION

Utility to help configure the SRA tools to be able
to access the local reference repository,
determine which reference sequences a SRA file relies
upon and to fetch them from NCBI.
Fetched references are placed in the local reference repository.

Usage: $bin [--fix] [--wordy] [SRA_FILE ...]

Options
-f, --fix         re-download existing reference files;
                  fix refseq repository permissions
-v, --version     print version and exit
-w, --wordy       increase "fetcher" verbosity
END

      exit $exit;
}

sub FixRefseqCfg {
    my ($refseq_cfg, $question, $bye) = @_;

    unless (AskYn($question)) {
        print $bye;
        exit 1;
    }

    ($refseq_cfg->{refseq_dir}, $refseq_cfg->{refseq_dir_prm})
        = MakeRefseqDir();
    UpdateRefseqCfgNode($refseq_cfg->{refseq_dir});
}

sub UpdateRefseqCfgNode {
    ($_) = @_;
    $_ = Posixify($_);
    UpdateConfigNode('refseq/paths' , $_);
}

sub UpdateKryptoCfgNode {
    ($_) = @_;
    $_ = Posixify($_);
    UpdateConfigNode('krypto/pwfile', $_);
}
    
sub UpdateConfigNode {
    my ($name, $val) = @_;

    print "'$name' => '$val': ";
    my $cmd = "$VDB_CONFIG -s \"$name=$val\"";
#   println "\n$cmd";
    `$cmd`;
    if ($?) {
        println "error: $!";
    } else {
        println "updated";
    }
}

sub DoSchemaCfg {
    my $error;

    print "checking schema configuration... ";

    my $tmp = `$VDB_CONFIG vdb/schema/paths 2>&1`;
    if ($? == 0) {
        chomp $tmp;

        if ($tmp =~ m|<paths>(.+)</paths>|) {
            my $paths = $1;
            println $paths;
            my @paths = split(/:/, $paths);

            $error += !CheckSchemaFile('align/align.vschema', @paths);
            $error += !CheckSchemaFile('ncbi/seq.vschema'   , @paths);
            $error += !CheckSchemaFile('vdb/vdb.vschema'    , @paths);
        } else {
            $error = "unexpected: $tmp";
            println $error;
        }
    } elsif ($tmp =~ /path not found/) {
        $error = "not found";
        println $error;
    } else {
        $error = "unknown vdb-config schema error";
        println $error;
    }

    if ($error) {
        println "--------------------------------------";
        println "WARNING: SCHEMA FILES CANNOT BE FOUND.";
        println "IT COULD CAUSE LOADERS TO FAIL.";
        println "--------------------------------------";
    }

    return ! $error;
}

sub CheckSchemaFile {
    my ($file, @dir) = @_;

    my $cygdrive = '/cygdrive';
    my $hasCygdrive = -e '/cygdrive';

    print "checking $file... ";

    foreach my $dir(@dir) {
        my $path = "$dir/$file";
        if (-e $path) {
            println $path;
            return 1;
        }
    }

    println "not found";

    return 0;
}

sub CleanEmptyFiles {
    my ($refseq_dir) = @_;

    print "checking $refseq_dir for invalid empty reference files... ";

    my $i = 0;
    opendir DIR, $refseq_dir or die "cannot opendir $refseq_dir";
    while ($_ = readdir DIR) {
        next if (/^\.{1,2}$/);

        my $f = "$refseq_dir/$_";

        my $empty;
        if (-z $f) {
            ++$empty;
        } elsif (-d $f) {
            # skip a directory
        } elsif (-s $f < 999) {
            open F, $f or die "cannot open $f";
            my $data = '';
            while (<F>) {
                while(chomp) {};
                $data .= $_;
            }
            ++$empty if ($data =~ m|<title>404 - Not Found</title>|);
        }
        if ($empty) {
            unlink $f or die "cannot remove $f";
            ++$i;
        }
    }
    closedir DIR;

    if ($i)
    {   println "$i found"; }
    else
    {   println "none found"; }
}

sub FixRefseqDirPermissions {
    my ($dir, $read_only) = @_;
    print "checking refseq repository...";
    my $mode = (stat($dir))[2] & 07777;

    if ($read_only) {
        println " read-only";
        print "\tchecking directory permissions...";
        unless (-r $dir && -x $dir) {
            Fatal("\n$dir cannot be accessed. "
                . "Either update its permissions\n"
                . "or choose another directory for refseq repository.");
        }
        println " OK";
        print "\tchecking file permissions...";
        opendir REP, $dir or die "cannot opendir $dir";
        my $ok = 1;
        while (my $f = readdir REP) {
            next if ($f =~ /^\.{1,2}$/);
            unless (-r "$dir/$f") {
                Warn("\n$dir/$f is not readable. "
                    . "Either update its permissions\n"
                    . "or choose another directory for refseq repository.");
                $ok = 0;
            }
        }
        closedir REP;
        println " OK" if $ok;
        println "\tchecking parent directories...";
        my @dirs = File::Spec->splitdir($dir);
        my $p = File::Spec->rootdir;
        while (@dirs) {
            $_ = shift @dirs;
            $p = File::Spec->catdir($p, $_);
            print "\t\t$p";
            $ok = 1;
            unless (-r $p && -x $p) {
                Warn("\n$p cannot be accessed. "
                    . "Either update its permissions\n"
                    . "or choose another directory for refseq repository.");
                $ok = 0;
            }
            println " OK" if $ok;
        }
    } else {
        println " writable";
        print "\tchecking directory permissions...";
        my $ok = 1;
        if (($mode & 0755) != 0755) {
            $mode |= 0755;
            if (chmod($mode, $dir) != 1) {
                Warn("\n$dir is not acessible by some of the users\n"
                    . "Dumping of some files could fail if run by anoother user"
                  );
                $ok = 0;
            } else { println " fixed" }
        } else { println " OK" }
        $ok = "OK";
        print "\tchecking file permissions...";
        opendir REP, $dir or die "cannot opendir $dir";
        while (my $next = readdir REP) {
            next if ($next =~ /^\.{1,2}$/);
            my $f = "$dir/$next";
            my $mode = (stat($f))[2] & 07777;
            if (($mode & 0644) != 0644) {
                if ($ok) {
                    $mode |= 0644;
                    if (chmod($mode, $f) != 1) {
                        Warn("\n$f cannot be acessed by some of the users\n"
                                . "Dumping of some files could fail "
                                . "if run by anoother user");
                        $ok = "";
                    } else { $ok = "fixed" }
                }
            }
        }
        closedir REP;
        println " $ok" if ($ok);
        $ok = 1;
        println "\tchecking parent directories...";
        my @dirs = File::Spec->splitdir($dir);
        my $p = File::Spec->rootdir;
        while (@dirs) {
            $_ = shift @dirs;
            $p = File::Spec->catdir($p, $_);
            print "\t\t$p";
            my $mode = (stat($p))[2] & 07777;
            if (($mode & 0550) != 0550) {
                if ($ok) {
                    $mode |= 0550;
                    if (chmod($mode, $p) != 1) {
                        Warn("\n$p cannot be acessed by some of the users\n"
                                . "Dumping of some files could fail "
                                . "if run by anoother user");
                        $ok = 0;
                    } else { println " fixed" }
                }
            } else { println " OK" }
        }
    }
}

################################################################################

sub CheckDir {
    my ($dir, $create_missing) = @_;

    $dir = File::Spec->canonpath($dir);
    print "checking $dir... ";

    my $prm = 0;
    unless (-e $dir) {
        println "not found";
        return (0, 0) unless ($create_missing);

        print "checking ${dir}'s parents... ";
        $dir = File::Spec->canonpath($dir);
        my @dirs = File::Spec->splitdir($dir);
        my $test = File::Spec->rootdir();
        if ($^O eq 'MSWin32') {
            $test = "";
        } else {
            Fatal("bad root directory '$test'") unless (-e $test);
        }
        foreach (@dirs) {
            my $prev = $test;
            if ($test) {
                $test = File::Spec->catdir($test, $_);
            } else {
                $test = File::Spec->catdir($_, File::Spec->rootdir());
            }
            if (! -e $test) {
                $test = $prev;
                last;
            }
        }

        print "($test)... ";
        my $cygwin_beauty;
# cygwin does not detect -r for $ENV{USERPROFILE}
        if (! -r $test || ! -x $test) {
            if ($^O eq 'cygwin') {
                ++$cygwin_beauty;
            } else {
                println "not readable";
                return (0, 0);
            }
        }
        if (! -x $test) {
            if ($^O eq 'cygwin') {
                ++$cygwin_beauty;
            } else {
                println "not writable";
                return (0, 0);
            }
        }
        if ($cygwin_beauty) {
            println("fail to check");
        } else {
            println("ok");
        }

        print "creating $dir... ";
        unless (mkpath($dir)) {
            die "cannot mkdir $dir" unless ($cygwin_beauty);
            println "failed. Is it writable?";
            return (0, 0);
        }
        println("ok");
        $prm += $CREATED;
        print "checking $dir... ";
    }

    my $perm = (stat($dir))[2];
    $prm += $FOUND;

    {
        my $cygwin_beauty;
        my $failures;
        if (-r $dir) {
            $prm += $R;
        }
        if (-w $dir) {
            $prm += $W;
        }
        if (-x $dir) {
            $prm += $X;
        }

        if (! -r $dir || ! -x $dir) {
            if ($^O eq 'cygwin') {
                ++$cygwin_beauty;
            } else {
                println "not readable";
                ++$failures;
            }
        }
        if (! $failures and ! -w $dir) {
            if ($^O eq 'cygwin') {
                ++$cygwin_beauty;
            } else {
                println "not writable";
                ++$failures;
            }
        }
        if ($cygwin_beauty) {
            println("fail to check");
        } elsif (!$failures) {
            println("ok");
        }
    }

    return ($prm, $perm);
}

sub CheckCfg {
    print "checking refseq configuration... ";

    my %konfig;
    RefseqFromConfig(\%konfig);

    if ($konfig{refseqrepository} || $konfig{servers} || $konfig{volumes})
    {
        if ($konfig{refseqrepository}) {
            println "refseq/repository found:";
        } elsif ($konfig{servers}) {
            println "refseq/servers found:";
        } elsif ($konfig{volumes}) {
            println "refseq/volumes found:";
        }
        println "Seems to be running in NCBI environment:";
        println "      refseq configuration fix/update is disabled;";
        println "      reference files download is disabled.";
        ++$options{NCBI};
        return;
    }

    if ($konfig{paths}) {
        println "paths=$konfig{paths}";
    } else {
        println "not found";
        return %konfig;
    }

    if ($konfig{paths} and index($konfig{paths}, ":") != -1) {
        die "Unexpected configuration: paths=$konfig{paths}";
    }

    my $dir = "$konfig{paths}";

    if ($^O eq 'MSWin32') { # Windows: translate POSIX to Windows path
        $dir =~ tr|/|\\|;
        $dir =~ s/^\\([a-zA-Z])\\/$1:\\/;
    } elsif ($^O eq 'cygwin' and $dir =~ m|^/[a-zA-Z]/|) {
        $dir = "/cygdrive$dir";
    }

    $konfig{refseq_dir} = $dir;
    my ($prm, $perm) = CheckDir($dir);
    $konfig{refseq_dir_prm} = $prm;
    if ($prm == 0) { # not found
        return %konfig;
    } elsif ($prm != $RWX) {
        if ($^O ne 'cygwin') {
            ++$konfig{FIX_paths};
#           Fatal("refseq repository is invalid or read-only");
        } # else cygwin does not always can tell permissions correctly
    }
    return %konfig;
}

sub FindWget {
    my $WGET;

    print "checking for wget... ";
    my $out = `wget -h 2>&1`;
    if ($? == 0) {
        println "yes";
        if ($options{fix}) {
            $WGET = "wget -O";
        } else {
            $WGET = "wget -c -O";
        }
        ++$options{all_references};
    } else {
        println "no";
    }

    unless ($WGET) {
        print "checking for curl... ";
        my $out = `curl -h 2>&1`;
        if ($? == 0) {
            println "yes";
            $WGET = "curl -o";
            ++$options{all_references} if ($options{fix});
        } else {
            println "no";
        }
    }

    unless ($WGET) {
        print "checking for ./wget... ";
        my $cmd = dirname($0) ."/wget";
        my $out = `$cmd -h 2>&1`;
        if ($? == 0) {
            println "yes";
            if ($options{fix}) {
                $WGET = "$cmd -O";
            } else {
                $WGET = "$cmd -c -O";
            }
            ++$options{all_references};
        } else {
            println "no";
        }
    }

    unless ($WGET) {
        print "checking for ./wget.exe... ";
        my $cmd = dirname($0) ."/wget.exe";
        my $out = `$cmd -h 2>&1`;
        if ($? == 0) {
            println "yes";
            if ($options{fix}) {
                $WGET = "$cmd -O";
            } else {
                $WGET = "$cmd -c -O";
            }
            ++$options{all_references};
        } else {
            println "no";
        }
    }

    Fatal("none of wget, curl could be found") unless ($WGET);

    return $WGET;
}

sub FindBin {
    my ($name, $optional) = @_;

    my $prompt = "checking for $name";
    my $basedir = dirname($0);

    # built from sources
    print "$prompt (local build)... ";
    if (-e File::Spec->catfile($basedir, "Makefile")) {
        my $f = File::Spec->catfile($basedir, "build", "Makefile.env");
        if (-e $f) {
            my $dir = `make -s bindir -C $basedir 2>&1`;
            if ($? == 0) {
                chomp $dir;
                my $try = File::Spec->catfile($dir, $name);
                print "($try";
                if (-e $try) {
                    print ": found)... ";
                    my $tmp = `$try -h 2>&1`;
                    if ($? == 0) {
                        println "yes";
                        return ($try, $dir);
                    } else {
                        println "\nfailed to run '$try -h'";
                    }
                } else {
                    println ": not found)";
                }
            }
        }
    } else {
        println "no";
    }

    # try the script directory
    {
        my $try = File::Spec->catfile($basedir, $name);
        print "$prompt ($try";
        if (-e $try) {
            print ": found)... ";
            my $tmp = `$try -h 2>&1`;
            if ($? == 0) {
                println "yes";
                return ($try, $basedir);
            } else {
                println "\nfailed to run '$try -h'";
            }
        } else {
            println ": not found)";
        }
    }

    # the script directory: windows
    {
        my $try = File::Spec->catfile($basedir, "$name.exe");
        print "$prompt ($try";
        if (-e $try) {
            print ": found)... ";
            my $tmp = `$try -h 2>&1`;
            if ($? == 0) {
                println "yes";
                return ($try, $basedir);
            } else {
                println "\nfailed to run '$try -h'";
            }
        } else {
            println ": not found)";
        }
    }

    # check from PATH
    {
        my $try = "$name";
        print "$prompt ($try)... ";
        my $tmp = `$try -h 2>&1`;
        if ($? == 0) {
            println "yes";
            return ($try, "");
        } else {
            println "no";
        }
    }

    Fatal("$name could not be found") unless ($optional);
    return (undef, undef);
}

sub DoKryptoCfg {
    my ($kfg) = @_;

    my $v = $kfg->{'krypto/pwfile'};
    unless ($v) {
        println "failed to read krypto configuration";
        return;
    }

    my $dflt = $DECRYPTION_PKG ? 'y' : 'n';
    print "Do you want to " . ((-e $v) ? "update" : "set") . " your dbGap encryption key? [" .
        ($DECRYPTION_PKG ? "Y/n" : "y/N") . "] ";
    my $answer = GetYNAnswer($dflt, 'y');
    UpdateKryptoCfg() if ($answer eq 'y');
}

sub UpdateKryptoCfg {
    my $VDB_PASSWD = (FindBin('vdb-passwd'))[0];

    my $res = system("$VDB_PASSWD -q");
    if ($res) {
        println "password update failed";
    } else {
        println "password updated ok";
    }
}

sub RefseqConfig {
    my ($nm) = @_;

    $_ = `$VDB_CONFIG refseq/$nm 2>&1`;

    if ($?) {
        if (/path not found while opening node/) {
            $_ = '';
        } else {
            die $!;
        }
    } else {
        m|<$nm>(.*)</$nm>|s;
        die "Invalid 'refseq/$nm' configuration" unless ($1);
        # TODO die if (refseq/paths = "") : fix it
        $_ = $1;
    }

    return $_;
}

sub RefseqFromConfig {
    my ($refseq) = @_;

    $refseq->{paths} = RefseqConfig('paths');
    $refseq->{refseqrepository} = RefseqConfig('repository');
    $refseq->{servers} = RefseqConfig('servers');
    $refseq->{volumes} = RefseqConfig('volumes');
}

sub GetKfgNode {
    my ($nm, $specPrnt) = @_;

    print "$nm: ";

    @_ = split(/\//, $nm);
    die "GetKfgNode($nm)" if ($#_ < 0);
    my $leaf = $_[$#_];

    $_ = `$VDB_CONFIG $nm 2>&1`;

    if ($?) {
        if (/path not found while opening node/) {
            $_ = '';
        } else {
            println;
            die $!;
        }
    } else {
        m|<$leaf>(.*)</$leaf>|s;
        unless ($1) {
            println;
            die "Invalid '$nm' configuration: '$_'" 
        }
        $_ = $1;
    }

    if ($_) {
        if ($specPrnt) {
            if ($specPrnt  eq 'file') {
                print "'$_': ";
                if (-e $_) {
                    println "exists";
                } else {
                    println "does not exist";
                }
            } else {
                println "found";
            }
        } else {
            println "'$_'";
        }
    } else {
        println "not found";
    }

    return $_;
}

sub Kfg {
    my ($kfg, $kfgMode, $msg) = @_;
    my $updated;
    my $read;
    if ($kfgMode eq 'fix') {
        println "\n$msg";
    } else {
        println "\n$msg";
        $read = 1;
    }

    if ($read) {
        $kfg->{dflt_user_root} = File::Spec->catdir(Home(), 'ncbi', 'public');
    }

#   my $nm = 'krypto/pwfile';
#   if ($read) {
#       $kfg->{$nm} = GetKfgNode($nm, 'file');
#       if ($kfg->{$nm} eq '1' || $kfg->{$nm} eq '/.ncbi/vdb-passwd') {
#            # fix bugs introduced by previous script version
#            $kfg->{$nm} = '';
#       }
#       println;
#   } else {
#       if (!$kfg->{$nm}) {
#           my $path = Posixify(
#               File::Spec->catdir(Home(), '.ncbi', 'vdb-passwd'));
#           UpdateConfigNode($nm, $path);
#           $updated = 1;
#       }
#   }

    if ($read) {
        my $nm = 'refseq/servers';
        $kfg->{$nm} = GetKfgNode($nm, 'file');

        $nm = 'refseq/volumes';
        $kfg->{$nm} = GetKfgNode($nm);
        if ($kfg->{'refseq/servers'} && !($kfg->{'refseq/servers'} =~ m|^/net|)
            && !($kfg->{'refseq/servers'} =~ m|^/panfs/|))
        {
            $kfg->{dflt_user_root}
                = File::Spec->catdir($kfg->{'refseq/servers'});
        }

        $nm = 'refseq/paths';
        $kfg->{$nm} = GetKfgNode($nm, 'file');
        if ($kfg->{$nm} && ($kfg->{$nm} =~ m|^(.+)/refseq$|)) {
            $kfg->{dflt_user_root} = $1;
        }

        println;
    }


    if ($read) {
        my $nm = 'repository';
        $kfg->{$nm} = GetKfgNode($nm, 'dont print');
    }

    my $disableRemoteName = 'repository/remote/main/NCBI/disabled';
    my %remote = (
        'repository/remote/main/NCBI/root'
                         => 'http://ftp-trace.ncbi.nlm.nih.gov/sra',
        'repository/remote/main/NCBI/apps/sra/volumes/fuse1000'
                         => 'sra-instant/reads/ByRun/sra',
        'repository/remote/main/NCBI/apps/refseq/volumes/refseq' => 'refseq',
        'repository/remote/main/NCBI/apps/wgs/volumes/fuseWGS'   => 'wgs',
        $disableRemoteName                                       => '',
        'repository/remote/protected/CGI/resolver-cgi'
                         => 'http://www.ncbi.nlm.nih.gov/Traces/names/names.cgi'
    );

    my $userRootName = 'repository/user/main/public/root';
    my $enableCacheName = 'repository/user/main/public/cache-enabled';
    my %user = (
        $userRootName => '',
        'repository/user/main/public/apps/sra/volumes/sraFlat'   => 'sra',
        'repository/user/main/public/apps/refseq/volumes/refseq' => 'refseq',
        'repository/user/main/public/apps/wgs/volumes/wgsFlat'   => 'wgs',
        $enableCacheName                                         => ''
   );

    if ($read && $kfg->{repository}) {
        my $nm = 'repository/site';
        $kfg->{$nm} = GetKfgNode($nm, 'dont print');

        println;
    }

    if ($read && $kfg->{repository}) {
        my $nm = 'repository/remote';
        $kfg->{$nm} = GetKfgNode($nm, 'dont print');
    }

    foreach my $nm(keys %remote) {
        if ($read) {
            if ($kfg->{'repository/remote'}) {
                $kfg->{$nm} = GetKfgNode($nm);
                unless ($kfg->{$nm}) {
                    if ($nm ne $disableRemoteName || ! $DECRYPTION_PKG) {
                        $kfg->{fix_remote} = 1;
                    }
                }
            }
        } else {
            my $v = $remote{$nm};
            if (!$kfg->{$nm}) {
                if (!$v) {
                    unless ($nm eq $disableRemoteName) {
                        println "$nm: unknown";
                        next;
                    }
                    next if ($DECRYPTION_PKG);
                    print "\nWould you like to enable Remote Internet"
                        . " access to NCBI(recommented)? [Y/n] ";
                    my $answer = GetYNAnswer('y', 'n');
                    if ($answer eq 'y') {
                        $v = 'no';
                    } else {
                        println "You will be able to work "
                            . "just with SRR files found locally";
                        $v = 'yes';
                    }
                }
                UpdateConfigNode($nm, $v);
                $updated = 1;
            } elsif ($v && ($kfg->{$nm} ne $v)) {
                UpdateConfigNode($nm, $v);
                $updated = 1;
            }
        }
    }

    if ($read && $kfg->{repository}) {
        my $nm = 'repository/user';
        $kfg->{$nm} = GetKfgNode($nm, 'dont print');
    }

    foreach my $nm(keys %user) {
        if ($read) {
            if ($kfg->{'repository/user'}) {
                my $mode;
                $mode = 'file' if ($nm eq $userRootName);
                $kfg->{$nm} = GetKfgNode($nm, $mode);
                $kfg->{fix_user} = 1 unless ($kfg->{$nm});
            }
        } elsif (!$kfg->{$nm}) {
            my $v = $user{$nm};
            unless ($v) {
                next if ($DECRYPTION_PKG);
                if ($nm eq $userRootName) {
                    my $dflt = '';
                    if ($kfg->{dflt_user_root}) {
                        $dflt = $kfg->{dflt_user_root};
                    }
                    println;
                    print
               "The SRA Toolkit has the ability to download and cache data.\n" .
               "Please indicate where data should be stored.\n" .
               "You should choose a volume with enough free space.\n" .
               "Path to your Repository ";
                    print "[ $dflt ]: " if ($dflt);
                    my $in = <STDIN>;
                    chomp $in;
                    $in = $dflt unless ($in);
                    if (-e $in) {
                        unless (-d $in) {
                            println "$in: bad path";
                        }
                    } else {
                        println "Directory '$in' does not exist.";
                        print "Would you like to create it? [Y/n] ";
                        my $answer = GetYNAnswer('y', 'y');
                        if ($answer eq 'y') {
                            print "'$in': creating... ";
                            if (mkpath($in)) {
                                println "ok";
                            } else {
                                println "error";
                            }
                        }
                    }
                    $v = Posixify($in);
                } elsif ($nm eq $enableCacheName) {
                    print
            "\nWould you like to enable caching of downloaded data? [Y/n] ";
                    my $answer = GetYNAnswer('y', 'y');
                    $v = $answer eq 'n' ? 'false' : 'true';
                } else {
                    println "$nm: unknown";
                    next;
                }
            }
            UpdateConfigNode($nm, $v);
            $updated = 1;
        }
    }

    println;

    if ($read) {
        if (!$kfg->{repository} || $kfg->{fix_remote} || $kfg->{fix_user}) {
            println "Configuration is incorrect";
        } else {
            println "Configuration is correct";
        }
    }

    return $updated;
}

sub FixKfg {
    my ($kfg) = @_;

    if (!$kfg->{repository} || $kfg->{fix_remote} || $kfg->{fix_user}) {
        print "Your configuration is incomplete. " .
            "Would you like to fix it? [Y/n] ";
        my $answer = GetYNAnswer('y', 'n');
        if ($answer eq 'y') {
            return Kfg($kfg, 'fix', 'Fixing configuration');
        } else {
            println
                "Warning: Some tools could fail without proper configuration.";
        }
    }

    return 0;
}

sub GetYNAnswer {
    my ($dflt, $no) = @_;
    my $in = <STDIN>;
    chomp $in;
    if ($in) {
        if ($in eq 'Y' || $in eq 'y' ||
            $in eq 'YES' || $in eq 'yes' || $in eq 'Yes')
        {
            return 'y';
        } elsif ($in eq 'N' || $in eq 'n'
            || $in eq 'NO' || $in eq 'no' || $in eq 'No')
        {
            return 'n';
        } else {
            return $no;
        }
    } else {
        return $dflt;
    }
}

sub Fatal {
    my ($msg) = @_;

    print basename($0);
    println ": Fatal: $msg";

    exit 1;
}

sub Warn {
    my ($msg) = @_;

    print basename($0);
    println ": WARNING: $msg";
}

# EOF #
################################################################################
