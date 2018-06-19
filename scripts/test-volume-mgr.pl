#!/usr/bin/perl -w

################################################################################

use strict;

use Cwd     qw ( abs_path getcwd );
use File::Temp   "tempdir";
use Getopt::Long "GetOptions";
use POSIX        "strftime";

my $BIN = abs_path ( './volume-mgr.pl' );

my %OPT;
my $firstTest = 1;
my $testing = '';

# open a next test case
sub TEST {
    $testing = $_[0];
    print "\n" unless ( $firstTest );
    print "TEST CASE: "; print @_; print "\n";
    $firstTest = 0;
    print "\n" if ( $OPT{verbose} );
}

my @options = qw ( verbose );
die "GetOptions: error" unless (GetOptions(\%OPT, @options));

`vdb-dump -h 2>&1`;
die 'error. vdb-dump is not found. Add its directory to $PATH' if ( $? );

my $gap = "dbGaP-8265";

my $cwd = getcwd ();

my ( $tmp, $public, $protected ) = prepare();

my $failures = 0;
my @failures;

################################################################################

{   TEST "Printing help";
    $failures += ok ( success ( '-help' ) );
}

{   TEST "Printing workspaces";
    $failures += ok ( success ( '-workspaces' ) );
}

{   TEST "Printing volumes of public workspace";
    $failures += ok ( success ( '-volumes public' ) );
}

{   TEST "Printing volumes of protected workspace";
    $failures += ok ( success ( "-volumes $gap" ) );
}

my @run = qw ( SRR619128 SRR647774 );
my @row =    (  17728742, 30829867 );
my @size =   (1019271027,1020681871);
my $cache = "$public/sra/$run[0].sra.cache";
my $du = 2528;

my @s_run = qw ( SRR053325 SRR045450 );
my @s_size =   (     13769,    13997 );
my $path = "$public/sra/$s_run[0].sra";

{   TEST "Creating a public sparse file";
    $failures += ok ( not_exist ( $public ) );
    $failures += ok ( vdb_dump ( $run[0], $row[0] ) );
    $failures += ok ( single ( $run[0], $cache, $size[0], $du ) );
}

{   TEST "Downloading a run";
    $failures += ok ( not_exist ( $path, $s_run[0] ) );
    $failures += ok ( prefetch ( $s_run[0] ) );
    $failures += ok ( single  ( $s_run[0], $path, $s_size[0] ) );
    $failures += ok ( srapath ( $s_run[0], $path ) );
}

{   TEST "Adding non-existent public volume should fail";
    my $new = "$tmp/ncbi/public0";
    my $args = "-add public $new";
    $failures += ok ( failure ( $args ) );

    mkdir $new or die;

    TEST "Adding a volume: cannot change root of public workspace";
    $failures += ok ( failure ( "$args -use-new-volume-as-root" ) );

    TEST "Adding a public volume";
    $failures += ok ( success ( $args ) );

    TEST "Existing public sparse file is updated in the old volume";
    $failures += ok ( single   ( $run[0], $cache, $size[0], $du ) );
    $failures += ok ( vdb_dump ( $run[0], $row[0] ) );
    $failures += ok ( single   ( $run[0], $cache, $size[0], $du ) );
    $failures += ok ( vdb_dump ( $run[0], 1 ) );
    $du = increase ( $du, 4328 );
    $failures += ok ( single ( $run[0], $cache, $size[0], $du ) );
    $failures += ok ( single ( $s_run[0], $path, $s_size[0] ) );

    my $vol2 = "$tmp/ncbi/public/sra0";

    TEST "Already downloaded run is found in the old volume";
    $failures += ok ( vdb_dump ( $s_run[0], 1 ) );
    $failures += ok ( single ( $s_run[0], $path, $s_size[0] ) );
    $failures += ok ( srapath ( $s_run[0], $path ) );

    TEST "New public sparse file is created in the new volume";
    my $cache1 = "$vol2/$run[1].sra.cache";
    $failures += ok ( not_exist ( $cache1, $run[1] ) );
    $failures += ok ( vdb_dump ( $run[1], $row[1] ) );

    my $du1 = 1848;
    $failures += ok ( single ( $run[1], $cache1, $size[1], $du1 ) );

    TEST "New runs are downloaded to the new volume";
    my $path1 = "$vol2/$s_run[1].sra";
    $failures += ok ( not_exist ( $path1, $s_run[1] ) );
    $failures += ok ( prefetch ( $s_run[1] ) );
    $failures += ok ( single  ( $s_run[1], $path1, $s_size[1] ) );
    $failures += ok ( srapath ( $s_run[1], $path1 ) );

    TEST "Updating public sparse file in the new volume";
    $failures += ok ( vdb_dump ( $run[1], 1 ) );
    $du1 = increase ( $du1, 2748 );
    $failures += ok ( single ( $run[1], $cache1, $size[1], $du1 ) );

    TEST "Moving public cache files to the active volume";
    $failures += ok ( single ( $run[0], $cache , $size[0], $du  ) );
    $failures += ok ( single ( $run[1], $cache1, $size[1], $du1 ) );
    $failures += ok ( success ( '-move public' ) );
    $failures += ok ( single ( $run[1], $cache1, $size[1], $du1 ) );
    $cache = "$vol2/$run[0].sra.cache";
    $failures += ok ( single ( $run[0], $cache, $size[0], $du ) );

    TEST "Moved public sparse file is updated in the new volume";
    $failures += ok ( vdb_dump ( $run[0], 1772874 ) );
    $du = increase ( $du, 6660 );
    $failures += ok ( single ( $run[0], $cache, $size[0], $du ) );
}

