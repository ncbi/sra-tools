# ============================================================================ #
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
# ============================================================================ #

my ( $BIN_DIR, $VERSION_CHECKER, $VERSION, $NGS_DIR ) = @ARGV;

unless ( $BIN_DIR && $VERSION_CHECKER && $VERSION ) {
    die "Usage: $0 <BIN_DIR> <VERSION_CHECKER> <VERSION>"
}

my $FAILED='';

my $VERBOSE;

if ( $NGS_DIR ) {
    if ( $NGS_DIR eq '-v' ) {
        ++ $VERBOSE
    } else {
        print "Smoke testing ngs tarball in $NGS_DIR ...\n";

        if ( -e "$NGS_DIR/ngs-java/ngs-java.jar" ) {
            print "ngs-java.jar exists\n";
        } else {
            $FAILED = "$FAILED ngs-java.jar doesn't exist;"
        }

        if ( -e "$NGS_DIR/ngs-java/ngs-doc.jar" ) {
            print "ngs-doc.jar exists\n";
        } else {
            $FAILED = "$FAILED ngs-doc.jar doesn't exist;"
        }

        if ( -e "$NGS_DIR/ngs-java/ngs-src.jar" ) {
            print "ngs-src.jar exists\n";
        } else {
            $FAILED = "$FAILED ngs-src.jar doesn't exist;"
        }

        if ( $FAILED ) {
            print "Failed: $FAILED\n";
            exit 1
        }

        print "Ngs tarball smoke test successful\n"
    }
} else {
    print "Skipped smoke testing ngs tarball.\n";
}

print "\n";

chdir $BIN_DIR or die;
@_ = glob ( '*e' );

my @TOOLS;
foreach ( @_ ) {
    next if ( /^.*DumpReferenceFASTA.exe$/ );
    next if ( /^.*samview-util.exe$/ );
    next if ( /^.*sratools.exe$/ );
    next if ( /^.*vdb-sql.exe$/ );
    next if ( /^.+-driver.exe$/ );
    next if ( /^.+Test.exe$/ );
    push @TOOLS, $_
}

open (F,'>tmp.mkfg') or die "cannot create tmp.mkfg";
print F '/LIBS/GUID = "8badf00d-1111-4444-8888-deaddeadbeef"';
close F or die "cannot close tmp.mkfg";
$ENV{NCBI_SETTINGS} = "tmp.mkfg";

if (-f $VERSION) {
 open(F, $VERSION) or die "Can't read file '$VERSION' [$!]\n";
 $VERSION = <F>;
 chomp $VERSION;
 close (F);
}

print "Smoke testing $VERSION toolkit tarball ...\n\n";

foreach ( @TOOLS ) {
    my $cmd = "$_ -h";
    $FAILED .= " $cmd" if ( RunTool ( $cmd ) );
}
print "\n";

foreach ( @TOOLS ) {
    # All tools are supposed to respond to -V and --version,
    # yet some respond only to --version, or -version, or nothing at all
    my $VERSION_OPTION = '-V';
    if ( /.+blastn_vdb.exe$/     ||
         /.+dump-ref-fasta.exe$/ ||
         /.+sra-blastn.exe$/     ||
	 /.+sra-tblastn.exe$/    ||
	 /.+tblastn_vdb.exe$/      )
    {
        $VERSION_OPTION = '-version';
    }
    $_ .= " $VERSION_OPTION";
    print "$_\n";
    `$_`;
    if ( $? ) {
        $FAILED .= " $cmd";
    } else {
        my $cmd .= "$_ | perl -w $VERSION_CHECKER $VERSION";
	`$cmd`;
	$FAILED .= " $_" if ( $? )
    }
}
print "\n";

# run some key tools, check return codes

my $cmd = 'prefetch SRR002749'            ; $FAILED .= " $cmd" if RunTool($cmd);
`rmdir /s /q        SRR002749`;

$cmd = 'sam-dump SRR002749'               ; $FAILED .= " $cmd" if RunTool($cmd);
$cmd = 'fastq-dump SRR002749 --fasta 0 -Z'; $FAILED .= " $cmd" if RunTool($cmd);
$VERBOSE = 0; # shut up ; to much words will follow
$cmd = 'vdb-dump SRR000001 -R 1'          ; $FAILED .= " $cmd" if RunTool($cmd);
$cmd = 'vdb-config'                       ; $FAILED .= " $cmd" if RunTool($cmd);
$cmd = 'test-sra'                         ; $FAILED .= " $cmd" if RunTool($cmd);
$cmd = 'sra-pileup SRR619505'             ; $FAILED .= " $cmd" if RunTool($cmd);

unlink "tmp.mkfg";

if ( $FAILED ) {
    print "Failed: $FAILED\n";
    exit 1
}

print "\nToolkit tarball smoke test successfull\n";
exit 0;

sub RunTool {
    my ( $cmd ) = @_;
    print "$cmd\n";
    if ( $VERBOSE ) {
        $cmd .= ' 2>&1'
    } else {
        $cmd .= ' >NUL 2>&1'
    }
    my $out = `$cmd`;
    print $out if $VERBOSE;
    return $?;
}
