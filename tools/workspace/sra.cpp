/*===========================================================================
*
*                            PUBLIC DOMAIN NOTICE
*               National Center for Biotechnology Information
*
*  This software/database is a "United States Government Work" under the
*  terms of the United States Copyright Act.  It was written as part of
*  the author's official duties as a United States Government employee and
*  thus cannot be copyrighted.  This software/database is freely available
*  to the public for use. The National Library of Medicine and the U.S.
*  Government have not placed any restriction on its use or reproduction.
*
*  Although all reasonable efforts have been taken to ensure the accuracy
*  and reliability of the software and data, the NLM and the U.S.
*  Government do not and cannot warrant the performance or results that
*  may be obtained by using this software or data. The NLM and the U.S.
*  Government disclaim all warranties, express or implied, including
*  warranties of performance, merchantability or fitness for any particular
*  purpose.
*
*  Please cite the author in any work or product based on this material.
*
* ===========================================================================
*
*/

#include <kapp/main.h>
#include <kapp/args.h>
#include <klib/text.h>
#include <klib/out.h>
#include <klib/log.h>
#include <klib/rc.h>
#include <klib/printf.h>
#include <klib/debug.h>
#include <klib/time.h>
#include <klib/text.h>
#include <klib/container.h>
#include <klib/data-buffer.h>
#include <kfg/config.h>
#include <kns/manager.h>
#include <kns/http.h>
#include <kns/stream.h>
#include <kfs/file.h>
#include <kfs/directory.h>
#include <kfs/gzip.h>
#include <kfs/bzip.h>
#include <kfs/defs.h>
#include <vfs/manager.h>
#include <vfs/manager-priv.h>
#include <vfs/path.h>

#include "prefs-priv.h"
#include "token.h"
#include "sra.vers.h"
#include "sratoolkit-exception.hpp"

#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <ctime>
#include <string>
#include <iostream>
#include <sys/wait.h>
#include <unistd.h>
#include <time.h>


namespace sra
{
    struct PrefsData
    {
        PrefsData ();
        ~ PrefsData ();

        PrefsData & operator = ( const PrefsData & pd );
        PrefsData ( const PrefsData & pd );

        KTime_t dl_date;     // download date
        KTime_t last_check_date; 
        KTime_t last_check_date_interval_limit; // amount of time allowed between checks
        ver_t l_vers;              // latest version available from NCBI
        ver_t c_vers;              // current version on user's system
        std :: string download_url;
        std :: string latest_vers_url;
        std :: string path_to_installation;
        bool auto_download_enabled;
    };

    PrefsData :: PrefsData ()
        : dl_date ( 0 )
        , last_check_date ( 0 )
        , last_check_date_interval_limit ( 7 * 24 * 60 * 60 ) // 1 week
        , l_vers ( 0 )
        , c_vers ( 0 )
        , download_url ( "http://ftp-trace.ncbi.nlm.nih.gov/sra/sdk" )
          // NB - this is currently a static file
          // needs to be replaced with CGI
        , latest_vers_url ( "http://ftp-trace.ncbi.nlm.nih.gov/sra/sdk/current/sratoolkit.current.version" )
        , auto_download_enabled ( true )
    {
    }

    PrefsData :: ~ PrefsData ()
    {
    }

    PrefsData & PrefsData :: operator = ( const PrefsData & pd )
    {
        dl_date = pd . dl_date;
        last_check_date = pd . last_check_date;
        last_check_date_interval_limit = pd . last_check_date_interval_limit;
        l_vers = pd . l_vers;
        c_vers = pd . c_vers;
        download_url = pd . download_url;
        latest_vers_url = pd . latest_vers_url;
        path_to_installation = pd . path_to_installation;
        auto_download_enabled = pd . auto_download_enabled;

        return * this;
    }

    PrefsData :: PrefsData ( const PrefsData & pd )
        : dl_date ( pd . dl_date )
        , last_check_date ( pd . last_check_date )
        , last_check_date_interval_limit ( pd . last_check_date_interval_limit )
        , l_vers ( pd . l_vers )
        , c_vers ( pd . c_vers )
        , download_url ( pd . download_url )
        , latest_vers_url ( pd . latest_vers_url )
        , path_to_installation ( pd . path_to_installation )
        , auto_download_enabled ( pd . auto_download_enabled )
    {
    }


    enum WhereFound { found_by_prefs, found_by_PATH };

    static PrefsData prefs_data;
    static bool dirty;

    static KTime_t current_time; // current time of running process
    static KTime_t lower_date_limit; //dawn of time for this program
    static std :: string prgm_name;
    static std :: string PATH;
    static std :: string path_to_binaries;
    static std :: string latest_vers_string; // TBD - record this from NCBI
    static std :: string path_to_prefs;
    static std :: string path_to_url;
    static KDirectory * wd;
    static VFSManager * vfs;
    static KNSManager * kns;
    static KConfig * kfg;
    static WhereFound where_found;
    

    static int g_argc;
    static char ** g_argv;

    enum SRAOption
    {
        opt_path_to_prefs,
        opt_offline_mode,
        opt_verbose,
        opt_auto_update,

        opt_unrecognized,
        opt_no_more_args
    }; 

    struct SRAOption_Iterator
    {
        SRAOption_Iterator ();

        int idx, num_params;
        const char *param;

        enum SRAOption next ();
        void remove ();
    };

    SRAOption_Iterator :: SRAOption_Iterator ()
        : idx ( 0 )
        , num_params ( 0 )
        , param ( "" )
    {
    }

