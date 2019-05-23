#!/usr/bin/env perl

use v5.16;
use strict;
use warnings;
use IO::Handle;
use IO::File;
use File::Spec;
use File::Temp qw{ tempfile };
use JSON::PP;
use LWP;
use URI;
use XML::LibXML;
use Data::Dumper;

use constant { TRUE => !0, FALSE => !!0 };
use constant EXIT_CODE_TRY_NEXT_SOURCE => 9; ### TODO: UPDATE TO CORRECT CODE AFTER TOOLS ARE UPDATED
use constant {
    REAL_SAM_DUMP => 'sam-dump-orig',
    REAL_FASTQ_DUMP => 'fastq-dump-orig',
    REAL_FASTERQ_DUMP => 'fasterq-dump-orig',
    REAL_SRA_PILEUP => 'sra-pileup-orig',
    REAL_PREFETCH => 'prefetch-orig',
    REAL_SRAPATH => 'srapath-orig'
};

my ($selfvol, $selfdir, $basename) = File::Spec->splitpath($0);
my $selfpath = File::Spec->rel2abs(File::Spec->catpath($selfvol, $selfdir, ''));

sub loadConfig;
my %config = %{loadConfig()};

delete $ENV{VDB_RUN_URL};
delete $ENV{VDB_CACHE_URL};

goto RUNNING_AS_FASTQ_DUMP      if $basename eq 'fastq-dump';
goto RUNNING_AS_FASTERQ_DUMP    if $basename eq 'fasterq-dump';
goto RUNNING_AS_SAM_DUMP        if $basename eq 'sam-dump';
goto RUNNING_AS_PREFETCH        if $basename eq 'prefetch';
goto RUNNING_AS_SRAPATH         if $basename eq 'srapath';
goto RUNNING_AS_SRA_PILEUP      if $basename eq 'sra-pileup';

goto RUN_TESTS if $ENV{DRIVER_TOOL_RUN_TESTS};

sub usage();
sub help(@);
sub info(@);
sub extract(@);

usage() unless @ARGV;

$_ = shift;
help(@ARGV) if /^help$/;
info(@ARGV) if /^info$/;
extract('-format=fastq', @ARGV) if /^fastq$/;
extract('-format=sam', @ARGV) if /^sam$/;
unshift @ARGV, $_;
usage();
die "unreachable";

sub expandAllAccessions(@);
sub resolveAccessionURLs($);
sub parseArgv($\@\@\%\%@);
sub help_path($$);
sub which($);

my $paramsFile;
END {
    unlink $paramsFile if $paramsFile;
}

#** \brief: runs tool on list of accessions
###
### After args parsing, this is called to do the meat of the work.
### Accession can be any kind of SRA accession.
###
### \param: tool name, e.g. fastq-dump
### \param: full path to tool, e.g. /path/to/fastq-dump-orig
### \param: tool parameters; arrayref
### \param: the list of accessions to process
sub processAccessions($$\@@)
{
    my $toolname = shift;
    my $toolpath = shift;
    my $params = shift;
    my @runs = expandAllAccessions(@_);
    
    foreach my $run (@runs) {
        my @sources = resolveAccessionURLs($run);

        foreach (@sources) {
            my ($runURL, $cacheURL) = @$_{'url', 'cache'};
            
            # print STDERR "# running $toolpath with runURL='$runURL' and ".($cacheURL ? "cacheURL='$cacheURL'" : "no cacheURL").".\n";
            # print STDERR "# $tool ".join(", ", @_)." $accession\n";

            my $kid = fork(); die "can't fork: $!" unless defined $kid;            
            if ($kid == 0) {
                $ENV{VDB_RUN_URL} = $runURL // '';
                $ENV{VDB_CACHE_URL} = $cacheURL // '';
    
                exec {$toolpath} $toolname, @$params, $run;
                die "can't exec $toolname: $!";
            }
            waitpid($kid, 0);

            last if $? == 0; # SUCCESS! process the next run
            exit ($? >> 8) if ($? & 0xFF) != 0; # abnormal end
            exit ($? >> 8) unless ($? >> 8) == EXIT_CODE_TRY_NEXT_SOURCE; # it's an error we can't handle
        }
    }
    exit 0;
}

sub expandAccession($);

### \brief: turns a list of accessions into constituent run accessions
###
### \param: array of query accessions
###
### \return: array of run accessions, array is uniqued, in order of first encounter
###
### currently, this is a stub that only works for run accessions
sub expandAllAccessions(@)
{
    my @rslt;
    my %seen = ();

    while (@_) {
        local $_ = shift;
        next if $seen{$_};
        $seen{$_} = TRUE;
        
        # check for ordinary run accessions, e.g. SRR000001 ERR000001 DRR000001
        if (/^[DES]RR\d+$/) {
            push @rslt, $_;
            next;
        }
        # check if it is a file
        if (-f) {
            push @rslt, $_;
            next;
        }
        # see if it can be expanded into run accessions
        my @expanded = expandAccession($_);
        if (@expanded) {
            push @_, @expanded;
        }
        else {
            print STDERR "nothing found for $_\n";
        }
    }
    return @rslt;
}

sub is_vdbcache($)
{
    my $path = URI->new($_[0])->path;
    return !0 if $path =~ /\.vdbcache$/ || $path =~ /\.vdbcache\./;
    return !!0
}

