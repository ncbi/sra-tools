#!/usr/bin/perl
#============================================================================
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
#
#

sub lower_case_sym
{
    my $sym;
    my $in = @_ [ 0 ];

    # split at leading lower case
    if ( $in =~ /^([^A-Z]*)([A-Z].*)/ )
    {
        $sym = $1;
        $in = $2;
    }
    else
    {
        return $in;
    }

    while ( $in )
    {
        # split off series of leading upper case
        if ( $in =~ /^([A-Z][A-Z]*)([A-Z][^A-Z].*)/ )
        {
            $sym and $sym .= "_";
            $sym .= lc $1;
            $in = $2;
        }

        # lower case next word
        if ( $in =~ /^([A-Z][^A-Z][^A-Z]*)([A-Z].*)/ )
        {
            $sym and $sym .= "_";
            $sym .= lc $1;
            $in = $2;
        }
        else
        {
            $sym and $sym .= "_";
            $sym .= lc $in;
            $in = '';
        }
    }

    return $sym;
}

my @classes;
$num_classes = 0;

my @inc_paths;
my @src_files;

my $h_out;
my $c_out;
my $module;

while ( $arg = shift )
{
    if ( $arg =~ /-(..*)/ )
    {
        $arg = $1;
        while ( $arg =~ /(.)(.*)/ )
        {
            $sw = $1;
            $arg = $2;

            if ( $sw eq "I" )
            {
                $arg or $arg = shift;
                $arg or die "expected include path";
                push @inc_paths, split ( ':', $arg );
                $arg = '';
            }
            elsif ( $sw eq "i" )
            {
                $arg or $arg = shift;
                $arg or die "expected interface file path";
                push @src_files, split ( ':', $arg );
                $arg = '';
            }
            elsif ( $sw eq "h" )
            {
                $arg or $arg = shift;
                $arg or die "expected h file path";
                $h_out = $arg;
                $arg = '';
            }
            elsif ( $sw eq "c" )
            {
                $arg or $arg = shift;
                $arg or die "expected c file path";
                $c_out = $arg;
                $arg = '';
            }
            elsif ( $sw eq "m" )
            {
                $arg or $arg = shift;
                $arg or die "expected module name";
                $module = $arg;
                $arg = '';
            }
            else
            {
                print STDERR "unrecognized switch: '$sw'\n";
                exit 3;
            }
        }
    }
    else
    {
        if ( $arg =~ /:(..*)/ )
        {
            $classes [ $num_classes ] [ 2 ] = 1;
            $arg = $1;
        }
        else
        {
            $classes [ $num_classes ] [ 2 ] = 0;
        }

        if ( $arg =~ /(.[^=]*)=(..*)/ )
        {
            $classes [ $num_classes ] [ 0 ] = $1;
            $classes [ $num_classes ] [ 1 ] = lower_case_sym $2;
        }
        else
        {
            $classes [ $num_classes ] [ 0 ] = $arg;
            $classes [ $num_classes ] [ 1 ] = lower_case_sym $arg;
        }

        ++ $num_classes;
    }
}

$num_paths = scalar @inc_paths;
$num_files = scalar @src_files;

# validate parameters
$num_classes or die "no object class specified";
$module or die "no module name specified";

# open output files
if ( $h_out )
{
    open $H, ">$h_out" or die "failed to open file '$h_out' for output";
}
if ( $c_out )
{
    open $C, ">$c_out" or die "failed to open file '$c_out' for output";
}
else
{
    $C = STDOUT;
}

if ( $src_file )
{
    open $in, $src_file;
}
else
{
    $in = STDIN;
}

# create function accumulator
my @funcs;
$num_funcs = 0;