    enum SRAOption SRAOption_Iterator :: next ()
    {
        idx += num_params + 1;
        num_params = 0;

        if ( idx >= g_argc )
            return opt_no_more_args;

        static struct
        {
            const char *opt_name;
            const char *alias;
            size_t num_params;
            const char *xtext;
            SRAOption opt_enum;
        }
        formal_opts [] =
        {
            { "--sra-prefs", NULL, 1, "expected path to prefs", opt_path_to_prefs },
            { "--offline-mode", NULL, 0, NULL, opt_offline_mode },
            { "--verbose", NULL, 0, NULL, opt_verbose },
            { "--auto-update", NULL, 1, "expected 'yes' or 'no'", opt_auto_update }
        };

        for ( uint32_t i = 0; i < sizeof formal_opts / sizeof formal_opts [ 0 ]; ++ i )
        {
            // "strcmp" is safe here because we know the formal_opt name
            // is properly NUL terminated and has a known, controlled length
            if ( strcmp ( g_argv [ idx ], formal_opts [ i ] . opt_name ) == 0 )
            {
                // right now we only support 0 or 1 option parameters
                // if we ever allow > 1, the class structure needs to change
                assert ( formal_opts [ i ] . num_params <= 1 );

                // look for option parameters
                for ( uint32_t p = 0; p < formal_opts [ i ] . num_params; ++ p )
                {
                    // make sure the command line has supplied the parameter
                    if ( ++ num_params + idx == g_argc )
                    {
                        rc_t rc = RC ( rcExe, rcNoTarg, rcNotFound, rcPath, rcEmpty );
                        throw Exception ( rc, __FILE__, __LINE__, formal_opts [ i ] . xtext );
                    }

                    // record the option parameter
                    param = g_argv [ idx + num_params ];
                }

                // return the option enum
                return formal_opts [ i ] . opt_enum;
            }
        }

        // unrecognized option - start of sra-shell parameters
        return opt_unrecognized;
    }

    void SRAOption_Iterator :: remove ()
    {
        int next_opt = idx + num_params + 1;
        int remaining = g_argc - next_opt;
        if ( remaining <= 0 )
            g_argc = idx;
        else
        {
            memmove ( & g_argv [ idx ], & g_argv [ next_opt ], remaining * sizeof g_argv [ 0 ] );
            g_argc -= num_params + 1;
        }

        g_argv [ g_argc ] = NULL;
    }
    

    static
    void InitFactoryDefaults ( void )
    {
        // create managers;
        
        rc_t rc = VFSManagerMake ( & vfs );
        if ( rc != 0 )
            throw Exception ( rc, __FILE__, __LINE__, "failed to create VFS manager" );
        
        rc = VFSManagerGetKNSMgr ( vfs, & kns );
        if ( rc != 0 )
            throw Exception ( rc, __FILE__, __LINE__, "failed to retrieve KNS manager" );

        // create Native Dir
        rc = KDirectoryNativeDir ( & wd );
        if ( rc != 0 )
            throw Exception ( rc, __FILE__, __LINE__, "failed to create native KDirectory" );

        // create KConfig
        rc = KConfigMake ( &kfg, NULL );
        if ( rc != 0 )
            throw Exception ( rc, __FILE__, __LINE__, "failed to create KConfig" ); 

        // locate NCBI_HOME directory from KConfig
        String *ncbi_home;
        rc = KConfigReadString ( kfg, "NCBI_HOME", &ncbi_home );
        if ( rc != 0 )
            throw Exception ( rc, __FILE__, __LINE__, "failed to determine NCBI_HOME" );

        // create path to preferences file
        path_to_prefs = std :: string ( ncbi_home -> addr, ncbi_home -> size );
        StringWhack ( ncbi_home );
        path_to_prefs += "/sra.prefs";

        // capture $PATH
        const char *p = getenv ( "PATH" );
        if ( p == NULL )
            throw Exception ( __FILE__, __LINE__, "failed to determine PATH" );

        PATH = std :: string ( p );
        
        // capture current timestamp
        current_time = KTimeStamp ();

        //cutoff time
        KTime cutoff = { 2014 };
        lower_date_limit = KTimeMakeTime ( &cutoff );

        
    }

    // user may want to indicate a different location from which
    // to load user preferences
    static
    void OverridePathToPrefs ( void )
    {
        SRAOption_Iterator it;

        while ( 1 )
        {
            switch ( it . next () )
            {
            case opt_path_to_prefs:
                path_to_prefs = std :: string ( it . param );
                it . remove ();
                break;

            case opt_unrecognized:
            case opt_no_more_args:
                return;

            default:
                break;
            }
        }
    }

    static 
    void LoadPrefsFromFile ()
    {
        // open prefs file for read
        FILE *prefs = fopen ( path_to_prefs . c_str (), "r" );
        if ( prefs != NULL )
        {
            // init flex with file
            rc_t rc = PrefsInitFlex ( prefs );
            if ( rc != 0 )
            {
                fclose ( prefs );
                throw Exception ( rc, __FILE__, __LINE__, "failed to initialize prefs parser" );
            }

            // parse contents with prefs_parse
            prefs_parse (); 

            // destroy flex
            PrefsDestroyFlex ();

            // close prefs
            fclose ( prefs );
        }
    }