my @p_run = qw ( SRR1996809 SRR1641134 );
my @p_row =    (        809,     12345 );
my @p_size =   ( 1742921259,1743939052 );
my $p_cache = "$protected/sra/$p_run[0].sra.cache";
my $p_du = 4880;

{   TEST "Creating a protected sparse file - will keep the root";
    mkdir $protected or die;
    chdir $protected or die;
    $failures += ok ( not_exist ( "$protected/sra" ) );
    $failures += ok ( vdb_dump ( $p_run[0], $p_row[0] ) );
    $failures += ok ( single ( $p_run[0], $p_cache, $p_size[0], $p_du ) );
}

{   TEST "Adding non-existent protected volume should fail";
    my $new = "$tmp/ncbi/dbGaP";
    my $args = "-add $gap $new";
    $failures += ok ( failure ( $args ) );

    unless ( mkdir $new ) { finish (); die; }

    TEST "Adding a protected volume";
    $failures += ok ( success ( $args ) );

    TEST "Existing protected sparse file updated in the old volume (old root)";
    $failures += ok ( single   ( $p_run[0], $p_cache, $p_size[0], $p_du ) );
    $failures += ok ( vdb_dump ( $p_run[0], $p_row[0] ) );
    $failures += ok ( single   ( $p_run[0], $p_cache, $p_size[0], $p_du ) );
    $failures += ok ( vdb_dump ( $p_run[0], 1 ) );
    $p_du = increase ( $p_du, 5140 );
    $failures += ok ( single   ( $p_run[0], $p_cache, $p_size[0], $p_du ) );

    my $vol2 = "$new/sra0";

    TEST "New protected sparse file is created in the new volume";
    my $cache1 = "$vol2/$p_run[1].sra.cache";
    $failures += ok ( not_exist ( $cache1, $p_run[1] ) );
    $failures += ok ( vdb_dump ( $p_run[1], $p_row[1] ) );

    my $du1 = 4464;
    $failures += ok ( single ( $p_run[1], $cache1, $p_size[1], $du1 ) );

    TEST "Updating protected sparse file in the new volume (old root)";
    $failures += ok ( vdb_dump ( $p_run[1], 1 ) );
    $du1 = increase ( $du1, 4852 );
    $failures += ok ( single ( $p_run[1], $cache1, $p_size[1], $du1 ) );

    TEST "Moving protected cache files to the active volume (old root)";
    $failures += ok ( single ( $p_run[0], $p_cache , $p_size[0], $du  ) );
    $failures += ok ( single ( $p_run[1], $cache1  , $p_size[1], $du1 ) );
    $failures += ok ( success ( "-move $gap" ) );
    $failures += ok ( single ( $p_run[1], $cache1  , $p_size[1], $du1 ) );
    $p_cache = "$vol2/$p_run[0].sra.cache";
    $failures += ok ( single ( $p_run[0], $p_cache, $p_size[0] , $du ) );

    TEST "Moved protected sparse file is updated in the new volume";
    $failures += ok ( vdb_dump ( $p_run[0], 1772874 ) );
    $p_du = increase ( $p_du, 6844 );
    $failures += ok ( single ( $p_run[0], $p_cache, $p_size[0], $p_du ) );
}

