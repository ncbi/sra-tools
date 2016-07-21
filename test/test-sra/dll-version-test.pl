#!/usr/bin/perl -w
use strict;

print STDERR "starting tests of test-sra dll-version...\n";

my $path;
$path = '/';
print "\n$path\n";

$path = '/net';
print "\n$path\n";
print `/bin/ls $path`;

$path = '/panfs';
print "\n$path\n";
print `/bin/ls $path`;

$path = '/net/traces01';
print "\n$path\n";

$path = '/panfs/traces01';
print "\n$path\n";

my $failures = 0;
my $testsra = 'test-sra';

`$testsra -h`;
die "cannot run $testsra" if ($?);

print STDERR "`test-sra -l` should fail: ";
`$testsra -l 2>&1`;
if ($?) {
    print STDERR "OK\n";
} else {
    print STDERR "ERROR\n";
    ++$failures;
}

if (1) {
    print STDERR
        "`test-sra -l <non existing path>` should report <rcPath,rcNotFound>: ";
    my $path = '/Q';
    @_ = `$testsra -l $path`;
    if ($? != 0) {
        print STDERR "ERROR\n";
        ++$failures;
    } else {
        while (1) {
            unless ($_[0] =~ /^dll path="$path"$/) {
                print STDERR "ERROR\n";
                ++$failures;
                last;
            }
            unless ($_[1] =~
             /^KDyldLoadLib="RC\(.*rcFS,rcDylib,rcLoading,rcPath,rcNotFound\)"$/
            )
            {
                print STDERR "ERROR\n";
                ++$failures;
                last;
            }
            print STDERR "OK\n";
            last;
        }
    }
}

if (1) {
    print STDERR
        "`test-sra -l <directory>` should report <rcNoObj,rcUnknown>: ";
    my $path = '/';
    @_ = `$testsra -l $path 2>&1`;
    if ($? != 0) {
        print STDERR "ERROR\n";
        ++$failures;
    } else {
        while (1) {
            unless ($_[0] =~ /^dll path="$path"$/) {
                print STDERR "ERROR\n";
                ++$failures;
                last;
            }
            unless ($_[1] =~ / test-sra.2.* warn:.+$path.+$/) {
                print STDERR "ERROR\n";
                ++$failures;
                last;
            }
            unless ($_[2] =~
             /^KDyldLoadLib="RC\(.*rcFS,rcDylib,rcLoading,rcNoObj,rcUnknown\)"$/
            )
            {
                print STDERR "ERROR\n";
                ++$failures;
                last;
            }
            print STDERR "OK\n";
            last;
        }
    }
}

if (1) {
    print STDERR
        "`test-sra -l <old libngs-sdk>` should report <rcName,rcNotFound>: ";
    my $path = '/net/pan1/sra-test/TOOLKIT/src/NGS/1.2.3/ngs-sdk/ngs-sdk.1.2.3';
    if ($^O =~ /^darwin$/) {
        $path .= '-mac/lib64/libngs-sdk.1.2.3.dylib';
    } else {
        $path .= '-linux/lib64/libngs-sdk.so.1.2.3';
    }
    @_ = `$testsra -l $path`;
    if ($? != 0) {
        print STDERR "ERROR\n";
        ++$failures;
    } else {
        while (1) {
            unless ($_[0] =~ /^dll path="$path"$/) {
                print STDERR "ERROR\n";
                ++$failures;
                last;
            }
            unless ($_[1] =~
           /^KDylibSymbol="RC\(.*rcFS,rcDylib,rcSelecting,rcName,rcNotFound\)"$/
            )
            {
                print STDERR "ERROR\n";
                ++$failures;
                last;
            }
            print STDERR "OK\n";
            last;
        }
    }
}

if (1) {
    print STDERR
        "`test-sra -l <old libncbi-vdb>` should report <rcName,rcNotFound>: ";
    my $path = '/net/pan1/sra-test/TOOLKIT/src/NGS/1.1.1/ngs-sdk/ngs-sdk.1.1.1';
    if ($^O =~ /^darwin$/) {
        $path .= '-mac/lib64/libncbi-vdb.2.5.0.dylib';
    } else {
        $path .= '-linux/lib64/libncbi-vdb.so.2.5.0';
    }
    @_ = `$testsra -l $path`;
    if ($? != 0) {
        print STDERR "ERROR\n";
        ++$failures;
    } else {
        while (1) {
            unless ($_[0] =~ /^dll path="$path"$/) {
                print STDERR "ERROR\n";
                ++$failures;
                last;
            }
            unless ($_[1] =~
           /^KDylibSymbol="RC\(.*rcFS,rcDylib,rcSelecting,rcName,rcNotFound\)"$/
            )
            {
                print STDERR "ERROR\n";
                ++$failures;
                last;
            }
            print STDERR "OK\n";
            last;
        }
    }
}

