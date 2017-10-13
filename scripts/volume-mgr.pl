#!/usr/bin/perl -w

################################################################################

use strict;

use Cwd "abs_path";
use File::Copy "copy";
use File::Path "make_path";
use Getopt::Long "GetOptions";
use POSIX "strftime";

my $NOT_FOUND = 0;
my $SYMLINK   = 1;
my $DIR       = 2;
my $BAD_TYPE  = 3;

my @saved;

my %OPT;
my @options
    = qw ( add dry-run help move use-new-volume-as-root volumes workspaces );
die "GetOptions: error" unless (GetOptions(\%OPT, @options));

if ($OPT{'dry-run'}) {
    $OPT{'dry-run'} = 0;
    $OPT{dry} = 1;
}
if ($OPT{'use-new-volume-as-root'}) {
    $OPT{'use-new-volume-as-root'} = 0;
    $OPT{use_new_volume_as_root} = 1;
}

print "WARNING: " .
    "THIS SCRIPT IS EXPERIMENTAL AND MIGHT NOT BE SUPPORTED IN THE FUTURE\n\n";

if ( $OPT{'help'} ) {
    help();
    exit 0;
}

unless ( $OPT{workspaces} || $OPT{volumes} || $OPT{add} || $OPT{move} ) {
    help();
    exit 1;
}

`which which`; die 'error. cannot run which' if ( $? );
`which grep` ; die 'error. cannot find grep' if ( $? );

`vdb-config -h 2>&1`;
die 'error. vdb-config is not found. Add its directory to $PATH' if ( $? );

my %workspace;
my @out = `vdb-config -on / | grep ^/repository/user`;
foreach ( @out ) {
    if    (m|/repository/user/main/public/root = "(.*)"$|) {
        $workspace{public} = $1;
    }
    elsif (m|/repository/user/protected/(.*)/root = "(.*)"$|) {
        $workspace{$1} = $2;
    }
}

################################################################################
if ( $OPT{workspaces} ) {
    workspaces();
}

if ( $OPT{volumes} ) {
    if ( $#ARGV < 0 ) {
        help();
        exit 1;
    }

    my ( $wrksp_name ) = @ARGV;
    unless ( $workspace{$wrksp_name} ) {
        print "Error. workspace '$wrksp_name' does not exist.\n\n";
        workspaces();
        exit 1;
    }

    volumes ( $wrksp_name );
}

my $t = `date +%s.%N`;
my $TIMESTAMP = strftime ( "%Y%m%d-%H%M%S", localtime $t );
$TIMESTAMP   .= sprintf ( ".%01d", ( $t - int ( $t ) ) * 10 );
my $NCBI_SETTINGS = ncbi_settings();
my $saved;

my $added;

if ( $OPT{add} ) {
    if ( $#ARGV != 1 ) {
        help();
        exit 1;
    }

    my ( $wrksp_name, $new_root_path ) = @ARGV;

    $added = add_volume ( $wrksp_name, $new_root_path );
}

if ( $OPT{move} ) {
    if ( $#ARGV < 0 ) {
        help();
        exit 1;
    }

    my ( $wrksp_name ) = @ARGV;
    unless ( $workspace{$wrksp_name} ) {
        print "Error. workspace '$wrksp_name' does not exist.\n\n";
        workspaces();
        exit 1;
    }

    if ( $OPT{add} && $OPT{dry} && $added ) {
        print "Info: -add and --dry-run are specified: -move is ignored\n";
    } else {
        move ( $wrksp_name ) ;
    }
}

if ( $saved ) {
    print "Old configuration was saved to '$NCBI_SETTINGS.$TIMESTAMP'\n";
    print "Update log was saved to '$NCBI_SETTINGS.log.$TIMESTAMP'\n";
}

################################################################################