finish ();

( $tmp, $public, $protected ) = prepare();
mkdir "$tmp/ncbi" || die;

$p_cache = "$protected/sra/$p_run[0].sra.cache";
$p_du = 4880;

{   TEST "Creating a protected sparse file - will change the root";
    mkdir $protected or die $protected;
    chdir $protected or die;
    $failures += ok ( not_exist ( "$protected/sra" ) );
    $failures += ok ( vdb_dump ( $p_run[0], $p_row[0] ) );
    $failures += ok ( single ( $p_run[0], $p_cache, $p_size[0], $p_du ) );
}

my $new = "$tmp/ncbi/dbGaP";

{   TEST "Adding a volume while changing the root";
    unless ( mkdir $new ) { finish (); die; }
    my $args = "-add $gap $new -use-new-volume-as-root";
    $failures += ok ( success ( $args ) );
}

{   TEST "Protected workspace has changed";
    $failures += ok ( fail_vdb_dump ( $p_run[0], $p_row[0] ) );

    chdir $new or die;

    TEST "Existing protected sparse file updated in the old volume (new root)";
    $failures += ok ( single ( $p_run[0], $p_cache, $p_size[0], $p_du ) );
    $failures += ok ( vdb_dump ( $p_run[0], $p_row[0] ) );
    $failures += ok ( single   ( $p_run[0], $p_cache, $p_size[0], $p_du ) );
    $failures += ok ( vdb_dump ( $p_run[0], 1 ) );
    $p_du = increase ( $p_du, 5140 );
    $failures += ok ( single   ( $p_run[0], $p_cache, $p_size[0], $p_du ) );

    my $vol2 = "$new/sra0";

    TEST "New protected sparse file is created in the new volume";
    my $cache1 = "$vol2/$p_run[1].sra.cache";
    $failures += ok ( not_exist ( $cache1, $p_run[1] ) );
    $failures += ok ( vdb_dump ( $p_run[1], $p_row[1] ) );

    my $du1 = 4464;
    $failures += ok ( single ( $p_run[1], $cache1, $p_size[1], $du1 ) );

    TEST "Updating protected sparse file in the new volume (new root)";
    $failures += ok ( vdb_dump ( $p_run[1], 1 ) );
    $du1 = increase ( $du1, 4852 );
    $failures += ok ( single ( $p_run[1], $cache1, $p_size[1], $du1 ) );

    TEST "Moving protected cache files to the active volume (new root)";
    $du = 5140;
    $failures += ok ( single ( $p_run[0], $p_cache , $p_size[0], $du  ) );
    $failures += ok ( single ( $p_run[1], $cache1  , $p_size[1], $du1 ) );
    $failures += ok ( success ( "-move $gap" ) );
    $failures += ok ( single ( $p_run[1], $cache1  , $p_size[1], $du1 ) );
    $p_cache = "$vol2/$p_run[0].sra.cache";
    $failures += ok ( single ( $p_run[0], $p_cache, $p_size[0] , $du ) );

    TEST "Moved protected sparse file is updated in the new volume";
    $failures += ok ( vdb_dump ( $p_run[0], 1772874 ) );
    $p_du = increase ( $p_du, 6844 );
    $failures += ok ( single ( $p_run[0], $p_cache, $p_size[0], $p_du ) );
}

finish ();
( $tmp, $public, $protected ) = prepare();
mkdir "$tmp/ncbi" or die;