    static 
    void Init ( int argc, char *argv [] )
    {
        // capture in global variables
        g_argc = argc;
        g_argv = argv;

        prgm_name = argv [ 0 ];

        InitFactoryDefaults ();
        OverridePathToPrefs ();
        LoadPrefsFromFile ();
    }

    static 
    void OverrideParamsFromCmdLine ( PrefsData & params )
    {
        SRAOption_Iterator it;
        while ( 1 )
        {
        // scan argv for options
            switch ( it . next () )
            {
                // path to installation
            case opt_path_to_prefs:
                // impossible - we already removed them all above.
                // good case for a debug msg... but doesn't hurt anything
                it . remove ();
                break;

                // offline non interactive mode
            case opt_offline_mode:
                // TBD - set offline mode, either to true
                // or if this option is made to take parameters,
                // interpret the parameter
                it . remove ();
                break;

                // verbocity
            case opt_verbose:
                // TBD - set verbosity level
                // this is either {none, some} or takes a parameter
                it . remove ();
                break;

                // no auto update
            case opt_auto_update:
            {
                std :: string up = std :: string ( it . param ); 
                if ( up . compare ( "yes" ) )
                    params . auto_download_enabled = true;
                else if ( up . compare ( "no" ) )
                    params . auto_download_enabled = false;
                else
                    throw Exception ( __FILE__, __LINE__, "invalid option for '--auto-update'\n" );
                it . remove ();
                break;
            }

            case opt_unrecognized:
            case opt_no_more_args:
                return;
            }
        }
    }

    static
    void SetPrefsCurrentVersion () // !!!
    {
        // set version in prefs and working params
        

        // mark dirty
        dirty = true;
    }

    static
    bool IsPathToInstallation ( const char * path, const char *ext )
    {
        uint32_t path_type = KDirectoryPathType ( wd,
                                                  "%s/%s"
#if WINDOWS
                                                  ".exe"
#endif
                                                  , path
                                                  , ext );
        switch ( path_type )
        {
        case kptFile:
        case kptFile | kptAlias:
            // TBD - test for executable mode ??
            return true;
        }

        return false;
    }

    static 
    bool IsPathToInstallation ( const char * path, unsigned int len, const char *ext )
    {
        uint32_t path_type = KDirectoryPathType ( wd,
                                                  "%.*s/%s"
#if WINDOWS
                                                  ".exe"
#endif
                                                  , len
                                                  , path
                                                  , ext );
        switch ( path_type )
        {
        case kptFile:
        case kptFile | kptAlias:
            // TBD - test for executable mode ??
            return true;
        }

        return false;
    }

    static 
    bool LocateUsingPrefsPath ( const PrefsData & params )
    {
        //# path names a directory, e.g. $HOME/sratoolkit-2.3.4
        //# on Windows, add ".exe" to end of file names
        //# implication of using existing prefs path is that we know the version
        
        if ( IsPathToInstallation ( params . path_to_installation . c_str (), "bin/sra-version" ) )
            path_to_binaries = params . path_to_installation + "/bin";
        else
        {
            if ( IsPathToInstallation ( params . path_to_installation . c_str (), "bin/fastq-dump" ) )
                path_to_binaries = params . path_to_installation + "/bin";
            else
            {
                if ( ! IsPathToInstallation ( params . path_to_installation . c_str (), "fastq-dump" ) )
                    return false;

                path_to_binaries = params . path_to_installation;
            }
        }
        

        // record found in user-installation
        where_found = found_by_prefs;

        // !!! if ! version, obtain version by running tool
        // run sra-version to capture simple version
        // or run fastq-dump -V to capture and parse version
        SetPrefsCurrentVersion ();

        return true;
    }

    static 
    bool LocateUsingPATH ( const PrefsData &params  )
    {
        //loop over $PATH using string_chr
        const char *sep, *dir = PATH . c_str ();
        while ( 1 )
        {
            // find next separator
            sep = strchr ( dir, ':' );

            // get length of this directory
            // if separator was found, distance from "dir" to ':'
            // otherwise, it's the remaining length in "dir"
            unsigned int len = ( sep != NULL ) ? sep - dir : strlen ( dir );
            if ( len != 0 )
            {
                if ( IsPathToInstallation ( dir, len,  "sra-version" ) ||
                     IsPathToInstallation ( dir, len, "fastq-dump" ) )
                {
                    path_to_binaries = std :: string ( dir, len );
                    break;
                }
            }

            // detect no more directories
            if ( sep == NULL )
                return false;

            // advance to next directory
            dir = sep + 1;
        }

        //record found in $PATH
        where_found = found_by_PATH;

        // !!! run sra-version to capture simple version
        //or run fastq-dump -V to capture and parse version
        SetPrefsCurrentVersion ();

        return true;
    }

    static
    bool LocateWithUserAssistance ( const PrefsData &params )
    {
        //save prefs.path-to-installation
        std :: string prev_path = prefs_data . path_to_installation;
        //tell user that we cannot locate the installation

        std :: cout << "Unable to find a previous installation of SRAToolkit." << std :: endl;

        //loop
        while ( 1 )
        {
            std :: cout << "Would you like to provide a path to an installation of SRAToolkit? [ y / N ] ";
            std :: cout . flush ();

            std :: string ch;
            std :: getline ( std :: cin, ch );

            // anything other than y or Y exits loop/function
            if ( ch . empty () )
                break;
            if ( ch . compare ( "y" ) != 0 && ch . compare ( "Y" ) != 0 )
                break;

            std :: string path;
            std :: cout << "Please provide the path to the installation: "; // !!! should we give an example?
            std :: cout . flush ();

            std :: getline ( std :: cin, path );
                
            // empty == retry.
            if ( path . empty () )
            {
                std :: cout << "Empty path." << std :: endl;
                continue;
            }

            // slam in new potential value
            prefs_data . path_to_installation = path;
            prefs_data . c_vers = 0;

            // test user's input
            if ( LocateUsingPrefsPath ( prefs_data ) )
            {
                // found it - need to update the prefs
                dirty = true;
                return true;
            }

            // restore previous information
            prefs_data . path_to_installation = prev_path;
            std :: cout
                << " Unable to find SRAToolkit at: "
                << path
                << std :: endl;
        }

        return false;
    }