sub add_volume {
    my ( $wrksp_name, $new_root_path ) = @_;

    if ( $OPT{use_new_volume_as_root} && $wrksp_name eq 'public' ) {
        print "ERROR: CHANGE OF ROOT OF PUBLIC WORKSPACE IS NOT ALLOWED\n";
        help();
        exit 1;
    }

    unless ( $workspace{$wrksp_name} ) {
        print "Error: workspace '$wrksp_name' does not exist.\n\n";
        workspaces();
        exit 1;
    }

    unless ( -e $new_root_path ) {
        print "Error: <path-to-new-volume> should exist: '$new_root_path'\n";
        exit 1;
    }

    unless ( -d $new_root_path ) {
        print "Error: "
            . "<path-to-new-volume> should be a directory: '$new_root_path'\n";
        exit 1;
    }

    print "Info: dry-run: no changes will be made\n" if ($OPT{dry});

    $new_root_path = abs_path $new_root_path;
    print "New volume: '$new_root_path'\n";

    workspaces ( $wrksp_name, $new_root_path );

    my @volumes = volumes ( $wrksp_name );

    my %volumes = get_volumes ( $wrksp_name );

    my $need_update = 1;
    my $updated = 1;
    my $new_vol;
# do we know new volume ?
    foreach ( keys %volumes ) {     # iterate all volumes: sra, sra2 etc
        my %vol = %{$volumes{$_}};
        if ( $vol{root} eq $new_root_path ) {
            if ( $vol{active} ) {
                unless ( $OPT{use_new_volume_as_root}
                    && $new_root_path ne $workspace{$wrksp_name} )
                {
                    print "Info: '$new_root_path' is already the active "
                        . "root of workspace '$wrksp_name': "
                        . "no changes needed\n";
                    $updated = $need_update = 0;
                }
                $new_vol = $_;
            } else {
# making $new_root_path active
                print "Info: '$new_root_path' is known already. "
                    . "Making the volume active...\n";
                set_volumes ( $wrksp_name, $_, @volumes );
                volumes ( $wrksp_name, 0, 1 ) unless ( $OPT{dry} );
                $need_update = 0;
            }
        }
    }

# make symlinks from new-root to all volumes
    my $p = $OPT{use_new_volume_as_root}
            ? $new_root_path : $workspace{$wrksp_name};
    if ( $need_update ) {
        mk_dir ( $p ) unless ( $OPT{use_new_volume_as_root} || -e $p );
        chdir $p || die "FATAL: cannot cd $p";
#   print "dbg: making symlinks from current root to new volume...\n";
        print "dbg: cd $p: ok\n";
# select name for new volume
        unless ( $new_vol ) {
            $new_vol = next_new_volume_name ( $workspace{$wrksp_name},
                $new_root_path, %volumes );
        }
        print "dbg: try '$new_vol' as new volume...\n";
        unless ( -e "$new_root_path/$new_vol" ) {
            unless ( -e $new_root_path ) {
                die "Fatal: '$new_root_path should exist'";
            }
            mk_dir ( "$new_root_path/$new_vol" );
        }

        if ( $new_root_path ne $workspace{$wrksp_name} ) {
            if ( ! $OPT{use_new_volume_as_root} && ! -e $new_vol ) {
                sym_link ( "$new_root_path/$new_vol", $new_vol );
            }

            foreach ( keys %volumes ) {
                if ( -l $_ ) {
                    if ( $volumes{$_}{type} != $NOT_FOUND ) {
                        my $v = readlink $_;
                        unless ( $v eq $volumes{$_}{resolved} ) {
                            die "'$_' points to '$v'";
                        }
                        print "dbg: '$_' points to '$v': ok\n";
                    }
                } elsif ( -e $_ ) {
                    if ( -d $_ ) {
                        print "dbg: "
                            . "'$_' exists and is a directory: ignored\n";
                    } else {
                        die "'$_' exists and is not a directory";
                    }
                } else {
                    if ( -e $volumes{$_}{resolved} ) {
                        sym_link ( $volumes{$_}{resolved}, $_);
                    } else {
                        print "dbg: '$volumes{$_}{resolved}' "
                            . "does not exist: ignored\n";
                    }
                }
            }
        }
    }

    if ( $need_update ) {
# update volumes in configuration
        set_volumes ( $wrksp_name, $new_vol, @volumes );

        save_new_root_in_configuration ( $wrksp_name, $new_root_path );

        print "Info: added '$new_root_path' to workspace '$wrksp_name'\n";

        volumes ( $wrksp_name, 0, 1 ) unless ($OPT{dry});
    }

    $updated;
}


