#!/usr/bin/env perl

use v5.16;
use strict;
use warnings;
use integer;
use Config;
use IO::Handle;
use IO::File;
use File::Spec;
use File::Temp qw{ tempfile };
use JSON::PP;
use XML::LibXML;
use Data::Dumper;

use constant { TRUE => !0, FALSE => !!0 };
use constant VERS_STRING => '2.10.0';
use constant EXIT_CODE_TRY_NEXT_SOURCE => 9; ### TODO: UPDATE TO CORRECT CODE AFTER TOOLS ARE UPDATED
use constant {
    REAL_SAM_DUMP => 'sam-dump-orig',
    REAL_FASTQ_DUMP => 'fastq-dump-orig',
    REAL_FASTERQ_DUMP => 'fasterq-dump-orig',
    REAL_SRA_PILEUP => 'sra-pileup-orig',
    REAL_PREFETCH => 'prefetch-orig',
    REAL_SRAPATH => 'srapath-orig'
};
use constant {
    DEFAULT_RESOLVER_VERSION => '130',
    DEFAULT_RESOLVER_URL => 'https://trace.ncbi.nlm.nih.gov/Traces/sdl/2/retrieve'
};

# if SRATOOLS_DEBUG is truthy, then DEBUG ... will print
my $debug = $ENV{SRATOOLS_DEBUG} ? sub { print STDERR Dumper(@_) } : sub {};
sub DEBUG
{
    goto &$debug
}

# if SRATOOLS_TRACE is truthy, then TRACE ... will print
my $trace = $ENV{SRATOOLS_TRACE} ? sub { print STDERR Dumper(@_) } : sub {};
sub TRACE
{
    goto &$trace
}

# if SRATOOLS_VERBOSE = 1, then LOG(0, ...) will print, but not LOG(1, ...)
# i.e. only log levels less than SRATOOLS_VERBOSE will print
my @loggers;
my $level = eval {
    local $SIG{__WARN__} = sub { }; #disable warning: expecting that it's not defined or not numeric
    0+$ENV{SRATOOLS_VERBOSE}
} // 0;
$level = 5 if $level > 5;
$loggers[0] = sub { print STDERR join("\n", @_)."\n" if @_ };
$loggers[$_] = $loggers[0] for 1 .. 4;
if ($level < 5) {
    for ($level .. 5) {
        $loggers[$_] = sub {};
    }
}
DEBUG \@loggers;
sub LOG($@)
{
    return unless $_[0] <= 5;
    my $level = shift;
    goto &{$loggers[$level]};
}

sub SignalName($);
sub remove_version_string($);

my ($selfvol, $selfdir, $basename) = File::Spec->splitpath($0);
my $selfpath = File::Spec->rel2abs(File::Spec->catpath($selfvol, $selfdir, ''));
my $vers_string = '';
($basename, $vers_string) = remove_version_string $basename;
$vers_string = VERS_STRING unless $vers_string;
TRACE {'selfvol' => $selfvol, 'selfdir' => $selfdir, 'basename' => $basename, 'selfpath' => $selfpath, 'vers_string' => $vers_string };

sub loadConfig;
my %config = %{loadConfig()};
TRACE \%config;

sub getRAMLimit($);
### We should document this
### We can consider the value from the environment
$ENV{VDB_MEM_LIMIT} = getRAMLimit($ENV{VDB_MEM_LIMIT});
LOG 2, "VDB_MEM_LIMIT = $ENV{VDB_MEM_LIMIT}";

goto RUN_TESTS  if $basename eq 'sratools.pl' && ($ARGV[0] // '') eq 'runtests';

delete $ENV{$_} for qw{ VDB_CE_TOKEN VDB_LOCAL_URL VDB_REMOTE_URL VDB_REMOTE_NEED_CE VDB_REMOTE_NEED_PMT VDB_CACHE_URL VDB_CACHE_NEED_CE VDB_CACHE_NEED_PMT VDB_LOCAL_VDBCACHE VDB_REMOTE_VDBCACHE VDB_CACHE_VDBCACHE };

# prefetch and srapath will handle --location themselves
goto RUNNING_AS_PREFETCH        if $basename =~ /^prefetch/;
goto RUNNING_AS_SRAPATH         if $basename =~ /^srapath/;

sub findLocation();
my $location = findLocation();

# these functions don't know location or need it
goto RUNNING_AS_FASTQ_DUMP      if $basename =~ /^fastq-dump/;
goto RUNNING_AS_FASTERQ_DUMP    if $basename =~ /^fasterq-dump/;
goto RUNNING_AS_SAM_DUMP        if $basename =~ /^sam-dump/;
goto RUNNING_AS_SRA_PILEUP      if $basename =~ /^sra-pileup/;

sub usage();
sub help(@);
sub info(@);
sub extract(@);

usage() unless @ARGV;
DEBUG \@ARGV;

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
    LOG(1, "unlinking params file") if $paramsFile;
    unlink $paramsFile if $paramsFile;
}

sub findParameter($@);
sub parameterValue($$);
sub deleteParameterAndValue($$);
sub hasParameter($$);