{   TEST "Adding a public volume twice does nothing";
    $failures += ok ( vdb_dump ( $run[0], $row[0] ) );
    $failures += ok ( prefetch ( $s_run[0] ) );
    my $new = "$tmp/ncbi/public0";
    mkdir $new or die;
    my $args = "-add public $new";

    $failures += ok ( success ( $args ) );

    $failures += ok ( vdb_dump ( $run[1], $row[1] ) );
    $failures += ok ( prefetch ( $s_run[1] ) );

    my $saved = save();

    $failures += ok ( success ( $args ) );

    $failures += ok ( the_same ( $saved,
                                "Adding a public volume twice does nothing" ) );

    TEST "Moving public cache files twice does nothing";
}

finish ();

unless ( $failures ) {
    print "\nSUCCESS\n";
} else {
    print "\n$failures FAILURES in:\n";
    print "$_\n" foreach ( @failures );
    exit 1;
}

print "\nXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX\n\n";

################################################################################


# make sure there is just a single copy of $name in $tmp (global)
# it should be found found in $path (absolute path)
# when $sz is specified - compare its size to $sz
# when $bl is specified - compare $du to output of "du $path"
#
# $name is run accession
# to make sure a single complete run or run cache is found the following is run:
# "find $tmp -name '$name*'
# it should return a single file in all volumes
sub single {
    my ( $name, $path, $sz, $bl ) = @_;

    print "$path should exist... ";
    unless ( -e $path ) {
        print "FAILURE !!!!!!!!!!\n";
        return 1;
    } else {
        print "ok\n";
    }

    my $failures = 0;

    return $failures unless ( $sz );

    print "size of $path should be $sz... ";
    my $s = -s $path;
    if ( $s != $sz ) {
        print "FAILURE ($s) !!!!!!!!!!\n";
        ++ $failures;
    } else {
        print "ok\n";
    }

    return $failures unless ( $bl );

    my $du = $bl * 1024;

    print "space used by $path should be <= $du ( $bl * 1024 )... ";
    my $out = `du $path`; if ( $? ) { finish (); die; }
    unless ( $out =~ /^(\d+)/ ) { finish (); die; }
    my $u = $1 * 1024;
    if ( $u > $du ) {
        chomp $out;
        print "FAILURE ($u > $du): '$out' !!!!!!!!!!\n";
        ++ $failures;
    } else {
        print "ok\n";
    }

    unless ( $du < $sz ) {
        finish ();
        die;
    }

    print "there should be a single copy of '$name' in '$tmp'...";
    $out = `find $tmp -name '$name*' | grep -v vdbcache | wc -l`;
    if ( $? ) {
        finish ();
        die;
    }
    if ( $out eq "1\n") {
        print "ok\n";
    } else {
        ++ $failures;
        print "FAILURE (" . `find $tmp -name '$name*'` . ") !!!!!!!!!!\n";
    }

    $failures;
}


# '$name*' should not be found in $tmp (global)
sub not_found {
    my ( $name ) = @_;

    print "there should not be '$name' in '$tmp'...";
    if ( `find $tmp -name '$name*' | wc -l` eq "0\n") {
        print "ok\n";
        return 0;
    } else {
        ++ $failures;
        return 1;
    }
}


# path should not exist
sub not_exist {
    my ( $path, $name ) = @_;

    my $failures = 0;

    print "'$path' should not exist... ";
    if ( -e $path ) {
        print "FAILURE !!!!!!!!!!\n";
        ++ $failures;
    } else {
        print "ok\n";
    }

    if ( $name ) {
        $failures += not_found ( $name );
    }

    $failures;
}


# execure "./volume-mgr.pl $args"
# if ( $fail ) then faiure is expected
# othervise it should succeed
sub success {
    my ( $args, $fail ) = @_;

    my $cmd = "$BIN $args";

    print "Running $cmd... ";
    my $out = `$cmd`;
    print "\n$out" if ( $OPT{verbose} );
    if ( $fail ) {
        unless ( $? ) {
            print "FAILURE !!!!!!!!!!\n";
            return 1;
        } else {
            print "ok\n";
            return 0;
        }
    } else {
        if ( $? ) {
            print "FAILURE !!!!!!!!!!\n";
            return 1;
        } else {
            print "ok\n";
            return 0;
        }
    }
}


# execure "./volume-mgr.pl $_[0]" and expect it to fail
sub failure { return success ( $_[0], 1 ); }