    static
    bool LocateSRAToolkit ( const PrefsData & params )
    {
        // first, try from previously written prefs
        // this cannot succeed the first time through,
        // because we have not created prefs yet.
        if ( LocateUsingPrefsPath ( params ) )
            return true;

        // MOST systems will not have an admin installation
        // but for those systems that do, check to see what's there
        if ( LocateUsingPATH ( params ) )
            return true;

        // the first time through, we will ask the user
        // to locate a previous installation, but the most
        // likely scenario is that there is none to be found
        if ( LocateWithUserAssistance ( params ) )
            return true;

        // probably a first time user
        return false;
    }

    static
    void UpdatePrefsFromCGI ( const char *data ) // !!!
    {
        // parse data buffer to get data
        // assign into prefs_data
    }

    static
    bool ContactNCBIForVersion ( const PrefsData & params )
    {
        bool success = false;
        
        assert ( prefs_data . latest_vers_url . c_str () != 0 ); // assume the url has been properly set

        //use HTTP to contact CGI
        KHttpRequest *req;
        rc_t rc = KNSManagerMakeRequest ( kns, &req, 0x01000000, NULL, 
            "%s", prefs_data . latest_vers_url . c_str () );


        if ( rc == 0 )
        {
            try
            {
                // pass OS & version, architecture, current "sra" tool version
                // OS by KConfig "/OS"
                String *os;
                rc = KConfigReadString ( kfg, "OS", &os );
                if ( rc != 0 )
                    throw Exception ( rc, __FILE__, __LINE__, "Could not determine operating system" );
              
                // add the POST parameter
                rc = KHttpRequestAddPostParam ( req, "OS=%S", os );
                StringWhack ( os );
                if ( rc != 0 )
                    throw Exception ( rc, __FILE__, __LINE__, "Failed to post operating system" );
                
                // architecture bits by KConfig "/kfg/arch/bits"
                String *arch;
                rc = KConfigReadString ( kfg, "/kfg/arch/bits", &arch );
                if ( rc != 0 )
                    
                    // add the POST parameter
                    rc = KHttpRequestAddPostParam ( req, "ARCH=%S", arch );

                StringWhack ( arch );
                if ( rc != 0 )
                    throw Exception ( rc, __FILE__, __LINE__, "Failed to post system architecture" );
                
                // TBD - OS version we do not have - should add to KConfig
                
                // our version is KAppVersion
                ver_t vers = KAppVersion ();
                
                // add the POST parameter
                rc = KHttpRequestAddPostParam ( req, "SELF_VERSION=%V", vers );
                if ( rc != 0 )
                    throw Exception ( rc, __FILE__, __LINE__, "Failed to post self version" );

                KHttpResult *rslt;
                rc = KHttpRequestPOST ( req, &rslt );
                if ( rc == 0 )
                {
                    try
                    {
                        printf ( " Success posting a request\n" );
                    
                        uint32_t code;
                        size_t msg_size;
                        char msg_buff [ 1024 ];
                        rc = KHttpResultStatus ( rslt, &code, msg_buff, sizeof msg_buff, &msg_size );
                        if ( rc != 0 )
                            throw Exception ( rc, __FILE__, __LINE__, "Could not retrieve status code" );
                        if ( code != 200 )
                            throw Exception ( rc, __FILE__, __LINE__, "Connection failed with status code: %u", code );
                        
                        KStream *response;
                        rc = KHttpResultGetInputStream ( rslt, &response );
                        if ( rc == 0 )
                        {
                            try
                            {
                                size_t num_read = 0;
                                // TBD - since we are going to control the response
                                // we can modify this size to fit accordingly
                                char buffer [ 4096 ]; 
                            
                                rc = KStreamRead ( response, buffer, sizeof buffer, &num_read );
                                if ( rc != 0  || num_read == 0 )
                                    throw Exception ( rc, __FILE__, __LINE__, "failed to read response from NCBI" );

                                // TBD - information to be obtained from CGI

                                //store update information in prefs
                                UpdatePrefsFromCGI ( buffer );
                            }
                            catch ( ... )
                            {
                                KStreamRelease ( response );
                                throw;
                            }
                            
                            KStreamRelease ( response );
                        }
                    }
                    catch ( ... )
                    {
                        KHttpResultRelease ( rslt );
                        throw;
                    }
                    
                    KHttpResultRelease ( rslt );
                }
            }
            catch ( ... )
            {
                KHttpRequestRelease ( req );
                throw;
            }
            
            KHttpRequestRelease ( req );
        }

        return success;
    }