### \brief: runs tool on list of accessions
###
### After args parsing, this is called to do the meat of the work.
### Accession can be any kind of SRA accession.
###
### \param: user-centric tool name, e.g. fastq-dump
### \param: full path to tool, e.g. /path/to/fastq-dump-orig
### \param: output-file parameter name if unsafe for multi-run output, e.g. '--output-file'
### \param: output-file extension, e.g. '.sam'
### \param: tool parameters; arrayref
### \param: the list of accessions to process
sub processAccessions($$$$\@@)
{
    my $toolname = shift;
    my $toolpath = shift;
    my $unsafeOutputFile = shift;
    my $extension = shift;
    my $params = shift;
    my $overrideOutputFile = FALSE;
    my @runs = expandAllAccessions(@_);
    
    LOG 1, "running $toolname on ".join(' ', @runs);
    
    if (@runs > 1 && $unsafeOutputFile && parameterValue($unsafeOutputFile, $params)) {
        # since we know the user asked that normal tool output go to a file,
        # we can safely use STDOUT to talk to the user here.
        printf <<'FMT', scalar(@runs), $toolname;
You are trying to process %u runs to a single output file, but %s
is not capable of producing valid output from more than one run into a single
file. The following output files will be created instead:
FMT
        printf "\t%s%s\n", $_, $extension for @runs;
        $overrideOutputFile = TRUE;
        deleteParameterAndValue($unsafeOutputFile, $params);
    }
    LOG 1, 'with parameters: '.join(' ', @$params) if @$params;
    
    $ENV{VDB_DRIVER_RUN_COUNT} = ''.scalar(@runs);
    foreach (0 .. $#runs) {
        my $acc = $runs[$_];
        my ($ce_token, @sources) = resolveAccessionURLs($acc);
        
        $ENV{VDB_DRIVER_RUN_CURRENT} = ''.($_ + 1);
        $ENV{VDB_CE_TOKEN} = $ce_token if $ce_token;

        foreach (@sources) {
            my ($run, $vdbcache) = @$_{'run', 'vdbcache'};
            
            LOG 0, sprintf("Reading %s from %s", $acc, $run->{'local'} ? 'local file system' : $run->{'source'});
            LOG 1, sprintf("accession: %s, ce_token: %s, data: {local: '%s', remote: '%s', cache: '%s', need ce: %s, need pmt: %s}, vdbcache: {%s}"
                            , $acc
                            , $ce_token // 'null'
                            , $run->{'local'}
                            , $run->{'url'}
                            , $run->{'cache'}
                            , $run->{'needCE'} ? 'yes' : 'no'
                            , $run->{'needPmt'} ? 'yes' : 'no'
                            , $vdbcache ? sprintf("local: '%s', remote: '%s', cache: '%s', need ce: %s, need pmt: %s"
                                                    , $vdbcache->{'local'}
                                                    , $vdbcache->{'url'}
                                                    , $vdbcache->{'cache'}
                                                    , $vdbcache->{'needCE'} ? 'yes' : 'no'
                                                    , $vdbcache->{'needPmt'} ? 'yes' : 'no'
                                                  )
                                        : ''
                          );
                                                    
            my $kid = fork(); die "can't fork: $!" unless defined $kid;
            if ($kid == 0) {
                $ENV{VDB_REMOTE_URL} = $run->{'url'};
                $ENV{VDB_CACHE_URL} = $run->{'cache'};
                $ENV{VDB_LOCAL_URL} = $run->{'local'};
                $ENV{VDB_SIZE_URL} = $run->{'size'} > 0 ? (''.$run->{'size'}) : '';
                $ENV{VDB_REMOTE_NEED_CE} = '1' if $run->{'needCE'};
                $ENV{VDB_REMOTE_NEED_PMT} = '1' if $run->{'needPmt'};
                $ENV{VDB_LOCAL_VDBCACHE} = '';
                $ENV{VDB_REMOTE_VDBCACHE} = '';
                if ($vdbcache) {
                    $ENV{VDB_REMOTE_VDBCACHE} = $vdbcache->{'url'};
                    $ENV{VDB_CACHE_VDBCACHE} = $vdbcache->{'cache'};
                    $ENV{VDB_LOCAL_VDBCACHE} = $vdbcache->{'local'};
                    $ENV{VDB_SIZE_VDBCACHE} = $vdbcache->{'size'} > 0 ? (''.$vdbcache->{'size'}) : '';
                    $ENV{VDB_CACHE_NEED_CE} = '1' if $vdbcache->{'needCE'};
                    $ENV{VDB_CACHE_NEED_PMT} = '1' if $vdbcache->{'needPmt'};
                }
                if ($overrideOutputFile) {
                    push @$params, $unsafeOutputFile, $acc.$extension
                }
                exec {$toolpath} $0, @$params, $acc; ### tool should run as what user invoked
                die "can't exec $toolname: $!";
            }
            my ($exitcode, $signal, $cored) = do {
                local ($!, $?);
                local $SIG{INT} = sub { kill shift, $kid }; #forward SIGINT to child
                waitpid($kid, 0);
                (($? >> 8), ($? & 0x7F), ($? & 0x80) != 0)
            };
            if ($exitcode == 0 && $signal == 0) { # SUCCESS! process the next run
                LOG 1, sprintf("Processed %s", $acc);
                last
            }
            last if $exitcode == 0 && $signal == 0; # SUCCESS! process the next run
            die sprintf('%s (PID %u) was killed [%s (%u)] (%s core dump was generated)', $toolname, $kid, SignalName($signal), $signal, $cored ? 'a' : 'no') if $signal;
            LOG 1, sprintf("%s (PID %u) quit with exit code %u", $toolname, $kid, $exitcode);
            exit $exitcode unless $exitcode == EXIT_CODE_TRY_NEXT_SOURCE; # it's an error we can't handle
        }
    }
    exit 0;
}

### \brief: runs tool on list of accessions, does not use name resolver
###
### After args parsing, this is called to do the meat of the work.
### Accession can be any kind of SRA accession.
###
### \param: user-centric tool name, e.g. fastq-dump
### \param: full path to tool, e.g. /path/to/fastq-dump-orig
### \param: tool parameters; arrayref
### \param: the list of accessions to process
sub processAccessionsNoResolver($$\@@)
{
    my $toolname = shift;
    my $toolpath = shift;
    my $params = shift;
    my @runs = expandAllAccessions(@_);

    LOG 1, "running $toolname on ".join(' ', @runs);
    LOG 1, 'with parameters: '.join(' ', @$params) if @$params;

    exec {$toolpath} $0, @$params, @runs;
    die "can't exec $toolname: $!";
}

### \brief: find (and remove) location parameter from ARGV
###
### \returns: the value of the parameter or undef
sub findLocation()
{
    my $result = undef;
    for my $i (0 .. $#ARGV) {
        local $_ = $ARGV[$i];
        next unless /^--location/;
        if ($_ eq '--location') {
            (undef, $result) = splice @ARGV, $i, 2;
            die "location parameter requires a value" unless $result;
            LOG 0, "Requesting data from $result";
            last;
        }
        elsif (/^--location=(.+)/) {
            $result = $1;
            LOG 0, "Requesting data from $result";
            splice @ARGV, $i, 1;
            last;
        }
    }
    $result
}