if (1) {
    print STDERR
        "`test-sra -l <libncbi-vdb.2.5.3>` should report <ncbi-vdb: 2.5.3>: ";
    my $path = '/net/pan1/sra-test/TOOLKIT/src/NGS/1.2.0/ngs-sdk/ngs-sdk.1.2.0';
    if ($^O =~ /^darwin$/) {
        $path .= '-mac/lib64/libncbi-vdb.2.5.3.dylib';
        @_ = `GetPackageVersion $path`;
    } else {
        $path .= '-linux/lib64/libncbi-vdb.so.2.5.3';
        @_ = `$testsra -l $path`;
    }
    if ($? != 0) {
        print STDERR "ERROR\n";
        ++$failures;
    } else {
        while (1) {
            unless ($_[0] =~ /^dll path="$path"$/) {
                print STDERR "ERROR\n";
                ++$failures;
                last;
            }
            unless ($_[1] =~ /^ncbi-vdb: "2.5.3"$/) {
                print STDERR "ERROR\n";
                ++$failures;
                last;
            }
            print STDERR "OK\n";
            last;
        }
    }
}

if (1) {
    print STDERR
        "`test-sra -l <libngs-sdk.1.2.4>` should report <ngs-sdk: 1.2.4>: ";
    my $path = '/net/pan1/sra-test/TOOLKIT/src/NGS/1.2.4/ngs-sdk/ngs-sdk.1.2.4';
    if ($^O =~ /^darwin$/) {
        $path .= '-mac/lib64/libngs-sdk.1.2.4.dylib';
    } else {
        $path .= '-linux/lib64/libngs-sdk.so.1.2.4';
    }
    @_ = `$testsra -l $path`;
    if ($? != 0) {
        print STDERR "ERROR\n";
        ++$failures;
    } else {
        while (1) {
            unless ($_[0] =~ /^dll path="$path"$/) {
                print STDERR "ERROR\n";
                ++$failures;
                last;
            }
            unless ($_[1] =~ /ngs-sdk: "1.2.4"$/) {
                print STDERR "ERROR\n";
                ++$failures;
                last;
            }
            print STDERR "OK\n";
            last;
        }
    }
}

if ($ARGV[0]) {
    print STDERR
        "`test-sra -l <VDB_LIBDIR/libncbi-vdb>` should report <2.7.0>: ";
    my $path = "$ARGV[0]/";
    if ($^O =~ /^darwin$/) {
        $path .= 'libncbi-vdb.dylib';
        @_ = `GetPackageVersion $path`;
    } else {
        $path .= 'libncbi-vdb.so';
        @_ = `$testsra -l $path`;
    }
    if ($? != 0) {
        print STDERR "ERROR\n";
        ++$failures;
    } else {
        while (1) {
            unless ($_[0] =~ /^dll path="$path"$/) {
                print STDERR "ERROR\n";
                ++$failures;
                last;
            }
            unless ($_[1] =~ /^ncbi-vdb: "2.7.0"$/) {
                print STDERR "ERROR\n";
                ++$failures;
                last;
            }
            print STDERR "OK\n";
            last;
        }
    }
}

if ($ARGV[1]) {
    open IN, "ngs.version" or die;
    my $version = <IN>;
    close IN;
    print STDERR
        "`test-sra -l <NGS_LIBDIR/libngs-sdk>` should report <$version>: ";
    my $path = "$ARGV[1]/";
    if ($^O =~ /^darwin$/) {
        $path .= 'libngs-sdk.dylib';
    } else {
        $path .= 'libngs-sdk.so';
    }
    @_ = `$testsra -l $path`;
    if ($? != 0) {
        print STDERR "ERROR\n";
        ++$failures;
    } else {
        while (1) {
            unless ($_[0] =~ /^dll path="$path"$/) {
                print STDERR "ERROR\n";
                ++$failures;
                last;
            }
            unless ($_[1] =~ /^ngs-sdk: "$version"$/) {
                print STDERR "ERROR\n";
                ++$failures;
                last;
            }
            print STDERR "OK\n";
            last;
        }
    }
}

if ($failures) {
    print STDERR "\n$failures test failed\n";
    exit 1;
} else {
    print STDERR "...all tests of test-sra dll-version passed\n"
}
