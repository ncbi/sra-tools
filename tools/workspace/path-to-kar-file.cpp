#include <kapp/main.h>
#include <klib/rc.h>
#include <vfs/manager.h>
#include <vfs/manager-priv.h>
#include <vfs/path.h>
#include <kfs/directory.h>

#include "sratoolkit-exception.hpp"

#include <iostream>

namespace sra
{

    static VFSManager *vfs;
    static KDirectory *wd;


    static
    rc_t CC HandleSourceFiles ( const KDirectory *dir, uint32_t type, const char *name, void *data )
    {
        rc_t rc = 0;

        unsigned int *indent = ( unsigned int * ) data;
        for ( unsigned int i = 0; i < * indent; ++ i )
            std :: cout << "  ";

        switch ( type )
        {
        case kptFile:
            std :: cout << "File: " << name << std :: endl;
            break;
        case kptDir:
        {
            std :: cout << "Dir: " << name << std :: endl;

            unsigned int sub = * indent + 1;
         
            rc = KDirectoryVisit ( dir, false, HandleSourceFiles, & sub, "%s", name );
            break;
        }
        case kptFile | kptAlias:
        case kptDir | kptAlias:
            std :: cout << "Alias: " << name << std :: endl;
            break;
        default:
            return RC ( rcExe, rcNoTarg, rcValidating, rcNoObj, rcUnknown );
        }
        return rc;
    }

    static 
    void run ( int argc, char *argv [] )
    {
        assert ( argc > 1 );

        rc_t rc = VFSManagerMake ( &vfs );
        if ( rc != 0 )
            throw Exception ( rc, __FILE__, __LINE__, "aaah" );

        rc = KDirectoryNativeDir ( &wd );
        if ( rc != 0 )
            throw Exception ( rc, __FILE__, __LINE__, "aaah" );

        VPath *url;
        rc = VFSManagerMakePath ( vfs, & url, "%s", argv [ 1 ] );
        if ( rc == 0 )
        {
            const KDirectory *src;

            rc = VFSManagerOpenDirectoryReadDirectoryRelative ( vfs, wd, &src, url );
            if ( rc == 0 )
            {
                unsigned int indent = 0;
                rc = KDirectoryVisit ( src, false, HandleSourceFiles, & indent, "." );

                KDirectoryRelease ( src );
            }

            VPathRelease ( url );
        }
    }
}

ver_t CC KAppVersion ( void )
{
    return 0;
}

const char UsageDefaultName[] = "aaaah";

rc_t CC UsageSummary ( const char *progname )
{
    return 0;
}

rc_t CC Usage ( const Args *args )
{
    return 0;
}


rc_t CC KMain ( int argc, char *argv [] )
{
    rc_t rc = 0;

    try
    {
        sra :: run ( argc, argv );
    }
    catch ( ... )
    {
        rc = RC ( rcExe, rcNoTarg, rcExecuting, rcNoObj, rcUnknown );
    }

    return rc;
}