sub workspaces {
    my ( $wrksp_name, $new_root ) = @_;

    return if ( $OPT{WORKSPACES_PRINTED} && ! $new_root );

    unless ( $OPT{WORKSPACES_PRINTED} ) {
        print <<EndText;
=============================================================
| WORKSPACES
-------------------------------------------------------------
| Name\tRoot path
-------------------------------------------------------------
EndText
        print "$_\t$workspace{$_}\n" foreach ( keys %workspace );
        print "=============================================================\n";
    }

    my @names = keys %workspace;

    while ( my ( $name, $root ) = each %workspace ) {
        foreach my $name2 ( @names ) {
            next if ( $name eq $name2 );
            my $root2 = $workspace{$name2};
            if ( $root eq $root2 ) {
                die "FATAL: YOUR CONFIGURATION IS INVALID:\nWORKSPACES "
                    . "'$name' and 'name2' HAVE THE SAME ROOT '$root'";
            }
            if ( is_in ( $root, $root2 ) ) {
                die "FATAL: YOUR CONFIGURATION IS INVALID:\n"
                    . "WORKSPACE '$name2' ($root2) IS INSIDE Of\n"
                    . "WORKSPACE '$name' ($root)";
            }
            if ( is_in ( $root2, $root ) ) {
                die "FATAL: your configuration is invalid:\n"
                    . "WORKSPACE '$name' ($root) is inside of\n"
                    . "WORKSPACE '$name2' ($root2)";
            }
        }

        if ( $new_root && $name ne $wrksp_name ) {
            if ( $root eq $new_root ) {
                die "FATAL: <path-to-new-volume> IS THE SAME AS "
                  . "ROOT OF WORKSPACE '$name' ($root)";
            }
            if ( is_in ( $root, $new_root ) ) {
                die "FATAL: <path-to-new-volume> ($new_root) IS INSIDE OF\n"
                  . "WORKSPACE '$name' ($root)";
            }
            if ( is_in ( $new_root, $root ) ) {
                die "FATAL: WORKSPACE '$name' ($root) IS INSIDE OF\n"
                  . "<path-to-new-volume> ($new_root)";
            }
        }
    }

    ++ $OPT{WORKSPACES_PRINTED};
}


sub move {
    my ( $wrksp_name ) = @_;
    my @volumes = volumes ( $wrksp_name );
    if ( $#volumes < 0 ) {
        print "Info: Workspace '$wrksp_name' does not have any volume\n";
        return 0;
    }
    if ( $#volumes == 0 ) {
        print "Info: Workspace '$wrksp_name' have a single volume\n";
        return 0;
    }

    `which cp`; die 'error. cannot find cp' if ( $? );

    my $cp = 'cp --sparse=always';
    my $cmd = "$cp " . abs_path ( $0 ) . ' /dev/null';
    my $out = `$cmd 2>&1`;
    if ( $? ) {
        if ( $out =~ /illegal option/ ) {
            $cp = '';
            `which mv`; die 'error. cannot find mv' if ( $? );
        } else {
            die 'cannot run cp';
        }
    } else {
        `which rm`; die 'error. cannot find rm' if ( $? );
    }

    my %volumes = get_volumes ( $wrksp_name );
    my $active = $volumes [ 0 ];
    my $dst = $volumes{$active}{path};
    my $found;
    foreach ( keys %volumes ) {     # iterate all volumes: sra, sra2 etc
        if ( $_ ne $active ) {
            my %vol = %{$volumes{$_}};
            my @files = glob ( "$vol{path}/*.cache" );
            foreach ( @files ) {
                unless ( $found ) {
                    $found = 1;
                    print "Info: Moving cache files of workspace '$wrksp_name' "
                        . "to active volume...\n";
                }
                if ( $cp ) {
                    run ( "$cp $_ $dst" );
                    run ( "rm $_" );
                } else {
                    run ( "mv $_ $dst" );
                }
            }
        }
    }

    if ( $found ) {
        print "Info: cache files of workspace '$wrksp_name' "
            . "were moved to the active volume\n";
    } else {
        print "Info: No cache files were found "
            . "in non-active volumes of workspace '$wrksp_name'\n";
    }
}