# execute srapath; compare its output to expected
sub srapath {
    my ( $acc, $path ) = @_;

    my $cmd = "srapath $acc";
    print "running '$cmd'... ";
    my $out = `$cmd`; if ( $? ) { finish (); die; }
    chomp $out;
    print "$out...";
    if ( $out eq $path ) {
        print "ok\n";
        return 0;
    } else {
        print "FAILURE ($out) !!!!!!!!!!\n";
        return 1;
    }
}


# execute $cmd; expect it to succeed
sub run {
    my ( $cmd, $fail ) = @_;

    $cmd .= ' 2>&1' if ( $fail );

    if ( $fail ) {
        print "running '$cmd' should fail... ";
    } else {
        print "running '$cmd'... ";
    }
    `$cmd`;
    if ( $fail ) {
        unless ( $? ) {
            print "FAILURE !!!!!!!!!!\n";
            return 1;
        }
        print "ok\n";
    } else {
        if ( $? ) {
            finish ();
            die;
        }
        print "ok\n";
    }

    0;
}

# execute vdb_dump; expect it to succeed
sub vdb_dump { return run "vdb-dump $_[0] -R $_[1]" }
sub fail_vdb_dump { return run "vdb-dump $_[0] -R $_[1]", 1 }

# execute prefetch; expect it to succeed
sub prefetch { return run "prefetch $_[0]" }


# increase expected 'du' output. make sure it was increased
sub increase {
    my ( $old, $new ) = @_;

    unless ( $new > $old ) { finish (); die; }

    $new;
}


# a wrapper on any check. saves current test name if check failed
sub ok {
    my ( $failure ) = @_;
    push @failures, $testing if ( $failure );
    $failure;
}


# make sure temporaty work directory is removed
sub finish {
    chdir '/';
    `rm -rf $tmp`;
    undef $tmp;
}


# create temporary working directory; and config files; setup environment
sub prepare {
    chdir $cwd || die;

    my @t = localtime();
    my $h = strftime '%H', @t;
    my $m = strftime '%M', @t;
    my $template = sprintf ( 'm%02d%02dXXXXX', $h, $m );
    my $tmp = tempdir ( $template, CLEANUP => 1 );
    $tmp = abs_path $tmp;

    my $kfg = "$tmp/k.kfg";

    my $node = '/repository/remote/main/CGI/resolver-cgi='
             . '\"https://www.ncbi.nlm.nih.gov/Traces/names/names.fcgi\"';
    `echo $node                                       >  $kfg`; die if $?;

    my $public = "$tmp/ncbi/public";
    $node = '/repository/user/main/public';
    `echo '$node/root="$public"'                      >> $kfg`; die if $?;
    `echo '$node/apps/sra/volumes/sraFlat  ="sra"'    >> $kfg`; die if $?;
    `echo '$node/apps/refseq/volumes/refseq="refseq"' >> $kfg`; die if $?;

    my $ticket = '536552F2-D685-43A4-AB79-4B6AEAB3D347';

    $node = "/repository/user/protected/$gap";
    my $protected = "$tmp/ncbi/$gap";
    `echo '$node/root="$protected"'                   >> $kfg`; die if $?;
    `echo '$node/apps/sra/volumes/sraFlat="sra"'      >> $kfg`; die if $?;
    `echo '$node/download-ticket="$ticket"'           >> $kfg`; die if $?;
    `echo '$node/encryption-key="ppJ;t9bQPJy#6v"'     >> $kfg`; die if $?;

    $ENV{VDB_CONFIG}=$tmp;
    $ENV{NCBI_SETTINGS}="$tmp/u.mkfg";

    ( $tmp, $public, $protected );
}


# save the currect state : configuration and workspaces
sub save {
    my $cmd = "vdb-config 2>&1";
    print "running '$cmd'... ";
    my $out = `$cmd`; if ( $? ) { finish (); die; }
    $cmd = "ls -lr $tmp";
    print "'$cmd'... ";
    $out .= `ls -lr $tmp`; if ( $? ) { finish (); die; }
    $out;
}


# compare the state with the saved
sub the_same {
    my ( $saved, $msg ) = @_;

    print "$msg... ";

    if ( save() eq $saved ) {
        print "ok\n";
        return 0;
    } else {
        print "FAILURE !!!!!!!!!!\n";
        return 1;
    }
}


################################################################################
