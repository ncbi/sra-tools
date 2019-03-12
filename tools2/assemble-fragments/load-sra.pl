#!/usr/bin/perl -w

use strict;
use warnings;
use File::Path qw(remove_tree);
use IO::Pipe;

sub which($);
sub find($@);
sub sra2ir;
sub reorder_ir;
sub filter_ir;
sub generate_contigs;
sub assemble_fragments;
sub reorder_fragments;
sub archive($$);

my %PATH = (
    'sort' => which 'sort',
    'general-loader' => which 'general-loader',
    'kar' => which 'kar',
    'sra2ir' => which 'sra2ir',
    'reorder-ir' => which 'reorder-ir',
    'filter-ir' => which 'filter-ir',
    'summarize-pairs' => which 'summarize-pairs',
    'assemble-fragments' => which 'assemble-fragments',
    'reorder-fragments' => which 'reorder-fragments',
    'include' => $ENV{'INCLUDE'} || 'include',
    'schema' => find('aligned-ir.schema.text', $ENV{'SCHEMA'} || 'schema'),
    'scratch' => $ENV{'TMPDIR'} || $ENV{'TMP'} || './',
);
$PATH{'include'} =~ s/\/*$//;
$PATH{'scratch'} =~ s/\/*$//;

my $outBase = "$PATH{scratch}/load-sra.$$";

my $source = $ARGV[0];
printf "Reading %s ...\n", $source;
my $ir = sra2ir @ARGV;

printf "Sorting ...\n";
my $sorted = reorder_ir $ir;
remove_tree $ir;

printf "Filtering ...\n";
my $filtered = filter_ir $sorted;
remove_tree $sorted;

printf "Generating contiguous regions ...\n";
my $contigs = generate_contigs $filtered;

printf "Assigning fragment alignments ...\n";
my $assembled = assemble_fragments $filtered, $contigs;
remove_tree $filtered, $contigs;

printf "Sorting fragment alignments ...\n";
my $final = reorder_fragments $assembled;
rename "$assembled/tbl/CONTIGS", "$final/tbl/CONTIGS";
remove_tree $assembled;

$source =~ s/.sra$//;
printf "Archiving final result %s ...\n", "$source.result.vdb";
archive "$source.result.vdb", $final;
remove_tree $final;
exit 0;

sub which($)
{
    my $wanted = $_[0];
    for (split(/:/, $ENV{PATH})) {
        my $exe = "$_/$wanted";
        if (-x $exe) {
            chomp($exe = `ls $exe`);
            return $exe;
        }
    }
    die "no $wanted in $ENV{PATH}";
}

sub find($@)
{
    my $wanted = shift;
    for (@_) {
        my $file = "$_/$wanted";
        if (-r $file) {
            chomp($file = `ls $file`);
            return $file;
        }
    }
    die "$wanted not found";
}

sub general_loader($)
{
    exec { $PATH{'general-loader'} } 'general-loader', '--log-level=err', "--include=$PATH{include}", "--schema=$PATH{schema}", "--target=$_[0]";
    die "can't exec general-loader: $!";
}

sub run_loader
{
    my $target = shift;
    my $tool = shift;
    my $failed = '';
    
    if (0) {
        local $\ = "\n";
        local $, = " ";
        print $tool, @_, '>', $target;
    }
    
    my $Wpid = open(TO_KID, '|-') // die "can't fork: $!";
    if ($Wpid == 0) { general_loader $target }
    
    my $Ppid = fork() // die "can't fork: $!";
    if ($Ppid == 0) {
        open STDOUT, ">&TO_KID" or die "can't redirect STDOUT: $!";
        exec { $PATH{$tool} } $tool, @_;
        die "can't exec $tool: $!";
    }
    
    waitpid $Ppid, 0;
    $failed = $tool if $? != 0;
    
    close TO_KID;
    $failed = 'general-loader' if $? != 0;
    
    die "$failed failed" if $failed;
    
    return $target;
}

sub sra2ir
{
    return run_loader "$outBase.IR", 'sra2ir', @_;
}

sub reorder_ir
{
    return run_loader "$outBase.sorted", 'reorder-ir', @_;
}

sub filter_ir
{
    return run_loader "$outBase.filtered", 'filter-ir', @_;
}

sub assemble_fragments
{
    return run_loader "$outBase.fragments", 'assemble-fragments', @_;
}

sub reorder_fragments
{
    return run_loader "$outBase.result", 'reorder-fragments', @_;
}

sub generate_contigs
{
    my $target = "$outBase.contigs";
    my $failed = '';
    
    my $GLpid = open(TO_GL, '|-') // die "can't fork: $!";
    if ($GLpid == 0) { general_loader $target }
    
    my $REDUCEpid = open(TO_REDUCE, '|-') // die "can't fork: $!";
    if ($REDUCEpid == 0) {
        open STDOUT, ">&TO_GL" or die "can't redirect STDOUT: $!";
        exec { $PATH{'summarize-pairs'} } 'summarize-pairs', 'reduce', '-';
        die "can't exec summarize-pairs: $!";
    }
    
    my $SORTpid = open(TO_SORT, '|-') // die "can't fork: $!";
    if ($SORTpid == 0) {
        open STDOUT, ">&TO_REDUCE" or die "can't redirect STDOUT: $!";
        exec { $PATH{'sort'} } 'sort', '-k1,1', '-k2n,2n', '-k3n,3n', '-k4,4', '-k5n,5n', '-k6n,6n';
        die "can't exec sort: $!";
    }
    
    my $MAPpid = fork() // die "can't fork: $!";
    if ($MAPpid == 0) {
        open STDOUT, ">&TO_SORT" or die "can't redirect STDOUT: $!";
        exec { $PATH{'summarize-pairs'} } 'summarize-pairs', 'map', @_;
        die "can't exec summarize-pairs: $!";
    }
    
    waitpid $MAPpid, 0;
    $failed = 'summarize-pairs map' if $? != 0;
    
    close TO_SORT;
    $failed = 'sort' if $? != 0;
    
    close TO_REDUCE;
    $failed = 'summarize-pairs reduce' if $? != 0;
    
    close TO_GL;
    $failed = 'general-loader' if $? != 0;
    
    die "$failed failed" if $failed;
    
    return $target;
}

sub archive($$)
{
    my $kid = fork() // die "can't fork: $!";
    if ($kid == 0) {
        exec { $PATH{'kar'} } 'kar', '--force', '-c', $_[0], '-d', $_[1];
        die "can't exec kar: $!";
    }
    waitpid $kid, 0;
    die "kar failed" unless $? == 0;
}