    // copy data from source file at NCBI to dest file on users system
    static 
    rc_t CC CopyData ( const KFile *src, KFile *dst )
    {
        KDataBuffer buffer;
        rc_t rc = KDataBufferMakeBytes ( &buffer, 1024 * 1024 );
        if ( rc != 0 )
            LogErr ( klogErr, rc, "Failed create 1Mb kdatabuffer " );
        else
        {
            uint64_t read_pos = 0, write_pos = 0;
            size_t num_read = 0, num_writ = 0;

            while ( 1 )
            {
                rc = KFileReadAll ( src, read_pos, buffer . base, ( size_t ) buffer . elem_count, &num_read );
                if ( rc != 0 )
                {
                    LogErr ( klogErr, rc, "Failed to read bytes for source file" );
                    break;
                }
                if ( num_read == 0 )
                    break;


                read_pos += num_read;

                rc = KFileWriteAll ( dst, write_pos, buffer . base, ( size_t ) buffer . elem_count, &num_writ );
                if ( rc != 0 )
                {
                    LogErr ( klogErr, rc, "Failed to write bytes to dest file" );
                    break;
                }
                
                if ( num_writ != num_read )
                {
                    rc = RC ( rcExe, rcFile, rcWriting, rcTransfer, rcIncomplete );
                    LogErr ( klogErr, rc, "Failed to transfer all bytes to dest file" );
                    break;
                }
                
                write_pos += num_writ;
            }
           
            KDataBufferWhack ( &buffer );
        }

        return rc;
    }

#define string_endswith( str, len, ending ) \
    ( ( len ) >= sizeof ( ending ) && \
      memcmp ( & ( str ) [ ( len ) - sizeof ( ending ) + 1 ], ending, sizeof ( ending ) - 1 ) == 0 )             


    static
    rc_t CC HandleFile ( const KDirectory *src_dir, KDirectory *dest_dir, const char *fname ) 
    {
        bool gz = false, bz2 = false;
        size_t fname_size = string_size ( fname );
        size_t dname_size = fname_size; 

        // look for compressed files
        if ( string_endswith ( fname, fname_size, ".gz" ) )
        {
            gz = true;
            dname_size = fname_size - sizeof ".gz" - 1;
        }
        else if ( string_endswith ( fname, fname_size, ".bz2" ) )
        {
            bz2 = true;
            dname_size = fname_size - sizeof ".bz2" - 1;
        }
        
        const KFile *fsrc;
        rc_t rc = KDirectoryOpenFileRead ( src_dir, &fsrc, "%s", fname );
        if ( rc != 0 )
            LogErr ( klogErr, rc, "Failed open file for read" );
        else
        {
            // if file was compressed, uncompress
            if ( gz )
            {
                const KFile *gz;
                rc = KFileMakeGzipForRead ( & gz, fsrc );
                if ( rc == 0 )
                {
                    KFileRelease ( fsrc );
                    fsrc = gz;
                }
            }
            else if ( bz2 )
            {
                const KFile *bz2;
                rc = KFileMakeBzip2ForRead ( & bz2, fsrc );
                if ( rc == 0 )
                {
                    KFileRelease ( fsrc );
                    fsrc = bz2;
                }
            }

            // create the destination file on users system
            KFile *fdest;
            rc = KDirectoryCreateFile ( dest_dir, &fdest, false, 0600, kcmInit | kcmParents
                                        , "%.*s.tmp"
                                        , ( int ) dname_size
                                        , fname );
                                        
            if ( rc != 0 )
                LogErr ( klogErr, rc, "Failed create file" );
            else
            {
                rc = CopyData ( fsrc, fdest );

                KFileRelease ( fdest );
            }

            KFileRelease ( fsrc );


            if ( rc != 0 )
                return rc;

            

            // rename dest.tmp to final name and setting mode bits
            uint32_t access;
            rc = KDirectoryAccess ( src_dir, &access, "%s", fname );
            if ( rc != 0 )
                LogErr ( klogErr, rc, "Failed to acquire directory access" );
            else
            {
                // TBD may need to tune up
                rc = KDirectorySetAccess ( dest_dir, false, access, 0777,  "%.*s.tmp", ( int ) dname_size, fname );
                if ( rc != 0 )
                    LogErr ( klogErr, rc, "Failed to set directory access" );
                else
                {
                    KDataBuffer names;
                    rc = KDataBufferMakeBytes ( & names, dname_size + dname_size + sizeof ".tmp" - 1 + 2 );
                    if ( rc != 0 )
                        LogErr ( klogErr, rc, "Failed create KDataBuffer" );
                    else
                    {
                        char * from = ( char * ) names . base;
                        char * to = from + dname_size + sizeof ".tmp";
                        
                        string_printf ( from, dname_size + sizeof ".tmp", NULL, "%.*s.tmp", ( uint32_t ) dname_size, fname );
                        string_printf ( to, dname_size + 1, NULL, "%.*s", ( uint32_t ) dname_size, fname );
                        
                        rc = KDirectoryRename ( dest_dir, true, from, to );
                        if ( rc != 0 )
                            LogErr ( klogErr, rc, "Failed to rename directory" );

                        KDataBufferWhack ( & names );
                    }
                }
            }
        }

        return rc;
    }

    static
    rc_t CC HandleAlias ( const KDirectory *src_dir, void *dest_dir, const char *name ) 
    {
        char resolved [ 4096 ];
        // TBD - check shallow resolution of alias
        rc_t rc = KDirectoryResolveAlias ( src_dir, false, resolved, sizeof resolved, "%s", name );
        if ( rc == 0 )
            rc = KDirectoryCreateAlias ( ( KDirectory * ) dest_dir, 0777, kcmInit | kcmParents, resolved, name );

        return rc;
    }