### \brief: search parameter array for a parameter and return its index
###
### NB. this function assumes the parameter array has been
###   validated and canonicalized by the parseArgv function
###
### \param: query, e.g. '--output-file'
### \param: params, parameter array
###
### \returns: the parameter index or undef
sub findParameter($@)
{
    for my $i (1 .. $#_) {
        return $i - 1 if $_[$i] eq $_[0];
    }
    undef
}

### \brief: search parameter array for a parameter and return its value
###
### NB. this function assumes the parameter array has been
###   validated and canonicalized by the parseArgv function
###
### \param: query, e.g. '--output-file'
### \param: params, by ref parameter array
###
### \returns: the parameter value or undef
sub parameterValue($$)
{
    if (defined(my $i = findParameter($_[0], @{$_[1]}))) {
        return $_[1]->[$i + 1]
    }
    undef
}

### \brief: search parameter array for a parameter and remove it and its value
###
### NB. this function assumes the parameter array has been
###   validated and canonicalized by the parseArgv function
###
### \param: query, e.g. '--output-file'
### \param: params, by ref parameter array
sub deleteParameterAndValue($$)
{
    if (defined(my $i = findParameter($_[0], @{$_[1]}))) {
        splice @{$_[1]}, $i, 2;
    }
}

### \brief: search parameter array for a parameter
###
### NB. this function assumes the parameter array has been
###   validated and canonicalized by the parseArgv function
###
### \param: query, e.g. '--output-file'
### \param: params, by ref parameter array
sub hasParameter($$)
{
    defined findParameter($_[0], @{$_[1]})
}

### \brief: runs tool --help
###
### \param: user-centric tool name, e.g. fastq-dump
### \param: full path to tool, e.g. /path/to/fastq-dump-orig
sub toolHelp($$)
{
    my $toolname = shift;
    my $toolpath = shift;
    
    exec {$toolpath} $0, '--help';
    die "can't exec $toolname: $!";
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
        
        # check if it is a file
        if (-f) {
            push @rslt, $_;
            next;
        }
        # check for ordinary run accessions, e.g. SRR000001 ERR000001 DRR000001
        if (/^[DES]RR\d+$/) {
            push @rslt, $_;
            next;
        }
        # check for run accessions with known extensions
        if (/^[DES]RR\d+\.(.+)$/) {
            my $ext = $1;
            if ($ext =~ /^realign$/i) {
                push @rslt, $_;
                next;
            }
        }
        # see if it can be expanded into run accessions
        my @expanded = expandAccession($_);
        if (@expanded) {
            # expanded accessions are push back on to the original list so that
            # they can potentially be recursively expanded
            push @_, @expanded;
        }
        else {
            ### Note: could add it back into the list,
            ### but the tool has no better means to make
            ### sense of the request.
            print STDERR "warn: nothing found for $_\n";
        }
    }
    return @rslt;
}