sub get_volumes {
    my ( $wrksp_name ) = @_;
    my $root = root ( $wrksp_name );
    my %volumes;
    my $first = 1;
    foreach ( volumes ( $wrksp_name, 1 ) ) {
        my $path = "$root/$_";
        my $resolved = $path;
        my $type = $NOT_FOUND;
        my $real_root;
        if ( -l $path ) {
            $type = $SYMLINK;
            $resolved = readlink ( $path ) or die;
            $real_root = abs_path "$resolved/..";
        }
        if ( -e $path ) {
            if ( ! -l $path && -d $path ) {
                $type = $DIR;
                $real_root = $root;
            } else {
                $type = $BAD_TYPE;
            }
        } else {
            $real_root = $root;
        }
        $volumes{ $_ } = {
            name     => $_,
            path     => $path,     # path to the volume: /.../path/sraX
            type     => $type,
            resolved => $resolved, # path to the volume with symlinnks resolved
            root     => $real_root,# root directory of the volume: /.../path
            active   => $first,
        };
        $first = 0;
    }
    %volumes;
}


sub volumes {
    my ( $wrksp_name, $silent, $updated ) = @_;

    $OPT{VOLUMES_PRINTED} = 0 if ( $updated );
    $silent = 1 if ( $OPT{VOLUMES_PRINTED} );

    my $cmd = 'vdb-config -on | grep ' . volumes_node ( $wrksp_name );
    my $volumes = `$cmd`;
    die if ( $? );
    my @volumes;
    $volumes =~ /^.* = "(.*)"$/;
    @volumes = split ':', $1 if ( $1 );
    unless ( $silent ) {
        print <<EndText;
=============================================================
Workspace name: '$wrksp_name'
Workspace root: '$workspace{$wrksp_name}'
-------------------------------------------------------------
| VOLUMES
-------------------------------------------------------------
| Name\tPath\tActive
=============================================================
EndText
        my $first = 1;
        foreach ( @volumes ) {
            print "$_\t";
            my $path = "$workspace{$wrksp_name}/$_";
            if ( -l $path ) {
                print readlink $path;
            } else {
                print $path;
            }
            if ( $first ) {
                $first = 0;
                print "\tACTIVE"
            }
            print "\n";
        }
        print "=============================================================\n"
    }

    ++ $OPT{VOLUMES_PRINTED} unless ( $silent );

    @volumes;
}


sub next_new_volume_name {
    my ( $old_root_path, $new_root_path, %volumes ) = @_;

    foreach ( keys %volumes ) {
        my %vol = %{$volumes{$_}};
        if ( $vol{type} == $NOT_FOUND ) {
            return $vol{name} if ( ! -e $vol{name} || -d $vol{name} );
        } elsif ( $vol{root} eq $new_root_path ) {
            return $vol{name};
        }
    }

    for ( my $i = 0; ; ++ $i ) {
        my $new_vol = "sra$i";
        unless ( $volumes{$new_vol} ) {
            if ( -e "$new_root_path/$new_vol" ) {
                next unless ( -d "$new_root_path/$new_vol" );
                next if ( -l "$new_root_path/$new_vol" );
            }
            if ( ! $OPT{use_new_volume_as_root}
                && $new_root_path ne $old_root_path)
            {
                if ( -e "$old_root_path/$new_vol" ) {
                    if ( -l "$old_root_path/$new_vol" ) {
                        my $v = readlink "$old_root_path/$new_vol";
                        next unless ( $v eq "$new_root_path/$new_vol" );
                    }
                } else {
                    return $new_vol;
                }
            } else {
                return $new_vol;
            }
        }
    }
}