    /* HandleSourceFiles
     * dir = source dir
     * name = name of file
     * data = destination dir
     */
    static
    rc_t CC HandleSourceFiles ( const KDirectory *src, uint32_t type, const char *name, void *data )
    {
        KDirectory *dst = ( KDirectory * ) data;

        // check for file type
        switch ( type )
        {
        case kptFile:
            return HandleFile ( src, dst, name );
        case kptDir:
            break;
        case kptFile | kptAlias:
        case kptDir | kptAlias:
            return HandleAlias ( src, dst, name );
        default:
            return RC ( rcExe, rcNoTarg, rcValidating, rcNoObj, rcError );
        }


        // handle directory here. recursive.
        rc_t rc = KDirectoryCreateDir ( dst, 0777, kcmOpen | kcmParents, "%s", name );
        if ( rc != 0 )
            LogErr ( klogErr, rc, "Failed to create directory directory" );
        else
        {
            KDirectory *sub; 
            rc = KDirectoryOpenDirUpdate ( dst, & sub, false, "%s", name );
            if ( rc != 0 )
                LogErr ( klogErr, rc, "Failed to open directory for update" );
            else
            {
                rc = KDirectoryVisit ( src, false, HandleSourceFiles, sub, "%s", name );
                
                KDirectoryRelease ( sub );
            }
        }

        return rc;
    }

    static
    void DownloadAndInstallToolkit ( const PrefsData & params )
    {
        // create the destination directory of the installation
        rc_t rc = KDirectoryCreateDir ( wd, 0775, kcmOpen | kcmParents, 
                                        "%s", params . path_to_installation . c_str () ); 
        if ( rc == 0 )
        {
            // open for update
            KDirectory *dest_dir;
            rc = KDirectoryOpenDirUpdate ( wd, &dest_dir, false, 
                                           "%s", params . path_to_installation . c_str () );
            if ( rc == 0 )
            {
                // establish connection to NCBI to access source files
                const KFile *src_file;
                rc = KNSManagerMakeReliableHttpFile ( kns, &src_file, NULL, 0x01010000,  
                                              "%s", path_to_url . c_str () ); 
                if ( rc == 0 )
                {
                    VPath *url;
                    rc = VFSManagerMakePath ( vfs, & url, "%s", path_to_url . c_str () );
                    if ( rc == 0 )
                    {
                        const KDirectory *src_dir;
                        // hack to mount kar file in memory
                        rc = VFSManagerOpenDirectoryReadDirectoryRelative ( vfs, wd, &src_dir, url );
                        if ( rc == 0 )
                        {
                            // visit each entry in the directory and handle
                            rc = KDirectoryVisit ( src_dir, false, HandleSourceFiles, dest_dir, "." );

                            KDirectoryRelease ( src_dir );
                        }

                        VPathRelease ( url );
                    }

                    KFileRelease ( src_file );
                }

                KDirectoryRelease ( dest_dir );
            }
        }
        if ( rc != 0 )
            throw Exception ( __FILE__, __LINE__, "failed to get visit src_dir" );
    }


    static
    bool LatestVersionValid ( const PrefsData & params )
    {
        // latest version has not yet been set
        if ( params . l_vers == 0 )
            return false;

        // the params should contain only valid dates
        if ( current_time < params . last_check_date )
            throw Exception ( __FILE__, __LINE__, "%s", "invalid date" );

        // TBD - allow for some time interval
        // this may also be part of prefs - probably should be
        // default might be 1 week... 1 day?
        if ( current_time - params . last_check_date <= 
             params . last_check_date_interval_limit );
            return true;

        // failed
        return false;
    }

    static
    bool CheckLatestVersion ( const PrefsData & params )
    {
        if ( LatestVersionValid ( params ) )
            return true;

        if ( ContactNCBIForVersion ( params ) )
        {
            prefs_data . last_check_date = current_time;
            dirty = true;

            return true;
        }

        return false;
    }

    static
    void EnsureLatestVersion ( const PrefsData & params )
    {
        if ( ! CheckLatestVersion ( params ) )
            return;

        //if ! user wants to be asked prefs.dont-ask-before-download
        // TBD maybe notify an installation is happening?
        if ( params . auto_download_enabled )
            DownloadAndInstallToolkit ( params );
        else
        {
            //ask if they want to download? [Yn]

            // first time around
            if ( params . c_vers == 0 )
                std :: cout << "Would you like to download and install the SRAToolkit? [Y/n] ";
            else
                std :: cout << "A newer version of SRAToolkit is available. Would you like to download and install? [Y/n] ";

            std :: cout . flush ();

            std :: string ch;
            std :: getline ( std :: cin, ch );

            if ( ch . empty () || ch . compare ( "y" ) == 0 || ch . compare ( "Y" ) == 0 )
                DownloadAndInstallToolkit ( params );
        }
    }

    static
    void EnsureVDBConfig ( const PrefsData & params )
    {
        //must have user public repository
        String *str;
        rc_t rc = KConfigReadString ( kfg, "/user/public", &str );
        if ( rc != 0 )
            throw Exception ( rc, __FILE__, __LINE__, "Failed to determine public repository" );
        StringWhack ( str );

        //must have ncbi remote
        rc = KConfigReadString ( kfg, "/ncbi/remote", &str );
        if ( rc != 0 )
            throw Exception ( rc, __FILE__, __LINE__, "Failed to determine remote repository" );
        StringWhack ( str );

        //update KConfig with anything missing
    }