### \brief: extract URLs from srapath response
###
### \param: the query run accession
### \param: the JSON objects from srapath
###
### \return: array of possible pairs of URLs to data and maybe to vdbcache files
sub extract_from_srapath($@)
{
    my $accession = shift;
    my @result;
    my %index;

    for my $i (0 .. $#_) {
        unless (@{$_[$i]->{remote}}) {
            printf STDERR "warn: name resolver found no remote source for %s.%s\n"
                , $_[$i]->{accession}, $_[$i]->{itemType};
            next;
        }
        $index{$_[$i]->{accession}}->{$_[$i]->{itemType}} = $i;
    }
    unless (defined($index{$accession})) {
        printf STDERR "warn: name resolver found nothing for '%s'\n", $accession;
FALLBACK: # produce an empty response, will cause tool to be run without any URLs
        push @result, {};
        return @result;
    }
    unless (defined($index{$accession}->{'sra'})) {
        printf STDERR "warn: name resolver found no SRA data for '%s'\n", $accession;
        goto FALLBACK;
    }
    my $run = $_[$index{$accession}->{'sra'}];
    my $default_vdbcache;
    
    # find the default vdbcache
    if (defined($index{$accession}->{'vdbcache'})) {
        my $vdbcache = $_[$index{$accession}->{'vdbcache'}];
        my ($local, $cache, $size) = @$vdbcache{'local', 'cache', 'size'};

        # find the vdbcache from ncbi        
        for (@{$vdbcache->{'remote'}}) {
            next unless $_->{'service'} eq 'sra-ncbi';
            $default_vdbcache = {
                  'local' => $local // ''
                , 'url' => $_->{'path'}
                , 'source' => $_->{'service'}
                , 'needCE' => ($_->{'CE-Required'} // '') eq 'true'
                , 'needPmt' => ($_->{'Payment-Required'} // '') eq 'true'
                , 'cache' => $cache // ''
                , 'size' => $size // -1
            };
            last
        }
        if (!defined($default_vdbcache)) {
            # use the first vdbcache
            $default_vdbcache = {
                  'local' => $local // ''
                , 'url' => $vdbcache->{'remote'}->[0]->{'path'}
                , 'source' => $vdbcache->{'remote'}->[0]->{'service'}
                , 'needCE' => ($vdbcache->{'remote'}->[0]->{'CE-Required'} // '') eq 'true'
                , 'needPmt' => ($vdbcache->{'remote'}->[0]->{'Payment-Required'} // '') eq 'true'
                , 'cache' => $cache // ''
                , 'size' => $size // -1
            }
        }
    }
    my ($local, $cache, $size) = @$run{'local', 'cache', 'size'};

    for (@{$run->{'remote'}}) {
        push @result, { 'run' => {
                  'url' => $_->{'path'}
                , 'source' => $_->{'service'}
                , 'needCE' => ($_->{'CE-Required'} // '') eq 'true'
                , 'needPmt' => ($_->{'Payment-Required'} // '') eq 'true'
                , 'size' => $size // -1
                , 'local' => $local // ''
                , 'cache' => $cache // ''
            }
        }
    }
    if (defined($index{$accession}->{'vdbcache'})) {
        my $vdbcache = $_[$index{$accession}->{'vdbcache'}];
        my ($local, $cache, $size) = @$vdbcache{'local', 'cache', 'size'};

        # add vdbcache info to each run result
        for (@result) {
            my $source = $_->{run}->{'source'};
            for my $vc (@{$vdbcache->{'remote'}}) {
                next unless $vc->{'service'} eq $source;
                $_->{'vdbcache'} = {
                      'local' => $local // ''
                    , 'url' => $vc->{'path'}
                    , 'source' => $source
                    , 'needCE' => ($vc->{'CE-Required'} // $_->{'CE-Required'} // '') eq 'true'
                    , 'needPmt' => ($vc->{'Payment-Required'} // $_->{'Payment-Required'} // '') eq 'true'
                    , 'cache' => $cache // ''
                    , 'size' => $size // -1
                };
                last
            }
            next if $_->{'vdbcache'};
            $_->{'vdbcache'} = $default_vdbcache;
        }
    }
    if (!@result && $run->{'local'}) {
        # use the local copy of the run and the default vdbcache (which might be nothing)
        push @result, { 'run' => {
                  'local' => $run->{'local'}
                , 'url' => undef
                , 'source' => 'local'
                , 'needCE' => FALSE
                , 'needPmt' => FALSE
                , 'size' => $run->{'size'} // -1
                , 'cache' => $run->{'cache'} // ''
            },
            'vdbcache' => $default_vdbcache
        }
    }
    return @result;
}

### \brief: turns an accession into URLs
###
### \param: the query accession
###
### \return: array of possible pairs of URLs to data and maybe to vdbcache files
sub resolveAccessionURLs($)
{
    if (-f $_[0]) {
        my @result = ({ 'run' => {
                              'local' => $_[0]
                            , 'url' => ''
                            , 'source' => 'local'
                            , 'needCE' => FALSE
                            , 'needPmt' => FALSE
                            , 'size' => (-s $_[0])
                            , 'cache' => ''
                        },
                        'vdbcache' => undef
                    });
        return (undef, @result);
    }
    
    my @tool_args = ('srapath', qw{ --function names --json }
        , '--vers', $config{'repository/remote/version'} // DEFAULT_RESOLVER_VERSION
        , '--url', $config{'repository/remote/main/SDL.2/resolver-cgi'} // DEFAULT_RESOLVER_URL);
    push @tool_args, ('--location', $location) if $location;
    push @tool_args, $_[0];

    my $toolpath = which(REAL_SRAPATH) or help_path(REAL_SRAPATH, TRUE);
    LOG 2, join(' ', $toolpath, @tool_args);
    my $json = do {
        my ($output, $exitcode) = do {
            use open qw{ :encoding(UTF-8) };
            my $kid = open(my $pipe, '-|') // die "can't fork: $!";
    
            if ($kid == 0) {
                exec {$toolpath} @tool_args;
                die "can't exec srapath: $!";
            }
            my $output = do { local $/; <$pipe> }; # slurp
            close($pipe);
            ($output, $?)
        };
        die "error from srapath:\n$output" if $exitcode;

        LOG 2, $output;
        decode_json($output) or die "unparsable response from srapath:\n$output";
    };
    DEBUG $json;
    ($json->{'CE-Token'}, extract_from_srapath($_[0], @{$json->{'responses'}}));
}

use URI;

### \brief: run a query with eutils
###
### \param: the function to perform
### \param: query parameters
###
### \return: the content of the response
sub queryEUtilsViaCurl($$)
{
    my $tries = 3;
    my $curl = which('curl') or die "no curl in PATH";
    my $url = do {
        my $uri = URI->new("https://eutils.ncbi.nlm.nih.gov/entrez/eutils/$_[0].fcgi");
        $uri->query_form(%{$_[1]});
        $uri->as_string
    };

RETRY:
    my ($output, $exitcode) = do {
        use open qw{ :encoding(UTF-8) };
        my $kid = open(my $pipe, '-|') // die "can't fork: $!";

        if ($kid == 0) {
            exec {$curl} $curl, qw{ --location --header "Accept-Charset: UTF-8" --silent --fail }, $url;
            die "can't exec curl: $!";
        }
        my $output = do { local $/; <$pipe> }; # slurp
        close($pipe);
        my $exitcode = $?;
        ($output, $exitcode)
    };
    return $output if $exitcode == 0;
    if ($tries > 0) {
        LOG 1, 'retrying query a short wait';
        sleep 1 * (4 - $tries);
        $tries -= 1;
        goto RETRY;
    }
    die "failed to retrieve result\n";
}

sub queryEUtils($$)
{
    goto &queryEUtilsViaCurl;
}

### \brief: run a query with eutils
###
### \param: the search term
###
### \return: array of matching IDs
sub esearch($)
{
    my $query = shift;
    my $isAccession = $query =~ m/^[DES]R[APRSX]\d+/;
    my $content = queryEUtils('esearch', {
        retmax => $isAccession ? 200 : 20,
        retmode => 'json', db => 'sra', term => $query });
    my $obj = decode_json $content;
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

sub parseExperimentXML($$\%)
{
    my ($submitter, $experiment, $study, $organism, $sample, $bioProject, $bioSample);
    my $exp = unescapeXML($_[1]);
    LOG 3, $exp;

    my $frag = $_[0]->parse_balanced_chunk($exp) or die "invalid response; unparsable XML: $exp";
    for ($frag->findnodes('Submitter')) {
        my $acc = $_->findvalue('@acc');
        $submitter = $acc;
        next if $_[2]->{'submitter'}->{$acc};
        LOG 2, "Found Submitter: $acc";
        $_[2]->{'submitter'}->{$acc} = {
            'accession' => $acc,
            'center' => $_->findvalue('@center_name') // '',
            'contact' => $_->findvalue('@contact_name') // '',
            'lab' => $_->findvalue('@lab_name') // '',
        };
    }
    for ($frag->findnodes('Experiment')) {
        my $acc = $_->findvalue('@acc');
        $experiment = $acc;
        next if $_[2]->{'experiment'}->{$acc};
        LOG 2, "Found Experiment: $acc";
        $_[2]->{'experiment'}->{$acc} = {
            'accession' => $acc,
            'ver' => $_->findvalue('@ver') // '',
            'status' => $_->findvalue('@status') // '',
            'name' => $_->findvalue('@name') // '',
        };
    }
    for ($frag->findnodes('Study')) {
        my $acc = $_->findvalue('@acc');
        $study = $acc;
        next if $_[2]->{'study'}->{$acc};
        LOG 2, "Found Study: $acc";
        $_[2]->{'study'}->{$acc} = {
            'accession' => $acc,
            'name' => $_->findvalue('@name') // '',
        };
    }
    for ($frag->findnodes('Organism')) {
        my $acc = $_->findvalue('@taxid');
        $organism = $acc;
        next if $_[2]->{'organism'}->{$acc};
        LOG 2, "Found Organism: $acc";
        $_[2]->{'organism'}->{$acc} = {
            'taxid' => $acc,
            'name' => $_->findvalue('@ScientificName') // '',
        };
    }
    for ($frag->findnodes('Sample')) {
        my $acc = $_->findvalue('@acc');
        $sample = $acc;
        next if $_[2]->{'sample'}->{$acc};
        LOG 2, "Found Sample: $acc";
        $_[2]->{'sample'}->{$acc} = {
            'accession' => $acc,
            'name' => $_->findvalue('@name') // '',
        };
    }
    $bioProject = $frag->findvalue('Bioproject/text()') // '';
    $bioSample = $frag->findvalue('Biosample/text()') // '';
    
    ($submitter, $experiment, $study, $organism, $sample, $bioProject, $bioSample);
}

### \brief: get Run Info for list of IDs
###
### \param: the IDs
###
### \return: array of Run Info
sub getRunInfo(@)
{
    my %response = (
        'submitter' => {},
        'study' => {},
        'organism' => {},
        'sample' => {},
        'experiment' => {},
        'runs' => []
    );
    return \%response unless @_;
    
    my $parser = XML::LibXML->new( no_network => 1, no_blanks => 1 );
    my $content = queryEUtils('esummary', { retmode => 'json', db => 'sra', id => join(',', @_)});

    LOG 4, $content;
    my $obj = decode_json $content or die "unexpected response from eutils";
    my $result = $obj->{result} or die "unexpected response from eutils";
    DEBUG $result;

    for (@_) {
        my $obj = $result->{$_} or die "unexpected response from eutils: no result ID $_";
        my ($submitter, $experiment, $study, $organism, $sample, $bioProject, $bioSample)
           = parseExperimentXML($parser, $obj->{expxml}, %response);

        my $runs = unescapeXML($obj->{runs}) or die "unexpected response from eutils: no run XML for ID $_";
        LOG 3, $runs;
        my $frag = $parser->parse_balanced_chunk($runs) or die "invalid response; unparsable run XML";
        for my $run ($frag->findnodes('Run')) {
            my $acc = $run->findvalue('@acc') or next;
            LOG 2, "Found Run: $acc";
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
    return map { $_->{accession} } grep { $_->{loaded} && $_->{public} } @{getRunInfo(esearch($_[0]))->{'runs'}};
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

    if (parseArgv('old', @params, @args, %long_arg, %param_has_arg, @ARGV)) {
        processAccessions('fastq-dump', $toolpath, undef, '.fastq', @params, @args);
    }
    else {
        toolHelp('fastq-dump', $toolpath);
    } 
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

    if (parseArgv('new', @params, @args, %long_arg, %param_has_arg, @ARGV)) {
        processAccessions('fasterq-dump', $toolpath, undef, '.fastq', @params, @args);
    }
    else {
        toolHelp('fasterq-dump', $toolpath);
    }
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
        '--output-file' => TRUE,
        '--output-buffer-size' => TRUE,
        '--cursor-cache' => TRUE,
        '--min-mapq' => TRUE,
        '--rna-splice-level' => TRUE,
        '--rna-splice-log' => TRUE,
        '--log-level' => TRUE,
        '--debug' => TRUE,
    );
    my @params = (); # short params get expanded to long form
    my @args = (); # everything that isn't part of a parameter

    if (parseArgv('new', @params, @args, %long_arg, %param_has_arg, @ARGV)) {
        my $extension = hasParameter('--fastq', \@params) ? '.fastq' : hasParameter('--fasta', \@params) ? '.fasta' : '.sam';
        processAccessions('sam-dump', $toolpath, $extension eq '.sam' ? '--output-file' : undef, $extension, @params, @args);
    }
    else {
        toolHelp('sam-dump', $toolpath);
    }
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
        '--type' => TRUE,
        '--transport' => TRUE,
        '--location' => TRUE,
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

    if (parseArgv('new', @params, @args, %long_arg, %param_has_arg, @ARGV)) {
        processAccessionsNoResolver('prefetch', $toolpath, @params, @args);
    }
    else {
        toolHelp('prefetch', $toolpath);
    }
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
        '-j' => '--json',
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
        '--location' => TRUE,
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

    if (parseArgv('new', @params, @args, %long_arg, %param_has_arg, @ARGV)) {
        processAccessionsNoResolver('srapath', $toolpath, @params, @args);
    }
    else {
        toolHelp('srapath', $toolpath);
    }
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
        '--minmismatch' => TRUE,
        '--merge-dist' => TRUE,
        '--function' => TRUE,        
        '--log-level' => TRUE,
        '--debug' => TRUE,
    );
    my @params = (); # short params get expanded to long form
    my @args = (); # everything that isn't part of a parameter

    if (parseArgv('new', @params, @args, %long_arg, %param_has_arg, @ARGV)) {
        processAccessions('sra-pileup', $toolpath, '--outfile', '.pileup', @params, @args);
    }
    else {
        toolHelp('sra-pileup', $toolpath);
    }
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
    
    LOG 3, 'old form args parsing';
    while (@_) {
        my $param;
        
        LOG 5, "arg: $_[0]";
        if ($_[0] !~ /^-/) {
            LOG 5, "ordinary argument";
            local $_ = shift;
            push @$args, $_;
            next;
        }
        if ($_[0] =~ /^--/) {
            LOG 5, "long form";
            $param = shift;
        }
        else {
            LOG 5, "short form";
            local $_ = shift;
            $param = $longArg->{$_};
            LOG(0, 'invalid arg $_ or missing conversion to long form') unless $param;
            return FALSE unless $param;
        }
        LOG(1, 'request for help') if $param eq '--help';
        return FALSE if $param eq '--help';
        if (exists $hasArg->{$param}) {
            if ($hasArg->{$param}) {
                LOG 5, 'argument is required';
                LOG(0, 'argument is missing!') unless @_;
                return FALSE unless @_;
                local $_ = shift;
                push @$params, $param, $_;
                next;
            }
            elsif (@_ && $_[0] !~ /^-/) {
                LOG 5, 'has optional argument';
                local $_ = shift;
                push @$params, $param, $_;
                next;
            }
        }
        # this parameter never has an argument or
        # optional argument wasn't given
        push @$params, $param;
    }
    LOG 5, 'parameters:', @$params;
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
    LOG 5, 'parsed from options file '.$_[0].':', @rslt;
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
    
    LOG 3, 'normal form args parsing';
    while (@_) {
        LOG 4, "arg: $_[0]";
        unless ($_[0] =~ /^-/) {
            LOG 4, "ordinary argument";
            local $_ = shift;
            push @$args, $_;
            next;
        }

        my $param = shift;
        if ($param =~ /^--option-file(=.+)*/) {
            my $filename = substr($1, 1) // shift or die "file name is required for --option-file";
            LOG 4, "options file: $filename";
            push @_, parseOptionFile($filename);
            next;
        }
        if ($param =~ /^--/ ) {
            LOG 4, "long form";
            if ($param =~ /^(--[^=]+)=(.+)$/) {
                LOG 4, 'convert --param=X to --param X';
                push @$params, $1, $2;
                push @fparams, { param => $1, arg => $2 };
                next;
            }
        }
        else {
            LOG 4, "short form";
            if ($param =~ /^(-.)(.+)$/) {
                $param = $longArg->{$1};
                LOG(0, 'invalid arg $1 or missing conversion to long form') unless $param;
                return FALSE unless $param;
                if (exists $hasArg->{$param}) {
                    LOG 4, 'convert -pX to --param X';
                    push @$params, $param, $2;
                    push @fparams, { param => $param, arg => $2 };
                    next;
                }
                else {
                    LOG 4, 'convert -pqr to --param -qr';
                    unshift @_, "-$2";
                    push @$params, $param;
                    push @fparams, { param => $param, arg => undef };
                    next;
                }
            }
            else {
                LOG 4, 'convert -p to --param';
                $param = $longArg->{$param};
                LOG(0, 'invalid arg or missing conversion to long form') unless $param;
                return FALSE unless $param;
            }
        }
        LOG(1, 'request for help') if $param eq '--help';
        return FALSE if $param eq '--help';
        if (exists $hasArg->{$param}) {
            if ($hasArg->{$param}) {
                LOG 4, 'argument is required';
                LOG(0, 'argument is missing!') unless @_;
                return FALSE unless @_;

                local $_ = shift;
                push @$params, $param, $_;
                push @fparams, { param => $param, arg => $_ };
                next;
            }
            elsif (@_ && $_[0] !~ /^-/) {
                LOG 4, 'has optional argument';
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
        LOG 4, "more than 10 parameters(+arguments); using options file";
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
    LOG 4, 'parameters:', @$params;
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
sub sandwich($)
{
    my $exe = $_[0];
    my $fullpath;

    LOG(4, "looking for $exe in self directory");
    $fullpath = isExecutable($exe, $selfpath);
    return $fullpath if $fullpath;

    LOG(4, "looking for $exe in PATH");
    for my $dir (File::Spec->path()) {
        TRACE { 'directory' => $dir, 'executable' => $exe };
        $fullpath = isExecutable($exe, $dir);
        return $fullpath if $fullpath;
    }

    LOG(4, "looking for $exe in current directory");
    return isExecutable($exe, File::Spec->curdir());
}

sub sandwich_versioned($)
{
    my @vers = split /\./, $vers_string;
    my @try = ($_[0]);
    for (@vers) { push @try, $try[-1].'.'.$_ }
    for (reverse @try) {
        my $fullpath = sandwich($_);
        return $fullpath if $fullpath;
    }
    undef
}

### \brief: memo-ized which
###
### \param: executable name
###
### \return: the full path to executable or undef
my %which_mem = ();
sub which($)
{
    my $cached = exists $which_mem{$_[0]};
    my $result = $cached ? $which_mem{$_[0]} : sandwich_versioned($_[0]);
    $which_mem{$_[0]} = $result unless $cached;
    LOG 4, sprintf('found %s for %s (%s)', $result // '???', $_[0], $cached ? 'cached' : 'first time');
    return $result;
}

### \brief: the info sub-command
###
### \param: command line ARGS
sub info(@)
{
    help('info') unless @_;
    
    my $before = '';
    foreach my $query (@_) {
        my $info = getRunInfo(esearch($query)) or goto NOTHING;
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

sub parseConfig($)
{
    my $content = \$_[0];
    my %ignore = map { $_ => 1 } qw { APPNAME APPPATH BUILD HOME NCBI_HOME NCBI_SETTINGS PWD USER };
    my %config;
    my ($key, $value);
    my ($ws, $st) = (TRUE, 0);
    my $N = length($_[0]) - 1;
    
    for my $i (0 .. $N) {
        local $_ = substr $$content, $i, 1;
        next if $ws && /\s/;
        $ws = FALSE;

        if ($st == 5) {
            goto PARSED if /"/;
            if ($_ eq '\\') {
                $st += 1;
            }
            else {
                $value .= $_;
            }
            next;
        }
        if ($st == 2) {
            if (/\s/) {
                $ws = 1;
                $st += 1;
                next;
            }
            if ($_ ne '=') {
                die "invalid character in key path '$_' while parsing config" unless /[-.A-Za-z_0-9\/]/;
                $key .= $_;
                next;
            }
            $st += 1;
            # fallthrough
        }
        if ($st == 3) {
            die "invalid key path while parsing config" unless /=/;
            $ws = 1;
            $st += 1;
            next;
        }
        if ($st == 0) {
            if (/\#/) {
                $st = 99;
                next;
            }
            die "can't parse config" unless /\//;
            $st += 1;
            next;
        }
        if ($st == 1) {
            die "invalid key path while parsing config" unless /[A-Za-z_0-9]/;
            $key = $_;
            $st += 1;
            next;
        }
        if ($st == 4) {
            die "invalid value while parsing config" unless /"/;
            $value = '';
            $st += 1;
            next;
        }
        if ($st == 6) {
            $value .= $_;
            $st -= 1;
            next;
        }
        if ($st == 99) {
            $st = 0 if $_ eq "\n";
            next;
        }

    PARSED:
        $ws = TRUE;
        $st = 0;
        TRACE {'key' => $key, 'value' => $value};
        next if $key =~ /^VDBCOPY\// or $ignore{$key};
        LOG 4, "parsed key: '$key' and value: '$value'";
        $config{$key} = $value;
    }
    return \%config;
}

sub loadConfig
{
    my $content = '';
    {
        my $toolpath = which('vdb-config') or goto NO_VDB_CONFIG;
        my $kid = open(my $pipe, '-|', $toolpath, '--output=n') or die "can't fork or can't exec vdb-config";
        my @files;
        my $st = 0;
        while (defined(local $_ = <$pipe>)) {
            if ($st == 0) {
                $st = 1 if /^<!-- Current configuration -->$/;
                next;
            }
            if ($st == 1) {
                if (/<!-- Configuration files -->/) {
                    $st = 2;
                    next;
                }
                $content .= $_;
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
        close $pipe;
    }
    return parseConfig($content);
    
NO_VDB_CONFIG:
    warn "configuration not loaded: no vdb-config found";
    return {};
}

sub sysctl_MemTotal()
{
    LOG 4, 'trying sysctl hw.memsize hw.physmem';
    my $result;
    open(SYSCTL, '-|', 'sysctl', qw{ hw.memsize hw.physmem }) or return undef;
    while (defined(local $_ = <SYSCTL>)) {
        chomp;
        LOG 4, 'sysctl '.$_;
        if (/^hw.memsize:\s*(.+)/) {
            local $_ = $1;
            s/\s+$//;
            $result = 0+$_ if /\d+/;
        }
        if (/^hw.physmem:\s*(.+)/) {
            local $_ = $1;
            s/\s+$//;
            $result = 0+$_ if /\d+/;
        }
        last if $result;
    }
    close SYSCTL;
    return $result;
}

sub proc_meminfo_MemTotal()
{
    if (-r '/proc/meminfo') {
        LOG 4, 'reading /proc/meminfo';
        for (IO::File->new('/proc/meminfo')->getlines()) {
            LOG 4, $_;
            if (/^MemTotal:\s*(.+)$/) {
                local $_ = ''.$1;
                s/\s+$//;
                return ((0+$1) << 10) if /(\d+)\s*kB/;
                return 0+$_ if /\d+/;
            }
        }
    }
    return undef;
}

sub windows_MemTotal() {
    undef
}

sub getRAMLimit($)
{
    return $_[0] if $_[0]; # use the value from the environment if truthy

    my $memTotal;
    
    if ($config{'OS'} and $config{'OS'} eq 'linux') {
        LOG 4, 'trying /proc/meminfo then sysctl';
        # try proc/meminfo then try sysctl
        $memTotal = proc_meminfo_MemTotal() // sysctl_MemTotal();
    }
    else {
        LOG 4, 'trying sysctl then /proc/meminfo';
        $memTotal = sysctl_MemTotal() // proc_meminfo_MemTotal();
    }
    return ((0+$memTotal) >> 2) if $memTotal; #default to 1/4 total RAM
    return undef;
}

sub aeq(\@\@)
{
    my $n = scalar(@{$_[0]}); return FALSE unless $n == scalar(@{$_[1]});
    for my $i (1 .. $n) {
        return 0 if $_[0]->[$i - 1] ne $_[1]->[$i - 1];
    }
    return 1;
}

my (@SignalName_name, %SignalName_no);
sub SignalName($)
{
    unless (@SignalName_name) {
        my $i = 0;
        for (split ' ', $Config{sig_name}) {
            s/^(?:SIG)+//;
            $SignalName_no{$SignalName_name[$i] = $_} = $i;
            $i += 1;
        }
    }
    return 'SIG'.$SignalName_name[$_[0]];
}

sub remove_version_string($)
{
    my $vers = '';
    my ($base, @part) = split(/\./, $_[0]);
    local $_;
    for (@part) {
        $vers .= ($vers ? '.' : '') . $_;
        unless (/^\d+$/) {
            $base .= '.'.$vers;
            $vers = '';
        }
    }
    return ($base, $vers);
}


RUN_TESTS:
eval <<'TESTS';

use Test::Simple tests => 47;
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
ok( sandwich($basename), 'found self');

{
    my ($base, $vers) = remove_version_string 'foo';
    ok($base eq 'foo' && $vers eq '', 'remove_version_string: no version string')
}
{
    my ($base, $vers) = remove_version_string 'foo.2';
    ok($base eq 'foo' && $vers eq '2', 'remove_version_string: .d')
}
{
    my ($base, $vers) = remove_version_string 'foo.2.0';
    ok($base eq 'foo' && $vers eq '2.0', 'remove_version_string: .d.d')
}
{
    my ($base, $vers) = remove_version_string 'foo.2.10.0';
    ok($base eq 'foo' && $vers eq '2.10.0', 'remove_version_string: .d.d.d')
}
{
    my ($base, $vers) = remove_version_string 'foo.2.bar';
    ok($base eq 'foo.2.bar' && $vers eq '', 'remove_version_string: .d.D')
}
{
    my ($base, $vers) = remove_version_string 'foo.2.bar.10.0';
    ok($base eq 'foo.2.bar' && $vers eq '10.0', 'remove_version_string: .d.D.d.d')
}

my $multiline = "-----BEGIN-----\n-----END-----\n";
my %testConfig = %{parseConfig('/test/key = "'.$multiline.'"')};
ok( $testConfig{'test/key'} eq $multiline, 'multiline config parsing');

# loadConfig test
ok( %config, 'loaded config' );
ok( $config{'kfg/arch/bits'} == $Config{ptrsize} << 3, 'arch/bits matches ptrsize' );

# Mem Limit test
ok( $ENV{VDB_MEM_LIMIT}, 'VDB_MEM_LIMIT: '.$ENV{VDB_MEM_LIMIT} );

@params = qw{ --foo bar };
ok( !hasParameter('--bar', \@params), 'hasParameter: no --bar');
ok( hasParameter('--foo', \@params), 'hasParameter: has --foo');
ok( !defined parameterValue('--bar', \@params), 'parameterValue: not found');
ok( parameterValue('--foo', \@params) eq 'bar', 'parameterValue: found');
deleteParameterAndValue('--bar', \@params);
ok( @params == 2, 'deleteParameterAndValue: not found');
deleteParameterAndValue('--foo', \@params);
ok( @params == 0, 'deleteParameterAndValue: found and removed');

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

# JSON srapath parsing tests
my $json = decode_json <<"JSON";
[
{"accession": "SRR850901", "itemType": "vdbcache", "itemClass": "run", "size": 17615526, "cache": "$ENV{HOME}/ncbi/public/sra/SRR850901.sra.vdbcache", "remote": [{ "path": "https://sra-download.ncbi.nlm.nih.gov/traces/sra11/SRR/000830/SRR850901.vdbcache", "service": "sra-ncbi" }]},
{"accession": "SRR850901", "itemType": "sra", "itemClass": "run", "size": 323741972, "local":"$ENV{HOME}/ncbi/public/sra/SRR850901.sra", "cache": "$ENV{HOME}/ncbi/public/sra/SRR850901.sra", "remote": [{ "path": "https://sra-download.ncbi.nlm.nih.gov/traces/sra3/SRR/000830/SRR850901", "service": "sra-ncbi" },
{ "path": "https://sra-download.ncbi.nlm.nih.gov/sos/sra-pub-run-2/SRR850901/SRR850901.2", "service": "sra-sos" }]},
{"accession": "SRR850902", "itemType": "vdbcache", "itemClass": "run", "size": 17615526, "cache": "$ENV{HOME}/ncbi/public/sra/SRR850901.sra.vdbcache", "remote": [{ "path": "https://sra-download.ncbi.nlm.nih.gov/traces/sra11/SRR/000830/SRR850901.vdbcache", "service": "sra-ncbi" }]},
{"accession": "SRR850902", "itemType": "sra", "itemClass": "run", "size": 323741972, "cache": "$ENV{HOME}/ncbi/public/sra/SRR850901.sra", "remote": [{ "path": "https://sra-download.ncbi.nlm.nih.gov/traces/sra3/SRR/000830/SRR850901", "service": "sra-ncbi" },
{ "path": "https://sra-download.ncbi.nlm.nih.gov/sos/sra-pub-run-2/SRR850901/SRR850901.2", "service": "sra-sos" }]},
{"accession": "SRR850903", "itemType": "sra", "itemClass": "run", "size": 323741972, "local":"$ENV{HOME}/ncbi/public/sra/SRR850901.sra", "cache": "$ENV{HOME}/ncbi/public/sra/SRR850901.sra", "remote": [{ "path": "https://sra-download.ncbi.nlm.nih.gov/traces/sra3/SRR/000830/SRR850901", "service": "sra-ncbi" },
{ "path": "https://sra-download.ncbi.nlm.nih.gov/sos/sra-pub-run-2/SRR850901/SRR850901.2", "service": "sra-sos" }]},
{"accession": "SRR850904", "itemType": "sra", "itemClass": "run", "size": 323741972, "cache": "$ENV{HOME}/ncbi/public/sra/SRR850901.sra", "remote": [{ "path": "https://sra-download.ncbi.nlm.nih.gov/traces/sra3/SRR/000830/SRR850901", "service": "sra-ncbi" },
{ "path": "https://sra-download.ncbi.nlm.nih.gov/sos/sra-pub-run-2/SRR850901/SRR850901.2", "service": "sra-sos" }]}
]
JSON
my @result;

@result = extract_from_srapath('SRR850901', @{$json});
ok( scalar(@result) == 2 && defined($result[0]->{run}) && defined($result[0]->{vdbcache}) && $result[0]->{run}->{local}
, 'srapath parse JSON: local run; remote vdbcache' );

@result = extract_from_srapath('SRR850902', @{$json});
ok( scalar(@result) == 2
 && defined($result[0]->{run})
 && defined($result[1]->{run})
 && $result[0]->{run}->{url} 
 && $result[1]->{run}->{url} 
 && $result[0]->{run}->{local} eq ''
 && $result[1]->{run}->{local} eq ''
 && $result[0]->{run}->{source} eq 'sra-ncbi' 
 && $result[1]->{run}->{source} eq 'sra-sos'
 && $result[0]->{run}->{cache} eq $result[1]->{run}->{cache} 
 && defined($result[0]->{vdbcache}) 
 && defined($result[1]->{vdbcache}) 
 && $result[0]->{vdbcache}->{url} eq $result[1]->{vdbcache}->{url} 
, 'srapath parse JSON: remote multi-source run, single vdbcache' );

@result = extract_from_srapath('SRR850903', @{$json});
ok( scalar(@result) == 2
 && defined($result[0]->{run})
 && !defined($result[0]->{vdbcache}) 
 && $result[0]->{run}->{local}
, 'srapath parse JSON: local run, no vdbcache' );

@result = extract_from_srapath('SRR850904', @{$json});
ok( scalar(@result) == 2 && defined($result[0]->{run}) && !defined($result[0]->{vdbcache}) && defined($result[1]->{run}) && !defined($result[1]->{vdbcache}) && $result[0]->{run}->{source} eq 'sra-ncbi' && $result[1]->{run}->{source} eq 'sra-sos', 'srapath parse JSON: remote multi-source run, no vdbcache' );

# accession lookup tests
my @runs;
ok( !(@runs = expandAccession('foobar')), 'lookup: nothing found for foobar' );
sleep(1);
ok( scalar(@runs = expandAccession('SRS107532')) == 1 && $runs[0] eq 'SRR064437', 'lookup: SRS107532 contains SRR064437' );
sleep(1);
ok( scalar(@runs = expandAccession('SRA000001')) == 3, 'lookup: SRA000001 contains 3 runs' );

TESTS
exit 0;
