################################################################################
sub PACKAGE      { "sra-tools" }
sub VERSION      { "2.4.2" }
sub PACKAGE_TYPE { 'B' }
sub PACKAGE_NAME { "SRA-TOOLS" }
sub PACKAGE_NAMW { "SRATOOLS" }
sub DEPENDS      { qw(hdf5 xml2 magic) }
sub CONFIG_OUT   { 'build' }
sub PKG { ( LNG   => 'C',
            OUT   => 'ncbi-outdir',
            PATH  => '/usr/local/ncbi/sra-tools',
            UPATH =>      '$HOME/ncbi/sra-tools', ) }
sub REQ { (
            { name    => 'ngs-sdk',
              namew   => 'NGS',
              option  => 'with-ngs-sdk-prefix',
              type    => 'L',
              srcpath => '../ngs/ngs-sdk',
              pkgpath => '/usr/local/ngs/ngs-sdk',
              usrpath =>      '$HOME/ngs/ngs-sdk',
              bldpath => '$HOME/ncbi-outdir/ngs-sdk',
              include => 'ngs/itf/Refcount.h',
              lib     => 'libngs-sdk.$SHLX',
              ilib    => 'libngs-bind-c++.a',
            },
            { name    => 'ncbi-vdb',
              namew   => 'VDB',
              option  => 'with-ncbi-vdb-sources',
              boption => 'with-ncbi-vdb-build',
              type    => 'SB',
              srcpath => '../ncbi-vdb',
              pkgpath => '/usr/local/ncbi/ncbi-vdb',
              usrpath =>      '$HOME/ncbi/ncbi-vdb',
              bldpath => '$HOME/ncbi-outdir/ncbi-vdb',
              include => 'klib/rc.h',
              lib     => 'libncbi-vdb.a',
              ilib    => 'libkapp.a',
        } ) }
1