    static
    void SavePrefs ( void )
    {
        //return if ! dirty
        if ( ! dirty )
            return;

        //open prefs file for write
        FILE *prefs = fopen ( path_to_prefs . c_str (), "w" );
        if ( prefs == NULL )
            throw Exception ( __FILE__, __LINE__, "Failed to open file for save" );

        //write all prefs to prefs file
        char buffer [ 256 ];
        
        string_printf ( buffer, sizeof buffer, NULL, "%T", prefs_data . dl_date );  
        fprintf ( prefs, "download-date = %s\n", buffer );
        string_printf ( buffer, sizeof buffer, NULL, "%T", prefs_data . last_check_date );
        fprintf ( prefs, "last-check-date = %s\n", buffer );

        string_printf ( buffer, sizeof buffer, NULL, "%V", prefs_data . l_vers );  
        fprintf ( prefs, "latest-version = %s\n", buffer );
        string_printf ( buffer, sizeof buffer, NULL, "%V", prefs_data . c_vers );  
        fprintf ( prefs, "current-version = %s\n", buffer );

        fprintf ( prefs, "download-url = %s\n", prefs_data . download_url . c_str () );
        fprintf ( prefs, "latest-ver-url = %s\n", prefs_data . latest_vers_url . c_str () );
        fprintf ( prefs, "path-to-installation = %s\n", prefs_data . path_to_installation . c_str () );

        fprintf ( prefs, "auto-download-enabled = %s\n", prefs_data . auto_download_enabled ? "true" : "false" );

        //close prefs file
        fclose ( prefs );

        //mark not dirty
        dirty = false;
    }

    //!!!
    static
    void RunSRAShell ( const PrefsData & params )
    {
        // append toolkit directory to PATH
        std :: string NEW_PATH ( path_to_binaries );
        NEW_PATH += ":";
        NEW_PATH += PATH;
        // TBD - use putenv or setenv to update PATH

        // launch sra-shell with remaining g_argv

        // wait upon return

        // throw exception if error
    }

    static
    void SRAMain ( void )
        throw ()
    {
        try
        {
            // make a copy of prefs data for current session
            PrefsData params ( prefs_data );

            // allow user to override prefs from command line
            OverrideParamsFromCmdLine ( params );

            // *** now have a complete idea of what we want to do ***

            // try to find the toolkit
            // not guaranteed to find anything
            LocateSRAToolkit ( params );

            // make sure that the software we have is up to date
            EnsureLatestVersion ( params );

            EnsureVDBConfig ( params );

            SavePrefs ();

            RunSRAShell ( params );
        }
        catch ( Exception & x )
        {
            // report nature of problem
        }
        catch ( ... )
        {
            // report unknown error
        }
    }

    static 
    void Cleanup ( void )
    {
        // close managers
        KNSManagerRelease ( kns );
        kns = NULL;

        KDirectoryRelease ( wd );
        wd = NULL;

        KConfigRelease ( kfg );
        kfg = NULL;
    }

    static
    void run ( int argc, char *argv [] )
    {
        Init ( argc, argv );
        SRAMain ();
        Cleanup ();
    }
}