sub help {
    print <<EndText;
Usage: $0 [options]

Options:
 -help                             - display this help and exit

 -workspaces                       - list user workspaces
 -volumes <workspace-name>         - list volumes of workspace

 -add <workspace-name> <path-to-new-volume> [ -dry-run ]
                                   - add a new volume to workspace,
                                     keep old workspace root 

 -add -use-new-volume-as-root <workspace-name> <path-to-new-volume> [ -dry-run ]
                                   - add a new volume to workspace,
                                     use <path-to-new-volume> as workspace root
                                     JUST FOR PROTECTED (DBGaP) WORKSPACES

 -move <workspace-name> [ -dry-run ]
                                   - move partally cached files in workspace
                                     to the active volume

 -dry-run                          - perform a trial run with no changes made
EndText
}


sub root {
    my ( $wrksp_name ) = @_;

    my $node = root_node ( $wrksp_name );

    my %roots;
    my @roots = `vdb-config -on | grep '$node '`;
    die if ( $? );
    die 'inclompete configuration' if ( $#roots == -1 );
    die if ( $#roots != 0 );
    die unless ( $roots[0] =~/^$node = "(.*)"$/ );
    $1;
}


sub ncbi_settings {
    $_ = `vdb-config -on NCBI_SETTINGS`;
    die if ( $? );
    die unless ( /^NCBI_SETTINGS = "(.*)"/ );
    $1;
};


sub save_new_root_in_configuration {
    my ( $wrksp_name, $new_root_path ) = @_;

    if ( $OPT{use_new_volume_as_root} ) {
        unless ( $workspace{$wrksp_name} eq $new_root_path ) {
            set ( root_node ( $wrksp_name ), $new_root_path );
        }
    }
}


sub run {
    my ( $cmd, $msg ) = @_;
    if ( $msg ) {
        print "dbg: $msg... ";
    } else {
        print "dbg: $cmd... ";
    }
    unless ($OPT{dry}) {
        system ($cmd) && die;
        print "ok";
    }
    print "\n";
}


sub log_ {
    my ( $msg ) = @_;
    my $filename = "$NCBI_SETTINGS.log.$TIMESTAMP";
    open ( my $fh, ">>$filename" ) or die "Cannot open file '$filename' $!";
    print $fh "cd " . `pwd`;
    print $fh "$msg\n\n";
    close $fh;
}


sub sym_link {
    my ( $old, $new ) = @_;
    print "dbg: symlink '$old' '$new'... ";
    unless ( $OPT{dry} ) {
        symlink ( $old, $new ) || die;
        print "ok";
    }
    print "\n";

    log_ ( "ln -s $old $new" );
}


sub mk_dir {
    my ( $path ) = @_;
    print "dbg: mkdir '$path'... ";
    unless ($OPT{dry}) {
        make_path ( $path ) or die;
        print "ok";
    }
    print "\n";

    log_ ( "mkdir -p $path" );
}


sub volumes_node {
    if ( $_[0] eq 'public' ) {
        return "/repository/user/main/public/apps/sra/volumes/sraFlat";
    } else {
        return "/repository/user/protected/$_[0]/apps/sra/volumes/sraFlat";
    }
}


sub root_node {
    if ( $_[0] eq 'public' ) {
        return "/repository/user/main/public/root";
    } else {
        return "/repository/user/protected/$_[0]/root";
    }
}


sub set_volumes {
    my ( $wrksp_name, $new_vol, @volumes ) = @_;

    my $vols = $new_vol;
    foreach ( @volumes ) {
        $vols .= ":$_" unless (/^$new_vol$/);
    }

    set ( volumes_node ( $wrksp_name ), $vols );
}


sub set {
    my ( $n, $v ) = @_;

    my $old = `vdb-config -on $n`;

    unless ( $saved ) {
        if ( -e $NCBI_SETTINGS ) {
            copy ( $NCBI_SETTINGS, "$NCBI_SETTINGS.$TIMESTAMP" ) or die;
        }
        ++ $saved;
    }

    run ( "vdb-config -s $n=$v", "Setting $n=$v" );
}


sub is_in {
    return substr ( $_[1], 0, length( $_[0] ) ) eq $_[0];
}