# process files
for ( $fidx = 0; $fidx == 0 || $fidx < $num_files; ++ $fidx )
{
    # open files or stdin
    if ( $num_files )
    {
        # file to open
        $src_file = $src_files [ $fidx ];

        # locate it first
        $st = stat $src_file;
        for ( $i = 0; ! $st && $i < $num_paths; ++ $i )
        {
            $inc_path = $inc_paths [ $i ];
            $st = stat "$inc_path/$src_file";
            $st and $src_file = "$inc_path/$src_file";
        }

        # open input file
        open $IN, $src_file or die "failed to open file '$src_file' for input";
    }
    else
    {
        $IN = STDIN;
    }

    while ( $line = <$IN> )
    {
        chomp $line;
        $line =~ s/^\s+//;
        $line =~ s/\s+$//;

        if ( ! $line )
        {
            # skip empty lines
        }
        elsif ( $line =~/^extern.*/ )
        {
            # skip over open of 'extern "C"'
        }
        elsif ( $line =~/^[#}].*/ )
        {
            # skip over preprocessor lines
            # and close of 'extern "C"'
        }
        elsif ( $line =~ /^typedef .*/ )
        {
            # skip over typedefs
        }
        else
        {
            # consume comments here
            while ( $line =~ /^(.*)\/\*(.*)$/ )
            {
                $line = $1;
                $cmt = $2;
                until ( ! $cmt || $cmt =~ /^.*\*\/(.*)$/ )
                {
                    $cmt = <$IN>;
                }

                $line and $1 and $line .= " ";
                $line .= "$1";

                chomp $line;
                $line =~ s/^\s+//;
                $line =~ s/\s+$//;
            }
            if ( ! $line )
            {
                # skip of comment line was empty
            }
            else
            {
                # assemble line
                until ( $line =~ /^(.*;)(.*)$/ )
                {
                    $cont = <$IN>;
                    chomp $cont;
                    $cont =~ s/^\s+//;
                    $cont =~ s/\s+$//;
                    $line .= " $cont";
                }

                # look for a function prototype
                if ( $line =~ /^[A-Z]+_EXTERN ([^\(]+)\s+CC ([A-Za-z_][A-Za-z0-9_]*)\s*\(\s*(.*)\s+\);/ ||
                     $line =~ /^[A-Z]+_EXTERN ([^\(]+\*+)CC ([A-Za-z_][A-Za-z0-9_]*)\s*\(\s*(.*)\s+\);/ ||
                     $line =~ /^([^\(]+)\s+([A-Za-z_][A-Za-z0-9_]*)\s*\(\s*(.*)\s+\);/ ||
                     $line =~ /^([^\(]+\*+)([A-Za-z_][A-Za-z0-9_]*)\s*\(\s*(.*)\s+\);/ )
                {
                    $rt = $1;
                    $func = $2;
                    $formals = $3;

                    $msg = '';
                    $cid = 0;
                    $cls = '';
                    $dual = 0;

                    for ( $i = 0; ! $msg && $i < $num_classes; ++ $i )
                    {
                        $cls = $classes [ $i ] [ 0 ];
                        if ( $func =~ /^$cls(..*)/ )
                        {
                            $cid = $i;
                            $msg = $1;
                            $dual = $classes [ $i ] [ 2 ];
                        }
                    }

                    $penult = '';
                    $vararg = '';
                    $update = 0;
                    $params = '';
                    $tformals = '';

                    $formals2 = '';
                    $tmplist = $formals;
                    while ( $tmplist =~ /([^\(]*\()([^\)]*)(\).*)/ )
                    {
                        $formals2 .= $1;
                        $pparms = $2;
                        $tmplist = $3;
                        $pparms =~ s/,/#/g;
                        $formals2 .= "$pparms";
                    }
                    $formals2 .= $tmplist;

                    @paramlist = split ( ',', $formals2 );
                    while ( $param = shift @paramlist )
                    {
                        # trim parameter
                        $param =~ s/^\s+//;
                        $param =~ s/\s+$//;

                        # catch vararg parameter
                        if ( $param eq '...' )
                        {
                            $vararg = $penult;
                            $params .= ", args";
                            $tformals = '';
                        }
                        else
                        {
                            # correct hidden commas
                            $param =~ s/#/,/g;

                            # strip off vector expr
                            if ( $param =~ /([^\[]*)\s*(\[.*\])$/ )
                            {
                                $param = $1;
                                $vect = $2;
                            }


                            # strip off param name
                            if ( $param =~ /^(.*)\s*(\([^\(]*\))$/ )
                            {
                                $param = $1;
                                $pparms = $2;
                                $param =~ /^(.*)\(\s*CC\s*\*\s*([A-Za-z0-9_]+)\s*\)/ or die "failed to determine name of parameter '$param'";
                                $param = "$1( CC * ) $pparms";
                                $penult = $2;
                            }
                            else
                            {
                                $param =~ /(.*[^A-Za-z0-9_]+)([A-Za-z0-9_]+)$/ or die "failed to determine name of parameter '$param'";
                                $param = $1;
                                $penult = $2;
                            }

                            $params and $params .= ", ";
                            $params .= $penult;
                            

                            # process typedecl
                            $param =~ s/\s+$//;
                            if ( $param =~ /(.*)\s+(\*+)$/ )
                            {
                                $param = "$1$2";
                            }
                            $vect and $param .= " $vect";
                            $tformals and $tformals .= ", ";
                            $tformals .= $param;

                            # finally, decide if the function is for read or update
                            if ( $param =~ /^$cls/ || $param =~ /^struct\s+$cls\*+/ )
                            {
                                $update = $dual;
                            }
                        }
                    }

                    $funcs [ $num_funcs ] [ 0 ] = $cid;
                    $funcs [ $num_funcs ] [ 1 ] = $msg;
                    $funcs [ $num_funcs ] [ 2 ] = $rt;
                    $funcs [ $num_funcs ] [ 3 ] = $formals;
                    $funcs [ $num_funcs ] [ 4 ] = $tformals;
                    $funcs [ $num_funcs ] [ 5 ] = $vararg;
                    $funcs [ $num_funcs ] [ 6 ] = $params;
                    $funcs [ $num_funcs ] [ 7 ] = $update;

                    ++ $num_funcs;
                }
            }
        }
    }

    if ( $num_files )
    {
        close $IN;
        $IN = '';
    }
}

# generate text into arrays
my @msgs;
my @cvt;
my @wmsgs;
my @wvt;
my @decls;
my @thunks;
$mgr = 'mgr';

for ( $num_thunks = 0, $num_ro = 0, $i = 0; $i < $num_funcs; ++ $i )
{
    # extract information
    $cid = $funcs [ $i ] [ 0 ];
    $msg = $funcs [ $i ] [ 1 ];
    $rt = $funcs [ $i ] [ 2 ];
    $formals = $funcs [ $i ] [ 3 ];
    $tformals = $funcs [ $i ] [ 4 ];
    $vararg = $funcs [ $i ] [ 5 ];
    $params = $funcs [ $i ] [ 6 ];
    $update = $funcs [ $i ] [ 7 ];

    $cls = $classes [ $cid ] [ 0 ];
    $prefix = $classes [ $cid ] [ 1 ];
    $dual = $classes [ $cid ] [ 2 ];

    # constructor flag
    $constructor = 3;

    # msg symbol
    $sym = "$cls$msg";
    $tname = "_$sym";

    # vt
    $vt = $module;
    $update == 0 and $vt .= "_cvt";
    $update == 1 and $vt .= "_wvt";

    # vt member
    $vt_mbr = $prefix;
    $vt_mbr .= '_';
    if ( $dual && ( $msg eq "MakeRead" || $msg eq "MakeUpdate" ) )
    {
        # detect special library constructor
        if ( $tformals =~ /^$cls\*\*,/ )
        {
            $constructor = 2;
            $vt = $module . "_cvt";
            $vt_mbr .= "make";
            $tname = "_$cls" . "Make";
        }
        elsif ( $tformals =~ /^const\s+$cls\*\*,/ )
        {
            $constructor = 0;
            $vt_mbr .= "make";
        }
        elsif ( $tformals =~ /^const\s+$cls\*\*$/ )
        {
            $constructor = 1;
            $vt_mbr .= "make";
        }
        else
        {
            $vararg and $vt_mbr .= "v_";
            $vt_mbr .= lower_case_sym $msg;
        }
    }
    else
    {
        $vararg and $vt_mbr .= "v_";
        $vt_mbr .= lower_case_sym $msg;
    }

    # vt function
    $vt_func = "$rt ( CC * $vt_mbr ) ( $tformals )";

    # dispatch
    $disp = "( * $vt . f . $vt_mbr ) ( $params )";

    # function declaration
    $decl = "$rt $tname ( $formals )";

    # thunk
    $thunk = "$decl\n{\n";
    if ( $vararg )
    {
        $thunk .= "    $rt ret;\n    va_list args;\n    va_start ( args, $vararg );\n";
        $thunk .= "    assert ( $vt . f . $vt_mbr != NULL );\n";
        $thunk .= "    ret = $disp;\n    va_end ( args );\n    return ret;\n";
    }
    else
    {
        $thunk .= "    assert ( $vt . f . $vt_mbr != NULL );\n";
        $thunk .= "    return $disp;\n";
    }
    $thunk .= "}";

    # handle library constructors specially
    if ( $constructor == 0 )
    {
        $msgs [ 0 ] = $sym;
    }
    elsif ( $constructor == 1 )
    {
        my $subst_sym;
        $subst_sym = $sym .= "WithDir";
        $msgs [ 0 ] = $subst_sym;
    }
    elsif ( $constructor == 2 )
    {
        $msgs [ 1 ] = $sym;
        $cvt [ 0 ] = $vt_func;
        $decls [ 0 ] = $decl;
        $thunks [ 0 ] = $thunk;
        @pnames = split ( ',', $params );
        $mgr = $pnames [ 0 ];
    }
    elsif ( $vararg )
    {
        $decls [ $num_thunks + 1 ] = $decl;
        $thunks [ $num_thunks + 1 ] = $thunk;
        ++ $num_thunks;
    }
    elsif ( $update == 1 )
    {
        push @wmsgs, $sym;
        push @wvt, $vt_func;

        $decls [ $num_thunks + 1 ] = $decl;
        $thunks [ $num_thunks + 1 ] = $thunk;
        ++ $num_thunks;
    }
    else
    {
        $msgs [ $num_ro + 2 ] = $sym;
        $cvt [ $num_ro + 1 ] = $vt_func;
        ++ $num_ro;

        $decls [ $num_thunks + 1 ] = $decl;
        $thunks [ $num_thunks + 1 ] = $thunk;
        ++ $num_thunks;
    }
}

# create warning comment
$warn_cmt = "/* THIS IS AN AUTO-GENERATED FILE - DO NOT EDIT */\n";

# write include file
if ( $h_out )
{
    $def = "_h_$h_out";
    $def =~ s/\.h$//;
    $def .= '_';
    $def =~ s/[\/-]/_/g;
    print $H "$warn_cmt\n#ifndef $def\n#define $def\n\n";

    while ( $src_file = shift @src_files )
    {
        $src_def = "_h_$src_file";
        $src_def =~ s/\.h$//;
        $src_def .= '_';
        $src_def =~ s/[\/-]/_/g;
        print $H "#ifndef $src_def\n#include <$src_file>\n#endif\n\n";
    }

    print $H "#ifdef __cplusplus\nextern \"C\" {\n#endif\n\n";

    while ( $decl = shift @decls )
    {
        print $H "$decl;\n"
    }

    print $H "\n#ifdef __cplusplus\n}\n#endif\n\n";

    print $H "#endif /* $def */\n";

    close $H;
}

# write thunk file
if ( $h_out )
{
    print $C "$warn_cmt\n#include \"$h_out\"\n";
}
else
{
    print $C "$warn_cmt\n#include \"kqsh-$module.h\"\n";
}
print $C "#include \"kqsh-priv.h\"\n";
print $C "#include <klib/rc.h>\n";
print $C "#include <assert.h>\n\n";

# read-only messages
print $C "static const char *$module" . "_msgs [] =\n{\n";
while ( $msg = shift @msgs )
{
    print $C "    \"$msg\",\n";
}
print $C "    NULL\n};\n\n";

# read-only vtable
print $C "static union\n{\n";
print $C "    fptr_t slots [ sizeof $module" . "_msgs / sizeof $module" . "_msgs [ 0 ] - 2 ];\n\n";
print $C "    struct\n    {\n";
while ( $vt_func = shift @cvt )
{
    print $C "        $vt_func;\n"
}
print $C "    } f;\n\n} $module" . "_cvt;\n\n";

# update messages
print $C "static const char *$module" . "_wmsgs [] =\n{\n";
while ( $msg = shift @wmsgs )
{
    print $C "    \"$msg\",\n";
}
print $C "    NULL\n};\n\n";

# update vtable
print $C "static union\n{\n";
print $C "    fptr_t slots [ sizeof $module" . "_wmsgs / sizeof $module" . "_wmsgs [ 0 ] - 1 ];\n\n";
print $C "    struct\n    {\n";
while ( $vt_func = shift @wvt )
{
    print $C "        $vt_func;\n"
}
print $C "    } f;\n\n} $module" . "_wvt;\n\n";

# libdata
print $C "kqsh_libdata $module" . "_data =\n{\n";
print $C "    NULL, NULL,\n";
print $C "    $module" . "_msgs, $module" . "_cvt . slots,\n";
print $C "    $module" . "_wmsgs, $module" . "_wvt . slots\n";
print $C "};\n\n";

# special library construct thunk
$thunk = shift @thunks;
@lines = split ( '\n', $thunk );
print $C $lines [ 0 ] . "\n{\n";
print $C "    if ( sizeof $module" . "_cvt . slots != sizeof $module" . "_cvt . f ||\n";
print $C "         sizeof $module" . "_wvt . slots != sizeof $module" . "_wvt . f )\n";
print $C "    {\n";
print $C "        * $mgr = NULL;\n";
print $C "        return RC ( rcExe, rcMgr, rcConstructing, rcInterface, rcCorrupt );\n";
print $C "    }\n\n";
print $C $lines [ 2 ] . "\n";
print $C $lines [ 3 ] . "\n";
print $C "}\n\n";

# remainder of thunks
while ( $thunk = shift @thunks )
{
    print $C "$thunk\n\n";
}