extern "C"
{
    int prefs_yyget_lineno ( void );
    int prefs_yylex ( union YYSTYPE *val );

    int prefs_lex ( YYSTYPE *val )
    {
        return prefs_yylex ( val );
    }

    void prefs_error ( const char *msg )
    {
        std :: cerr
            << sra :: prgm_name
            << ':'
            << prefs_yyget_lineno ()
            << ": Error in preferences file: "
            << msg
            << std :: endl;
    }

    void  prefs_token_error ( const Token *token, const char *msg )
    {
        std :: cerr
            << sra :: prgm_name
            << ':'
            << token -> lineno
            << ": Error in preferences file: "
            << msg
            << std :: endl;
    }

    static
    bool CheckValidTimeStamp ( const Token *token )
    {
        if ( token -> val . t < sra :: lower_date_limit )
        {
            prefs_token_error ( token, "timestamp is too old" );
            return false;
        }
        if ( token -> val . t > sra :: current_time )
        {
            prefs_token_error ( token, "timestamp is in the future" );
            return false;
        }

        return true;
    }

    static 
    bool CheckValidToolkitVersion ( const Token *token )
    {
        const ver_t two_three = ( 2 << 24 ) | ( 3 << 16 );
        if ( token -> val . v < two_three )
        {
            prefs_token_error ( token, "version is too old" );
            return false;
        }
        // To the reader: 
        // We have no way of knowing if the version beyond the latest
        // without contacting NCBI.
        // All version will have to be post-validated.

        return true;
    }

    static
    bool CheckValidURL ( const Token *token )
    {
        bool valid = false;
        // create VPath using VFSManagerMakePath
        VPath *path;
        rc_t rc = VFSManagerMakePath ( sra :: vfs, &path, "%s", token -> val . c );
        if ( rc != 0 )
            prefs_token_error ( token, "could not establish a valid path to check url" );
        else
        {
            // check that the scheme is http using VPathGetScheme.
            String scheme;
            rc = VPathGetScheme ( path, &scheme );
            if ( rc != 0 )
                prefs_token_error ( token, "could not establish scheme of url" );
            else
            {
                String http;
                CONST_STRING ( & http, "http" );
                if ( ! StringCaseEqual ( & scheme, & http ) )
                    prefs_token_error ( token, "not a valid http url" );
                else
                {
                    // check that the host is NCBI
                    String host;
                    rc = VPathGetHost ( path, &host );
                    if ( rc != 0 )
                        prefs_token_error ( token, "could not establish host of url" );
                    else
                    {
                        String ncbi;
                        CONST_STRING ( &ncbi, "www.ncbi.nlm.nih.gov" );
                        if ( ! StringCaseEqual ( &host, &ncbi ) )
                            prefs_token_error ( token, "not a valid host for url" );
                        else
                            valid = true;
                    }
                }
            }
            VPathRelease ( path );
        }
        return valid;
    }

    static
    bool CheckValidPath ( const Token *token )
    {
        uint32_t type = KDirectoryPathType ( sra :: wd, "%.*s", token -> len, token -> val . c );
        switch ( type )
        {
        case kptFile:
        case kptDir:
        case kptFile | kptAlias:
        case kptDir | kptAlias:
            return true;
        }
        return false;
    }

    void PrefsDataSetDownloadDate ( const Token  *download_date )
    {
        if ( CheckValidTimeStamp ( download_date ) )
            sra :: prefs_data . dl_date = download_date -> val . t;
    }

    void PrefsDataSetLastCheckDate ( const Token *last_check_date )
    {
        if ( CheckValidTimeStamp ( last_check_date ) )
            sra :: prefs_data . last_check_date = last_check_date -> val . t;
    }

    void PrefsDataSetLatestVersion ( const Token *latest_version )
    {
        if ( CheckValidToolkitVersion ( latest_version ) )
             sra :: prefs_data . l_vers = latest_version -> val . v;
    }

    void PrefsDataSetCurrentVersion ( const Token *current_version )
    {
        if ( CheckValidToolkitVersion ( current_version ) )
            sra :: prefs_data . c_vers = current_version -> val . v;
    }

    void PrefsDataSetDownloadURL ( const Token *download_url )
    {
        if ( CheckValidURL ( download_url ) )
            sra :: prefs_data . download_url = std :: string ( download_url -> val . c, 
                                                               download_url -> len );
    }

    void PrefsDataSetLatestVersionURL ( const Token *latest_version_url )
    {
        if ( CheckValidURL ( latest_version_url ) )
            sra :: prefs_data . latest_vers_url = std :: string ( latest_version_url -> val . c, 
                                                                  latest_version_url -> len );
    }

    void PrefsDataSetPathToInstallation ( const Token *path_to_installation )
    {
        if ( CheckValidPath ( path_to_installation ) )
            sra :: prefs_data . path_to_installation = std :: string ( path_to_installation -> val . c,
                                                                       path_to_installation -> len );
    }

    void PrefsDataSetAutoDownloadEnabled ( bool which )
    {
        sra :: prefs_data . auto_download_enabled = which;
    }

    /* Version  EXTERN
     *  return 4-part version code: 0xMMmmrrrr, where
     *      MM = major release
     *      mm = minor release 
     *    rrrr = bug-fix release
     */
    ver_t CC KAppVersion ( void )
    {
        return SRA_VERS;
    }
    
    
    const char UsageDefaultName[] = "sra";

    /* Usage
     *  This function is called when the command line argument
     *  handling sees -? -h or --help
     */
    rc_t CC UsageSummary ( const char *progname )
    {
        return KOutMsg (
              "\n"
              "Usage:\n"
              "  %s [Options]\n"
              "\n"
              "Summary:\n"
              "  Launch bash shell with appropriate environment configuration.\n"
              , progname );
    }
    
    rc_t CC Usage ( const Args *args )
    {
#if 0
        uint32_t i;
        const char *progname, *fullpath;
        rc_t rc = ArgsProgram ( args, & fullpath, & progname );
        if ( rc != 0 )
            progname = fullpath = UsageDefaultName;
        
        UsageSummary ( progname );
        
        KOutMsg ( "Options:\n" );
        
        for ( i = 0; i < sizeof options / sizeof options [ 0 ]; ++ i )
        {
            HelpOptionLine ( options [ i ] . aliases, options [ i ] . name,
                             option_params [ i ], options [ i ] . help );
        }
        
        HelpOptionsStandard ();
        
        HelpVersion ( fullpath, KAppVersion () );
#endif   
        return 0;
    }

    rc_t CC KMain ( int argc, char *argv [] )
    {
        rc_t rc = 0;

        try
        {
            sra :: run ( argc, argv );
        }
        catch ( sra :: Exception &x )
        {
            rc = x . return_code ();
#if _DEBUGGING
            // TBD - check that this doesn't conflict with internal attempts
            // at printing filename and lineno. It's probably just going to repeat...
            pLogErr ( klogErr, rc, "$(file):$(lineno): $(msg)",
                      "file=%s,lineno=%u,msg=%s"
                      , x . file_name ()
                      , x . line_number ()
                      , x . what () );
#else
            LogErr ( klogErr, rc, x . what () );
#endif
        }
        catch ( std :: exception & x )
        {
            rc = RC ( rcExe, rcNoTarg, rcExecuting, rcNoObj, rcUnknown );
            LogErr ( klogFatal, rc, x . what () );            
        }
        catch ( ... )
        {
            rc = RC ( rcExe, rcNoTarg, rcExecuting, rcNoObj, rcUnknown );
            rc = LogErr ( klogFatal, rc, "unknown error" );
        }

        return rc;

    }
}