### \brief: turns an accession into URLs
###
### \param: the accession
###
### \return: array of possible pairs of URLs to data and maybe to vdbcache files
###
### currently, this is a stub that just uses srapath
sub resolveAccessionURLs($)
{
    my @obj;
    my @result;
if (!0) {
    my $toolpath = which(REAL_SRAPATH) or help_path(REAL_SRAPATH, TRUE);
    my $kid = open(my $pipe, '-|') // die "can't fork: $!";
    
    if ($kid == 0) {
        exec {$toolpath} 'srapath', qw{ --function names --json }, @_;
        die "can't exec srapath: $!";
    }

    @obj = @{decode_json join '', $pipe->getlines} or goto FALLBACK;
    close($pipe);
}
else {
    @obj = @{decode_json <<'JSON'};
[
{"accession":"SRR954885", "local":"file:///Users/ken/ncbi/public/sra/SRR954885.sra","remote":
[
"https://sra-download.ncbi.nlm.nih.gov/traces/sra11/SRR/000932/SRR954885"
]},
{"accession":"SRR954885", "local":"file:///Users/ken/ncbi/public/sra/SRR954885.sra.vdbcache","remote":
[
"https://sra-download.ncbi.nlm.nih.gov/traces/sra11/SRR/000932/SRR954885.vdbcache"
]}
]
JSON
}    
    my ($run, $cache);
    for (@obj) {
        my $is_vdbcache;
#        unless ($_->{accession} && $_->{accession} eq $_[0]) {
#            warn "unexpected accession: $_->{accession}";
#            next;
#        }
        if ($_->{local}) {
            $is_vdbcache = is_vdbcache $_->{local}
        }
        for (@{$_->{remote}}) {
            last if $is_vdbcache;
            $is_vdbcache = is_vdbcache $_
        }
        if ($is_vdbcache) {
            warn "vdbcache location was already set" if $cache;
            $cache = $_
        }
        else {
            warn "run location was already set" if $run;
            $run = $_
        }
    }
    if (!defined($run)) {
FALLBACK:
        push @result, {};
        return @result;
    }
    if (!defined($cache)) {
        # there is no vdbcache for this run
        if ($run->{'local'}) {
            # use the local copy of the run
            push @result, { 'url' => $run->{'local'}, 'source' => 'local' };
        }
        else {
            # try all remotes in order
            for (@{$run->{remote}}) {
                my $uri = URI->new($_);
                my $source = 'remote';
                $source = 'ncbi' if $uri->host =~ /ncbi\.nlm\.nih\.gov$/i;
                push @result, { 'url' => $_, 'source' => $source }
            }
        }
        return @result;
    }
    my $default_remote_cache;
    # try ncbi
    for (@{$cache->{'remote'}}) {
        my $host = URI->new($_)->host;
        if ($host =~ /ncbi\.nlm\.nih\.gov$/i) {
            $default_remote_cache = $_;
            last;
        }
    }
    if (!defined($default_remote_cache)) {
        # use first
        $default_remote_cache = $cache->{'remote'}->[0];
    }
    if ($run->{'local'}) {
        # use the local copy of the run and the vdbcache from the default location
        push @result, { 'url' => $run->{'local'}, 'cache' => $cache->{'local'} // $default_remote_cache, 'source' => 'local' };
        return @result;
    }
    # there is no local copy of run
    # try all remotes in order
    # for vdbcache, use same source or use default
    for (@{$run->{remote}}) {
        my $r = $_;
        my $c = $cache->{'local'};
        my $uri = URI->new($r);
        my $source = 'remote';
        $source = 'ncbi' if $uri->host =~ /ncbi\.nlm\.nih\.gov$/i;
        
        for (@{$cache->{'remote'}}) {
            last if $c;
            my $curi = URI->new($_);
            next unless $curi->host eq $uri->host;
            $c = $_;
        }
        push @result, { 'url' => $r, 'cache' => ($c // $default_remote_cache), 'source' => $source };
    }
    return @result;
}

use constant EUTILS_URL => URI->new('https://eutils.ncbi.nlm.nih.gov/entrez/eutils/');

### \brief: run a query with eutils
###
### \param: the LWP::UserAgent
### \param: the search term
###
### \return: array of matching IDs
sub queryEUtils($$)
{
    my $ua = shift;
    my $query = shift;
    my $isAccession = $query =~ m/^[DES]R[APRSX]\d+/;
    my $url = URI->new_abs('esearch.fcgi', EUTILS_URL);
    $url->query_form(retmax => $isAccession ? 200 : 20, retmode => 'json', db => 'sra', term => $query);

    my $res = $ua->get($url); die $res->status_line unless $res->is_success;

    my $obj = decode_json $res->content;
    my $result = $obj->{esearchresult} or die "unexpected response from eutils";
    my $idlist = $result->{idlist} or die "unexpected response from eutils";
    
    return @$idlist;
}

sub unescapeXML($)
{
    $_[0] =~ s/&lt;/</g;
    $_[0] =~ s/&gt;/>/g;
    $_[0] =~ s/&amp;/&/g;
    $_[0]
}

sub isTrue($)
{
    defined($_[0]) ? ($_[0] eq 'true') : undef
}

### \brief: get Run Info for list of IDs
###
### \param: the LWP::UserAgent
### \param: the IDs
###
### \return: array of Run Info
sub getRunInfo($@)
{
    my %response = (
        'submitter' => {},
        'study' => {},
        'organism' => {},
        'sample' => {},
        'experiment' => {},
        'runs' => []
    );
    my $ua = shift;
    return \%response unless @_;
    
    my $parser = XML::LibXML->new( no_network => 1, no_blanks => 1 );
    my $url = URI->new_abs('esummary.fcgi', EUTILS_URL);
    $url->query_form(retmode => 'json', db => 'sra', id => join(',', @_));

    my $res = $ua->get($url); die $res->status_line unless $res->is_success;
    
    my $obj = decode_json $res->content or die "unexpected response from eutils";
    my $result = $obj->{result} or die "unexpected response from eutils";

    for (@_) {
        my $obj = $result->{$_} or die "unexpected response from eutils";
        my $runs = unescapeXML($obj->{runs}) or die "unexpected response from eutils";
        my $exp = unescapeXML($obj->{expxml});
        
        my ($submitter, $experiment, $study, $organism, $sample, $bioProject, $bioSample);
        {
            my $frag = $parser->parse_balanced_chunk($exp) or die "invalid response; unparsable XML";
            for ($frag->findnodes('Submitter')) {
                my $acc = $_->findvalue('@acc');
                $submitter = $acc;
                next if $response{'submitter'}->{$acc};
                $response{'submitter'}->{$acc} = {
                    'accession' => $acc,
                    'center' => $_->findvalue('@center_name') // '',
                    'contact' => $_->findvalue('@contact_name') // '',
                    'lab' => $_->findvalue('@lab_name') // '',
                };
            }
            for ($frag->findnodes('Experiment')) {
                my $acc = $_->findvalue('@acc');
                $experiment = $acc;
                next if $response{'experiment'}->{$acc};
                $response{'experiment'}->{$acc} = {
                    'accession' => $acc,
                    'ver' => $_->findvalue('@ver') // '',
                    'status' => $_->findvalue('@status') // '',
                    'name' => $_->findvalue('@name') // '',
                };
            }
            for ($frag->findnodes('Study')) {
                my $acc = $_->findvalue('@acc');
                $study = $acc;
                next if $response{'study'}->{$acc};
                $response{'study'}->{$acc} = {
                    'accession' => $acc,
                    'name' => $_->findvalue('@name') // '',
                };
            }
            for ($frag->findnodes('Organism')) {
                my $acc = $_->findvalue('@taxid');
                $organism = $acc;
                next if $response{'organism'}->{$acc};
                $response{'organism'}->{$acc} = {
                    'taxid' => $acc,
                    'name' => $_->findvalue('@ScientificName') // '',
                };
            }
            for ($frag->findnodes('Sample')) {
                my $acc = $_->findvalue('@acc');
                $sample = $acc;
                next if $response{'sample'}->{$acc};
                $response{'sample'}->{$acc} = {
                    'accession' => $acc,
                    'name' => $_->findvalue('@name') // '',
                };
            }
            $bioProject = $frag->findvalue('Bioproject/text()') // '';
            $bioSample = $frag->findvalue('Biosample/text()') // '';
        }

        my $frag = $parser->parse_balanced_chunk($runs) or die "invalid response; unparsable XML";
        for my $run ($frag->findnodes('Run')) {
            my $acc = $run->findvalue('@acc') or next;
            my $loaded = $run->findvalue('@load_done');
            my $public = $run->findvalue('@is_public');
            push @{$response{'runs'}}, {
                'accession' => $acc,
                'public' => isTrue($public),
                'loaded' => isTrue($loaded),
                'submitter' => $submitter,
                'experiment' => $experiment,
                'study' => $study,
                'organism' => $organism,
                'sample' => $sample,
                'bioproject' => $bioProject,
                'biosample' => $bioSample,
            };
        }
    }
    return \%response;
}

### \brief: turns an accession into its constituent run accessions
###
### \param: the query accession
###
### \return: array of run accessions
sub expandAccession($)
{
    my $ua = LWP::UserAgent->new( agent => 'sratoolkit'
                                , parse_head => 0
                                , ssl_opts => { verify_hostname => 0 } );
    return map { $_->{accession} } grep { $_->{loaded} && $_->{public} } @{getRunInfo($ua, queryEUtils($ua, $_[0]))->{'runs'}};
}

RUNNING_AS_FASTQ_DUMP:
{
    my $toolpath = which(REAL_FASTQ_DUMP) or help_path(REAL_FASTQ_DUMP, TRUE);
    my %long_arg = (
        '-A' => '--accession',
        '-N' => '--minSpotId',
        '-X' => '--maxSpotId',
        '-W' => '--clip',
        '-M' => '--minReadLen',
        '-R' => '--read-filter',
        '-E' => '--qual-filter',
        '-O' => '--outdir',
        '-Z' => '--stdout',
        '-G' => '--spot-group',
        '-T' => '--group-in-dirs',
        '-K' => '--keep-empty-files',
        '-C' => '--dumpcs',
        '-B' => '--dumpbase',
        '-Q' => '--offset',
        '-F' => '--origfmt',
        '-I' => '--readids',
        '-V' => '--version',
        '-L' => '--log-level',
        '-v' => '--verbose',
        '-+' => '--debug',
        '-h' => '--help',
        '-?' => '--help',
    );
    my %param_has_arg = (
        '--accession' => TRUE,
        '--table' => TRUE,
        '--minSpotId' => TRUE,
        '--maxSpotId' => TRUE,
        '--spot-groups' => TRUE,
        '--minReadLen' => TRUE,
        '--read-filter' => TRUE,
        '--aligned-region' => TRUE,
        '--matepair-distance' => TRUE,
        '--outdir' => TRUE,
        '--offset' => TRUE,
        '--defline-seq' => TRUE,
        '--defline-qual' => TRUE,
        '--log-level' => TRUE,
        '--debug' => TRUE,
        '--dumpcs' => 0, # argument not required
        '--fasta' => 0, # argument not required
    );
    my @params = (); # short params get expanded to long form
    my @args = (); # everything that isn't part of a parameter

    if (!parseArgv('old', @params, @args, %long_arg, %param_has_arg, @ARGV))
    {
        # usage error or user asked for help
        exec {$toolpath} 'fastq-dump', '--help';
        die "can't exec original fastq-dump: $!";
    }
    processAccessions('fastq-dump', $toolpath, @params, @args);
    die "unreachable";
}

RUNNING_AS_FASTERQ_DUMP:
{
    my $toolpath = which(REAL_FASTERQ_DUMP) or help_path(REAL_FASTERQ_DUMP, TRUE);
    my %long_arg = (
        '-o' => '--outfile',
        '-O' => '--outdir',
        '-b' => '--bufsize',
        '-c' => '--curcache',
        '-m' => '--mem',
        '-t' => '--temp',
        '-e' => '--threads',
        '-p' => '--progress',
        '-x' => '--details',
        '-s' => '--split-spot',
        '-S' => '--split-files',
        '-3' => '--split-3',
        '-f' => '--force',
        '-N' => '--rowid-as-name',
        '-P' => '--print-read-nr',
        '-M' => '--min-read-len',
        '-B' => '--bases',
        '-V' => '--version',
        '-L' => '--log-level',
        '-v' => '--verbose',
        '-+' => '--debug',
        '-h' => '--help',
        '-?' => '--help',
    );
    my %param_has_arg = (
        '--outfile' => TRUE,
        '--outdir' => TRUE,
        '--bufsize' => TRUE,
        '--curcache' => TRUE,
        '--mem' => TRUE,
        '--temp' => TRUE,
        '--threads' => TRUE,
        '--min-read-len'=> TRUE,
        '--log-level' => TRUE,
        '--debug' => TRUE,
    );
    my @params = (); # short params get expanded to long form
    my @args = (); # everything that isn't part of a parameter

    if (!parseArgv('new', @params, @args, %long_arg, %param_has_arg, @ARGV))
    {
        # usage error or user asked for help
        exec {$toolpath} 'fasterq-dump', '--help';
        die "can't exec original fasterq-dump: $!";
    }
    processAccessions('fasterq-dump', $toolpath, @params, @args);
    die "unreachable";
}

RUNNING_AS_SAM_DUMP:
{
    my $toolpath = which(REAL_SAM_DUMP) or help_path(REAL_SAM_DUMP, TRUE);
    my %long_arg = (
        '-u' => '--unaligned',
        '-1' => '--primary',
        '-c' => '--cigar-long',
        '-r' => '--header',
        '-n' => '--no-header',
        '-s' => '--seqid',
        '-=' => '--hide-identical',
        '-g' => '--spot-group',
        '-p' => '--prefix',
        '-Q' => '--qual-quant',
        '-V' => '--version',
        '-L' => '--log-level',
        '-v' => '--verbose',
        '-+' => '--debug',
        '-h' => '--help',
        '-?' => '--help',
    );
    my %param_has_arg = (
        '--header-file' => TRUE,
        '--header-comment' => TRUE,
        '--aligned-region' => TRUE,
        '--matepair-distance' => TRUE,
        '--prefix' => TRUE,
        '--qual-quant' => TRUE,
        '--log-level' => TRUE,
        '--debug' => TRUE,
    );
    my @params = (); # short params get expanded to long form
    my @args = (); # everything that isn't part of a parameter

    if (!parseArgv('new', @params, @args, %long_arg, %param_has_arg, @ARGV))
    {
        # usage error or user asked for help
        exec {$toolpath} 'sam-dump', '--help';
        die "can't exec original sam-dump: $!";
    }
    processAccessions('sam-dump', $toolpath, @params, @args);
    die "unreachable";
}

RUNNING_AS_PREFETCH:
{
    my $toolpath = which(REAL_PREFETCH) or help_path(REAL_PREFETCH, TRUE);
    my %long_arg = (
        '-T' => '--type',
        '-t' => '--transport',
        '-N' => '--min-size',
        '-X' => '--max-size',
        '-f' => '--force',
        '-p' => '--progress',
        '-c' => '--check-all',
        '-l' => '--list',
        '-n' => '--numbered-list',
        '-s' => '--list-sizes',
        '-R' => '--rows',
        '-o' => '--order',
        '-a' => '--ascp-path',
        '-o' => '--output-file',
        '-O' => '--output-directory',
        '-V' => '--version',
        '-L' => '--log-level',
        '-v' => '--verbose',
        '-+' => '--debug',
        '-h' => '--help',
        '-?' => '--help',
    );
    my %param_has_arg = (
        '--transport' => TRUE,
        '--min-size' => TRUE,
        '--max-size' => TRUE,
        '--force' => TRUE,
        '--progress' => TRUE,
        '--rows' => TRUE,
        '--order' => TRUE,
        '--ascp-path' => TRUE,
        '--ascp-options' => TRUE,
        '--output-file' => TRUE,
        '--output-directory' => TRUE,
        '--log-level' => TRUE,
        '--debug' => TRUE,
    );
    my @params = (); # short params get expanded to long form
    my @args = (); # everything that isn't part of a parameter

    if (!parseArgv('new', @params, @args, %long_arg, %param_has_arg, @ARGV))
    {
        # usage error or user asked for help
        exec {$toolpath} 'prefetch', '--help';
        die "can't exec original prefetch: $!";
    }
    processAccessions('prefetch', $toolpath, @params, @args);
    die "unreachable";
}

RUNNING_AS_SRAPATH:
{
    my $toolpath = which(REAL_SRAPATH) or help_path(REAL_SRAPATH, TRUE);
    my %long_arg = (
        '-f' => '--function',
        '-t' => '--timeout',
        '-a' => '--protocol',
        '-e' => '--vers',
        '-u' => '--url',
        '-p' => '--param',
        '-r' => '--raw',
        '-d' => '--project',
        '-c' => '--cache',
        '-P' => '--path',
        '-V' => '--version',
        '-L' => '--log-level',
        '-v' => '--verbose',
        '-+' => '--debug',
        '-h' => '--help',
        '-?' => '--help',
    );
    my %param_has_arg = (
        '--function' => TRUE,
        '--timeout' => TRUE,
        '--protocol' => TRUE,
        '--vers' => TRUE,
        '--url' => TRUE,
        '--param' => TRUE,
        '--log-level' => TRUE,
        '--debug' => TRUE,
    );
    my @params = (); # short params get expanded to long form
    my @args = (); # everything that isn't part of a parameter

    if (!parseArgv('new', @params, @args, %long_arg, %param_has_arg, @ARGV))
    {
        # usage error or user asked for help
        exec {$toolpath} 'srapath', '--help';
        die "can't exec original srapath: $!";
    }
    processAccessions('srapath', $toolpath, @params, @args);
    die "unreachable";
}

RUNNING_AS_SRA_PILEUP:
{
    my $toolpath = which(REAL_SRA_PILEUP) or help_path(REAL_SRA_PILEUP, TRUE);
    my %long_arg = (
        '-r' => '--aligned-region',
        '-o' => '--outfile',
        '-t' => '--table',
        '-q' => '--minmapq',
        '-d' => '--duplicates',
        '-p' => '--spotgroups',
        '-e' => '--seqname',
        '-n' => '--noqual',
        '-V' => '--version',
        '-L' => '--log-level',
        '-v' => '--verbose',
        '-+' => '--debug',
        '-h' => '--help',
        '-?' => '--help',
    );
    my %param_has_arg = (
        '--aligned-region' => TRUE,
        '--outfile' => TRUE,
        '--table' => TRUE,
        '--minmapq' => TRUE,
        '--duplicates' => TRUE,
        '--function' => TRUE,        
        '--log-level' => TRUE,
        '--debug' => TRUE,
    );
    my @params = (); # short params get expanded to long form
    my @args = (); # everything that isn't part of a parameter

    if (!parseArgv('new', @params, @args, %long_arg, %param_has_arg, @ARGV))
    {
        # usage error or user asked for help
        exec {$toolpath} 'sra-pileup', '--help';
        die "can't exec original sra-pileup: $!";
    }
    processAccessions('sra-pileup', $toolpath, @params, @args);
    die "unreachable";
}

### \brief: process ARGV using fastq-dump style parsing rules
###
### \param: params   - out, by ref; array of parameters
### \param: args     - out, by ref; array of arguments
### \param: longName - by ref; hash mapping short to long parameter names
### \param: hasArg   - by ref; hash specifying which parameters may take arguments
### \param: argv
sub parseArgvOldStyle(\@\@\%\%@)
{
# parameter may be long or short form
# each parameter must be a separate element (always `-X -C`; never `-XC`)
# parameter arguments must follow parameter as a separate element (never `--param=value` or `-Xvalue`)

    my $params = shift;
    my $args = shift;
    my $longArg = shift;
    my $hasArg = shift;

    @$params = ();
    @$args = ();
    
    while (@_) {
        my $param;
        
        if ($_[0] !~ /^-/) { # ordinary argument
            local $_ = shift;
            push @$args, $_;
            next;
        }
        if ($_[0] =~ /^--/) { # long param
            $param = shift;
        }
        else { # short param
            local $_ = shift;
            $param = $longArg->{$_};
        }
        return FALSE if $param eq '--help';
        if (exists $hasArg->{$param}) {
            if ($hasArg->{$param}) { # param requires an argument
                return FALSE unless @_;
                local $_ = shift;
                push @$params, $param, $_;
                next;
            }
            elsif (@_ && $_[0] !~ /^-/) {
                local $_ = shift;
                push @$params, $param, $_;
                next;
            }
        }
        # this parameter never has an argument or
        # optional argument wasn't given
        push @$params, $param;
    }
    return TRUE;
}

### \brief: parse the options file
###
### \param: filename
###
### \return: array of parsed arguments
sub parseOptionsFile($)
{
    my @rslt = ();
    my $string = '';
    {
        my $fh = IO::File->new($_[0]) or die "can't open option file $_[0]: $!";
        binmode $fh, ':utf8';
        $string = join '', $fh->getlines();
        close $fh;
    }
    my $token = '';
    my $i = 0;
    my $n = length $string;
    my $ws = TRUE;
    my $esc = FALSE;
    my $quote = '';
    
    while ($i < $n) {
        local $_ = substr($string, $i, 1);
        $i += 1;

        goto APPEND if $esc;

        if (/\\/) {
            $esc = TRUE;
            next;
        }

        if ($quote) {
            $quote = '' if $_ eq $quote;
            goto APPEND;
        }

        if (/\s/) {
            next if $ws;
            push @rslt, $token;
            $ws = TRUE;
            $token = '';
            next;
        }
        $ws = FALSE;
        
        $quote = $_ if /['"]/;

    APPEND:
        $token .= $_;
        $esc = 0;
    }
    return @rslt;
}

### \brief: process ARGV
###
### \param: style    - style of args parsing: 'old' = like fastq-dump
### \param: params   - out, by ref; array of parameters
### \param: args     - out, by ref; array of arguments
### \param: longName - by ref; hash mapping short to long parameter names
### \param: hasArg   - by ref; hash specifying which parameters may take arguments
### \param: argv
sub parseArgv($\@\@\%\%@)
{
    {
        my $style = shift;
        goto &parseArgvOldStyle if $style eq 'old';
    }
# parameter may be long or short form
# parameter arguments may follow as a separate element
# parameter arguments may be attached to parameter name with an equals
# short form parameter arguments may be attached directly to parameter
# converts short form to long form
# converts `--param=arg` to `--param arg`

    my $params = shift;
    my $args = shift;
    my $longArg = shift;
    my $hasArg = shift;
    my @fparams = ();

    @$params = ();
    @$args = ();
    
    while (@_) {
        unless ($_[0] =~ /^-/) {
            # ordinary argument
            local $_ = shift;
            push @$args, $_;
            next;
        }

        my $param = shift;
        if ($param =~ /^--option-file(=.+)*/) {
            my $filename = substr($1, 1) // shift or die "file name is required for --option-file";
            push @_, parseOptionFile($filename);
            next;
        }
        if ($param =~ /^--/ ) {
            if ($param =~ /^(--[^=]+)=(.+)$/) {
                # convert `--param=arg` to `--param arg`
                push @$params, $1, $2;
                push @fparams, { param => $1, arg => $2 };
                next;
            }
        }
        else {
            # short form
            if ($param =~ /^(-.)(.+)$/) {
                $param = $longArg->{$1};
                if (exists $hasArg->{$param}) {
                    # convert short form with concatenated arg to long form with separate arg
                    push @$params, $param, $2;
                    push @fparams, { param => $param, arg => $2 };
                    next;
                }
                else {
                    # concatenated short form; put back the remainder
                    unshift @_, "-$2";
                    push @$params, $param;
                    push @fparams, { param => $param, arg => undef };
                    next;
                }
            }
            else {
                $param = $longArg->{$param};
            }
        }
        return FALSE if $param eq '--help';
        if (exists $hasArg->{$param}) {
            if ($hasArg->{$param}) { # argument is required
                return FALSE unless @_;
                local $_ = shift;
                push @$params, $param, $_;
                push @fparams, { param => $param, arg => $_ };
                next;
            }
            elsif (@_ && $_[0] !~ /^-/) { # argument is optional
                local $_ = shift;
                push @$params, $param, $_;
                push @fparams, { param => $param, arg => $_ };
                next;
            }
        }
        # this parameter never has an argument or
        # optional argument wasn't given
        push @$params, $param;
        push @fparams, { param => $param, arg => undef };
    }
    if (@fparams > 10) {
        my $fh;
        ($fh, $paramsFile) = tempfile();
        binmode $fh, ':utf8';
        for (@fparams) {
            my $line = "$_->{param}";
            if (defined $_->{arg}) {
                #if ($_->{arg} =~ /[ \r\n\f\t\v\\"]/) {
                if ($_->{arg} =~ /[ \r\n\f\t\v\\]/) {
                    # $_->{arg} =~ s/\\/\\\\/g;
                    # $_->{arg} =~ s/"/\\"/g;
                    $_->{arg} = '"'.$_->{arg}.'"';
                }
                $line .= " $_->{arg}" if defined $_->{arg};
            }
            $line .= "\n";
            print $fh $line;
        }
        close $fh;
        @$params = ( '--option-file', $paramsFile );
    }
    return TRUE;
}

sub help_path($$)
{
    print "could not find $_[0]";
    print $_[1] ? ", which is part of this software distribution\n" : "\n";
    print <<"EOM";
This can be fixed in one of the following ways:
* adding $_[0] to the directory that contains this tool, i.e. $selfpath
* adding the directory that contains $_[0] to the PATH environment variable
* adding $_[0] to the current directory
EOM
    exit 78; # EX_CONFIG from <sysexits.h>
}

### \brief: check if path+file is an executable
###
### \param: executable name
### \param: directory to examine
###
### \return: path+file if it exists and is executable
sub isExecutable($$)
{
    my ($vol, $dirs, undef) = File::Spec->splitpath($_[1], !0);
    local $_ = File::Spec->catpath($vol, $dirs, $_[0]);
    return (-e && -x) ? $_ : undef;
}

### \brief: like shell `which` but checks more than just PATH
###
### \param: executable name
###
### \return: the full path to executable or undef
sub witch($)
{
    my $exe = $_[0];
    my $fullpath;

    ## first check self directory
    $fullpath = isExecutable($exe, $selfpath);
    return $fullpath if $fullpath;

    ## check PATH
    local $_;
    foreach (File::Spec->path()) {
        $fullpath = isExecutable($exe, $_);
        return $fullpath if $fullpath;
    }

    ## lastly check current directory
    return isExecutable($exe, File::Spec->curdir());
}

### \brief: memo-ized which
###
### \param: executable name
###
### \return: the full path to executable or undef
my %which_mem = ();
sub which($)
{
    $which_mem{$_[0]} = witch($_[0]) unless exists $which_mem{$_[0]};
    return $which_mem{$_[0]};
}

### \brief: the info sub-command
###
### \param: command line ARGS
sub info(@)
{
    help('info') unless @_;
    
    my $before = '';
    my $ua = LWP::UserAgent->new( agent => 'sratoolkit'
                                , parse_head => 0
                                , ssl_opts => { verify_hostname => 0 } );
    foreach my $query (@_) {
        my $info = getRunInfo($ua, queryEUtils($ua, $query)) or goto NOTHING;
        my @runs = sort { 
               $a->{'sample'} cmp $b->{'sample'}
            || ($a->{'organism'} // '') cmp ($b->{'organism'} // '')
            || $a->{'study'} cmp $b->{'study'}
            || $a->{'submitter'} cmp $b->{'submitter'}
            || $a->{'experiment'} cmp $b->{'experiment'}
            || $a->{'accession'} cmp $b->{'accession'}
        } @{$info->{runs}};
        my $nruns = @runs;
        say $before."Runs matching $query: $nruns\n";
        
        my $public = 0; for my $run (@runs) { $public += 1 if $run->{'public'} }
        
        my $last = undef;
        for my $run (@runs) {
            my $before = "\n";
            if ($last) {
                goto SAMPLE     if $last->{'sample'    } && $last->{'sample'    } ne $run->{'sample'    };
                goto ORGANISM   if $last->{'organism'  } && $last->{'organism'  } ne $run->{'organism'  };
                goto STUDY      if $last->{'study'     } && $last->{'study'     } ne $run->{'study'     };
                goto SUBMITTER  if $last->{'submitter' } && $last->{'submitter' } ne $run->{'submitter' };
                goto EXPERIMENT if $last->{'experiment'} && $last->{'experiment'} ne $run->{'experiment'};
            }
            else {
                $before = '';
            SAMPLE:
                say $before."Sample    : $run->{'sample'} ".($info->{'sample'}->{$run->{'sample'}}->{'name'});
                $before = '';
            ORGANISM:
                say $before."Organism  : taxid=$run->{'organism'} ".$info->{'organism'}->{$run->{'organism'}}->{'name'} if $run->{'organism'};
                $before = '';
            STUDY:
                say $before."Study     : $run->{'study'} ".$info->{'study'}->{$run->{'study'}}->{'name'};
                $before = '';
            SUBMITTER:
                say $before."Submitter : $run->{'submitter'} ".($info->{'submitter'}->{$run->{'submitter'}}->{'center'} || $info->{'submitter'}->{$run->{'submitter'}}->{'lab'} || $info->{'submitter'}->{$run->{'submitter'}}->{'contact'});
                $before = '';
            EXPERIMENT:
                say $before."Experiment: $run->{'experiment'} ".($info->{'experiment'}->{$run->{'experiment'}}->{'name'});
                $before = '';

                $last = {
                    'organism' => $run->{'organism'},
                    'study' => $run->{'study'},
                    'submitter' => $run->{'submitter'},
                    'sample' => $run->{'sample'},
                    'experiment' => $run->{'experiment'},
                }
            }
            say "Run       : ".$run->{accession}.($run->{public} ? "" : " - not public data")
        }
        sleep(1);
        $before = "\n=== *** ===\n";
        next;
    NOTHING:
        say "nothing found for '$query'";
    }
    exit 0;
}

### \brief: the extract sub-command
###
### \param: command line ARGS
sub extract(@)
{
    exit 0;
}

### \brief: the help sub-command
###
### \param: command line ARGS
sub help(@)
{
    local $0 = $basename;
    
    goto INFO if $_[0] && $_[0] eq 'info';
    goto FASTQ if $_[0] && $_[0] eq 'fastq';
    goto SAM if $_[0] && $_[0] eq 'sam';
    
    print "Usage: $0 <command> [<args> ...]\n";
    print <<"EOF";
Commands are:
    info  - get info for an SRA accession
    fastq - extract FASTQ from an SRA accession
    sam   - extract SAM from an SRA accession
    
    help <command> - detailed help for <command> or this message
    
About SRA accessions:
    SRA accessions come in several types. Most types are containers of other
    types, except for SRA run accessions which contain sequence data. Only SRA
    run accessions may be extracted. This tool will expand container accessions
    to their constituent run accessions and perform the requested function on
    each.
    
    SRA run accessions can be extracted as FASTQ.
    
    SRA run accessions can be extracted as SAM; if the run contains alignments,
    the alignment information will be in the SAM records for the aligned reads.
    
    SRA does not guarantee that the original read names are preserved. SRA does
    guarantee that the identity of the reads and mate-pairs are preserved.
    
    SRA does not guarantee that the original quality scores are preserved.
    
See also:
    The SRA homepage at NCBI:
    https://trace.ncbi.nlm.nih.gov/Traces/sra/sra.cgi

    The SRA Toolkit homepage, where you may obtain the latest versions of this
    software and related tools:
    https://trace.ncbi.nlm.nih.gov/Traces/sra/sra.cgi?view=software
EOF
    exit 0;

  INFO:
    print <<"EOM";
Usage: $0 info <query> [<query> ...]
Searches the SRA data hierarchy.

The main purpose is to give THE list of runs that will be processed if you try
to extract SRA data using your query. Think of it as sort of a "pre-flight".

This search resolves the hierachy only to the level of experiment. Thus, if you
ask for a run, you will get the info for its experiment, along with the list of
runs in that experiment. However, if you extract data using a run accession, you
will ONLY get data from that run. If you want to extract data for all the runs
in an experiment, you can use the experiment accession.

Please see:
https://www.ncbi.nlm.nih.gov/sra/docs/srasearch/

That webpage contains a more complete explanation of the query syntax and the
results. Using the search on that page allows for building more complicated
queries and can create larger result sets than this tool is capable of
generating.

Except for the Organism record, the format of the response from this command is:
<Label>   : <Accession> <optional Name or Description>

For the Organism record, NCBI's taxonomy ID is used in place of accession.

An example:
\$ $basename info SRR011211
Runs matching SRR011211: 9

Sample    : SRS000605 
Organism  : taxid=9606 Homo sapiens
Study     : SRP000001 Paired-end mapping reveals extensive structural variation in the human genome
Submitter : SRA000197 454MSC
Experiment: SRX002438 454 sequencing of Human HapMap individual NA15510 genomic fragment library
Run       : SRR011203
Run       : SRR011204
Run       : SRR011205
Run       : SRR011206
Run       : SRR011207
Run       : SRR011208
Run       : SRR011209
Run       : SRR011210
Run       : SRR011211

\$


You are not limited to searching for accessions, but searching for accessions
will be more precise and can produce more results.

For example, searching by organism name:
\$ $basename info 'D. rerio'
Runs matching D. rerio: 32

Sample    : ERS1869565 
Organism  : taxid=7955 Danio rerio
Study     : ERP024473 ChIP-seq for Prep on whole zebrafish embryos at 3.5 and 12hpf
Submitter : ERA987158 European Nucleotide Archive
Experiment: ERX2151745 Illumina HiSeq 2500 paired end sequencing; ChIP-seq for Prep on whole zebrafish embryos at 3.5 and 12hpf
Run       : ERR2094311

...
snip
...

Sample    : SRS4054046 
Organism  : taxid=7955 Danio rerio
Study     : SRP169118 Glucocorticoids inhibit macrophage differentiation towards a pro-inflammatory phenotype upon wounding without affecting their migration
Submitter : SRA811328 GEO
Experiment: SRX5021281 GSM3477138: control_amputated_rep3; Danio rerio; RNA-Seq
Run       : SRR8485426
Run       : SRR8485427

\$

*Note* this response is truncated. The actual number is too many for this tool
to handle. For the full proper response, see:
https://www.ncbi.nlm.nih.gov/sra/?term=D.+rerio

EOM
    exit 0;

  FASTQ:
    print <<"EOM";
Usage: $0 fastq <object>
Extract FASTQ from (possibly many) SRA run accessions

THIS IS NOT YET IMPLEMENTED
EOM
    exit 0;

  SAM:
    print <<"EOM";
Usage: $0 info <object>
Extract SAM from (possibly many) SRA run accessions

THIS IS NOT YET IMPLEMENTED
EOM
    exit 0;
}

### \brief: prints basic usage message
###
### \return: does not return; calls exit
sub usage()
{
    print "Usage: $basename <command> [<args> ...]\nTry\n   $basename help\n";
    exit 64; # EX_USAGE from <sysexits.h>
}

sub loadConfig
{
    my %ignore = map { $_ => 1 } qw { /APPNAME /APPPATH /BUILD /HOME /NCBI_HOME /NCBI_SETTINGS /OS /PWD /USER };
    my $toolpath = which('vdb-config') or help_path('vdb-config', TRUE);
    my $kid = open(my $pipe, '-|', $toolpath, '--output=n') or die "can't fork or can't exec vdb-config";
    my %config;
    my @files;
    my $st = 0;
    while (defined(local $_ = <$pipe>)) {
        # print;
        chomp;
        next if /^$/;
        if ($st == 0) {
            $st = 1 if /^<!-- Current configuration -->$/;
            next;
        }
        if ($st == 1) {
            if (/<!-- Configuration files -->/) {
                $st = 2;
                next;
            }
            my ($keypath, $value);
            ($keypath, $value) = ($1, substr($2, 1, -1)) if /^((?:\/[A-Za-z_0-9][-.A-Za-z_0-9]*)+)\s*=\s*(.+)$/; # nodepath regex from config-lex.l
            unless ($keypath) {
                warn "unexpected output from vdb-config: '$_'";
                next
            }
            next if $keypath =~ /^\/VDBCOPY\// or $ignore{$keypath};
            $config{$keypath} = $value;
            next;
        }
        if ($st == 2) {
            if (/<!-- Environment -->/) {
                $st = 3;
                next;
            }
            push @files, $_;
            next;
        }
        if ($st == 3) {
            next;
        }
    }
    # printf("%s = %s\n", $_, $config{$_}) for sort keys %config;
    return \%config;
}

sub aeq(\@\@)
{
    my $n = scalar(@{$_[0]}); return FALSE unless $n == scalar(@{$_[1]});
    for my $i (1 .. $n) {
        return 0 if $_[0]->[$i - 1] ne $_[1]->[$i - 1];
    }
    return 1;
}

RUN_TESTS:
eval <<'TESTS';

use Config;
use Test::Simple tests => 29;
my %long_args = (
    '-h' => '--help',
    '-?' => '--help',
    '-L' => '--log-level',
);
my %param_args = (
    '--log-level' => TRUE,
    '--option-file' => TRUE
);
my @params;
my @args;

# which test
ok( witch($basename), 'found self');

# loadConfig test
ok( %config, 'loaded config' );
ok( $config{'/kfg/arch/bits'} == $Config{ptrsize} << 3, 'arch/bits matches ptrsize' );

# old style args parsing
ok( !parseArgv('old', @params, @args, %long_args, %param_args, qw{ -h }), 'oldstyle: -h returns false' );
ok( !parseArgv('old', @params, @args, %long_args, %param_args, qw{ -? }), 'oldstyle: -? returns false' );
ok( !parseArgv('old', @params, @args, %long_args, %param_args, qw{ --help }), 'oldstyle: --help returns false' );
ok( !parseArgv('old', @params, @args, %long_args, %param_args, qw{ -L }), 'oldstyle: -L without parameter returns false' );
ok( !parseArgv('old', @params, @args, %long_args, %param_args, qw{ --log-level }), 'oldstyle: --log-level without parameter returns false' );

ok( parseArgv('old', @params, @args, %long_args, %param_args, qw{ -L 5 })
    && 0+@params == 2 && $params[0] eq '--log-level' && $params[1] eq '5'
    , 'oldstyle: -L 5' );
ok( parseArgv('old', @params, @args, %long_args, %param_args, qw{ --log-level 5 })
    && 0+@params == 2 && $params[0] eq '--log-level' && $params[1] eq '5'
    , 'oldstyle: --log-level 5' );
ok( parseArgv('old', @params, @args, %long_args, %param_args, qw{ foobar })
    && 0+@params == 0 && 0+@args == 1 && $args[0] eq 'foobar'
    , 'oldstyle: foobar' );
ok( parseArgv('old', @params, @args, %long_args, %param_args, qw{ -L 5 foo bar })
    && 0+@params == 2 && $params[0] eq '--log-level' && $params[1] eq '5'
    && 0+@args == 2 && $args[0] eq 'foo' && $args[1] eq 'bar'
    , 'oldstyle: -L 5 foo bar' );

# new style args parsing
ok( !parseArgv('new', @params, @args, %long_args, %param_args, qw{ -h }), 'newstyle: -h returns false' );
ok( !parseArgv('new', @params, @args, %long_args, %param_args, qw{ -? }), 'newstyle: -? returns false' );
ok( !parseArgv('new', @params, @args, %long_args, %param_args, qw{ --help }), 'newstyle: --help returns false' );
ok( !parseArgv('new', @params, @args, %long_args, %param_args, qw{ --log-level 5 --help }), 'newstyle: --help with other stuff still returns false' );
ok( !parseArgv('new', @params, @args, %long_args, %param_args, qw{ -L }), 'newstyle: -L without parameter returns false' );
ok( !parseArgv('new', @params, @args, %long_args, %param_args, qw{ --log-level }), 'newstyle: --log-level without parameter returns false' );

ok( parseArgv('new', @params, @args, %long_args, %param_args, qw{ -L5 })
    && 0+@params == 2 && $params[0] eq '--log-level' && $params[1] eq '5'
    , 'newstyle: -L5' );
ok( parseArgv('new', @params, @args, %long_args, %param_args, qw{ -L 5 })
    && 0+@params == 2 && $params[0] eq '--log-level' && $params[1] eq '5'
    , 'newstyle: -L 5' );
ok( parseArgv('new', @params, @args, %long_args, %param_args, qw{ --log-level=5 })
    && 0+@params == 2 && $params[0] eq '--log-level' && $params[1] eq '5'
    , 'newstyle: --log-level=5' );
ok( parseArgv('new', @params, @args, %long_args, %param_args, qw{ --log-level 5 })
    && 0+@params == 2 && $params[0] eq '--log-level' && $params[1] eq '5'
    , 'newstyle: --log-level 5' );
ok( parseArgv('new', @params, @args, %long_args, %param_args, qw{ foobar })
    && 0+@params == 0 && 0+@args == 1 && $args[0] eq 'foobar'
    , 'newstyle: foobar' );
ok( parseArgv('new', @params, @args, %long_args, %param_args, qw{ -L 5 foo bar })
    && 0+@params == 2 && $params[0] eq '--log-level' && $params[1] eq '5'
    && 0+@args == 2 && $args[0] eq 'foo' && $args[1] eq 'bar'
    , 'newstyle: -L 5 foo bar' );

# option-file test
my @testargs = qw{ --one --two --three --four --five --six --seven --eight --nine --ten --eleven --twelve };
ok( parseArgv('new', @params, @args, %long_args, %param_args, @testargs)
    && 0+@params == 2 && $params[0] eq '--option-file' && $params[1] eq $paramsFile
    , 'newstyle: many params to option file' );
my @readargs;
for (IO::File->new($paramsFile)->getlines()) {
    chomp; push @readargs, $_;
}
ok( aeq(@readargs, @testargs), 'newstyle: option file matches' );

# accession lookup tests
my @runs;
ok( !(@runs = expandAccession('foobar')), 'lookup: nothing found for foobar' );
sleep(1);
ok( scalar(@runs = expandAccession('SRS107532')) == 1 && $runs[0] eq 'SRR064437', 'lookup: SRS107532 contains SRR064437' );
sleep(1);
ok( scalar(@runs = expandAccession('SRA000001')) == 3, 'lookup: SRA000001 contains 3 runs' );

TESTS
exit 0;
