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
#include <kfg/config.h>
#include <kns/manager.h>
#include <kns/http.h>
#include <kns/stream.h>
#include <kfs/file.h>
#include <kfs/directory.h>

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


#define OPT_IGNORE_FAILURE "ignore-failure"
#define OPT_FORCE "force"

static const char *hlp_ignore_failure [] = { "ignore failure when ......", NULL  };
static const char *hlp_force [] = { "force overwrite of exisiting destination", NULL };

static OptDef options [] = 
{
    /* 1. long-name
       2. list of single character short names
       3. help-gen function
       4. list of help strings, NULL terminated
       5. max count
       6. option requires value
       7. option is required
    */
    { OPT_IGNORE_FAILURE, "i", NULL, hlp_ignore_failure, 1, false, false }
    , { OPT_FORCE, "f", NULL, hlp_force, 1, true, false }   
};

static const char *option_params [] =
{
    NULL
    , NULL
};
    
namespace workspace
{
    struct PrefsData
    {
        PrefsData ();

        KTime_t dl_date;     // download date
        KTime_t last_check_date; 
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
        , l_vers ( 0 )
        , c_vers ( 0 )
        , download_url ( "http://ftp-trace.ncbi.nlm.nih.gov/sra/sdk" )
          // NB - this is currently a static file
          // needs to be replaced with CGI
        , latest_vers_url ( "http://ftp-trace.ncbi.nlm.nih.gov/sra/sdk/current/sratoolkit.current.version" )
        , auto_download_enabled ( true )
    {
    }

    static PrefsData prefs_data;
    static bool dirty;

    static std :: string latest_vers_string; // TBD - record this from NCBI
    static std :: string path_to_prefs;
    static KDirectory * wd;
    static KNSManager * kns;


    /* Initialize to specified default settings 
     *  Latest version
     *  ( last check date )
     *  url for checking latest version 
     *  current version 
     *  download date
     *  path to the installation 
     */
    static
    void InitWithDefaultSettings ()
    {
        // "prefs_data is initialized via constructor

        // by default, we will look for prefs file in user's home directory
        // set the default path to prefs file
        const char * HOME = getenv ( "HOME" );
#if WINDOWS
        if ( HOME == NULL )
            HOME = getenv ( "USERPROFILE" );
#endif
        if ( HOME != NULL )
        {
            path_to_prefs = HOME;
            path_to_prefs += "/.ncbi/sratoolkit.prefs";
        }

        // some operations will require access to KFS
        rc_t rc = KDirectoryNativeDir ( & wd );
        if ( rc != 0 )
            throw Exception ( rc, __FILE__, __LINE__, "failed to create native KDirectory" );

        // other operations will require access to KNS
        rc = KNSManagerMake ( & kns );
        if ( rc != 0 )
            throw Exception ( rc, __FILE__, __LINE__, "failed to create KNS manager" );
    }



    static
    const KFile * FindUserPreferences ( const Args *args )
    {
        const KFile * prefs = NULL;

        // see if the user is giving us specific prefs
        uint32_t opt_count;
        const char *opt_string = "-f"; // TBD - what option are we counting / looking for  here
        rc_t rc = ArgsOptionCount ( args, opt_string, & opt_count );
        if ( rc == 0 && opt_count != 0 )
        {
            // send in 0-based index to fetch last prefs option
            const char *opt_value = NULL;
            rc = ArgsOptionValue ( args, opt_string, opt_count - 1, & opt_value );
            if ( rc == 0 && opt_value != NULL )
            {
                rc = KDirectoryOpenFileRead ( wd, & prefs, "%s", opt_value );
                if ( rc == 0 )
                {
                    path_to_prefs = std :: string ( opt_value );
                    return prefs;
                }
            }
        }

        // check factory defaults
        // if no $HOME could be determined, we have a problem
        if ( path_to_prefs . size () == 0 )
        {
            rc = RC ( rcExe, rcFile, rcOpening, rcPath, rcUndefined );
            throw Exception ( rc, __FILE__, __LINE__, "path to preferences could not be established" );
        }

        // use factory defaults
        rc = KDirectoryOpenFileRead ( wd, & prefs, "%s", path_to_prefs.c_str() );
        if ( rc == 0 )
            return prefs;
        
        // no prefs could be found
        DBGMSG ( DBG_APP, -1, ("no prefs found ( rc = %R )\n", rc ) );

        // leave factory defaults in place
        return NULL;
        
    }

    /* Init with users previous settings, 
     * always initialized with default settings */
    static 
    void InitWithUserSettings ( const KFile *prefs )
    {
#if 0
        // 1. initialize lexical scanner with "prefs" file.
        //    it will then read the entire text of the file
        // 2. call parser function, pass in the address of "prefs_data"
        //    the PARSER will call all of the functions below whenever
        //    it sees a line trying to set the value
        // 3. call the function to cleanup the lexical scanner
#endif
#if 0
        if ( PrefsDataSetDownloadDate ( &prefs_data, prefs . download_date ) )
            dirty = true;
        if ( PrefsDataSetLastCheckDate ( &prefs_data, prefs . last_check_date ) )
            dirty = true;
        if ( PrefsDataSetLatestVersion ( &prefs_data, prefs . latest_version ) )
            dirty = true;
        if ( PrefsDataSetCurrentVersion ( &prefs_data, prefs . current_version ) )
            dirty = true;
        if ( PrefsDataSetDownloadURL ( &prefs_data, prefs . download_url ) )
            dirty = true;
        if ( PrefsDataSetLatestVersionURL ( &prefs_data, prefs . latest_version_url ) )
            dirty = true;
        if ( PrefsDataSetPathToInstallation ( &prefs_data, prefs . path_to_installation ) )
            dirty = true;
        if ( PrefsDataSetAutoDownloadEnabled ( &prefs_data, prefs . download_enabled ) )
            dirty = true;
#endif
    }

    static 
    void InitWithCmdLineSettings ( const Args *args, const uint32_t count )
    {
        const char *dst;
        rc_t rc = ArgsParamValue ( args, count - 1, &dst );
        if ( rc != 0 )
            throw Exception ( rc, __FILE__, __LINE__, "ArgsParamValue [ %u ] failed", count - 1 );

        // TBD - switch to the "prefs" data and the "working" data or whatever
        // allow the command line to change working data, but not prefs
    }

    static 
    void Init ( int argc, char *argv [] )
    {
        InitWithDefaultSettings ();

        // parse command line
        Args *args;
        rc_t rc = ArgsMakeAndHandle ( & args, argc, argv, 0 );
        if ( rc != 0 )
            throw Exception ( rc, __FILE__, __LINE__, "failed to parse args list" );

        // have to perform these steps within try/catch
        // in order to handle cleanup of "args"
        try
        {
            const KFile *prefs = FindUserPreferences ( args );
            if ( prefs != NULL )
            {
                try
                {
                    InitWithUserSettings ( prefs );
                }
                catch ( ... )
                {
                    KFileRelease ( prefs );
                    throw;
                }

                KFileRelease ( prefs );
            }
        
            // Command line settings trump previous user settings
            uint32_t count;
            rc = ArgsParamCount ( args, &count );
            if ( rc != 0 )
                throw Exception ( rc, __FILE__, __LINE__, "ArgsParamCount failed" );
            else if ( count < 2 )
            {
                rc = RC ( rcExe, rcArgv, rcParsing, rcParam, rcInsufficient );
                throw Exception ( rc, __FILE__, __LINE__, " expect source amd destination parameters" );
            }
            else
            {
                InitWithCmdLineSettings ( args, count );
            }
            
            
        }
        catch ( ... )
        {
            ArgsWhack ( args );
            throw;
        }

        ArgsWhack ( args );
    }

    static
    void Cleanup ()
    {
        KNSManagerRelease ( kns );
        kns = NULL;

        KDirectoryRelease ( wd );
        wd = NULL;
    }


    static
    bool ReadInstallationVersion ()
    {
        return false;
    }

    /* Contact NCBI to find latest version of the ToolKit
     * No failure if connection is not established
     * if connection established, download information will be 
     * gathered here
     */
    static
    bool GetLatestToolkitVersion ()
    {
        bool success = false;

        // NB - since all functions underneath are C, they are "nothrow"
        // there is no need for try/catch blocks unless C++ functions are
        // later interleaved.

        // read the latest published version from NCBI
        // NB - this has to change to use CGI
        assert ( prefs_data . latest_vers_url . c_str () != 0 ); // assume the url has been properly set

        KHttpRequest *req;
        rc_t rc = KNSManagerMakeRequest ( kns, &req, 0x01010000, NULL,
            "%s", prefs_data . latest_vers_url . c_str () );
        if ( rc == 0 )
        {
            KHttpResult *rslt;
            rc = KHttpRequestGET ( req, &rslt );
            if ( rc == 0 )
            {
                printf ( " Success Making a request\n" );

                uint32_t code;
                size_t msg_size;
                char msg_buff [ 1024 ];
                rc = KHttpResultStatus ( rslt, &code, msg_buff, sizeof msg_buff, &msg_size );
                if ( rc != 0 )
                    printf ( "Could not retrieve status code\n" );
                else if ( code != 200 )
                    printf ( "Connection failed with status code:%d\n", code  );
                else
                {
                    // TBD - do this
                    // read the version ( text to ver_t )
                    KStream *vers; 
                    rc = KHttpResultGetInputStream ( rslt, &vers );
                    if ( rc == 0 )
                    {
                        size_t num_read = 0;
                        char buffer [ 256 ];
                        
                        rc = KStreamRead ( vers, buffer, sizeof buffer, &num_read );
                        if ( rc != 0 || num_read == 0 )
                        {
                            printf ( "Error retrieving version number\n" );
                            return false;
                        }
                        else
                        {
                            printf( "Version: %s\n", buffer );
                            
                            ver_t new_vers = 0;
                            if ( prefs_data . l_vers != new_vers )
                            {
                                // detect if different from current l_vers, update "dirty" flag
                                dirty = true;
                                // store it in static variables
                                prefs_data . l_vers = new_vers;
                                success = true;
                            }
                        }

                        KStreamRelease ( vers );
                    }
                }

                KHttpResultRelease ( rslt );
            }
            
            KHttpRequestRelease ( req );
        }

        return success;
    }

    /* Locate the toolkit in the user's environment
     * If it is not found, prompt the user if they have an existing version elsewhere
     *  YES - Have the user direct us to it, and specify their desired directory
     *        update location in prefs if it has changed. 
     *        If newer version exists, prompt to download and install.
     *  NO  - Prompt user to download and install the toolkit and specify a directory 
     */
    static
    bool LocateToolkit ()
    {
        rc_t rc;
        const KFile *file;

        // first, test value from prefs
        if ( prefs_data . path_to_installation . size () != 0 )
        {
            // test if this path still points at an installation
            rc = KDirectoryOpenFileRead ( wd, &file, "%s", prefs_data . path_to_installation . c_str () );
            if ( rc == 0 )
            {
                KFileRelease ( file );
                return true;
            }
        }

        // try to find toolkit in current $PATH
        // NB - can be dangerous, since the tools may not belong to user
        char *PATH = getenv ( "PATH" );
        for ( char *path = strtok ( PATH, ":" ) ; path ; path = strtok ( NULL, ":" ) )
        {
            size_t psize = strlen ( path );
            const char fname [] = "sratoolkit.exe";
            path = strncat ( path, fname, psize + sizeof fname );
            rc = KDirectoryOpenFileRead ( wd, &file, "%s", path );
            if ( rc == 0 )
            {
                KFileRelease ( file );
                return true;
            }
        } 

        // prompt user for location of previous installation
        std :: string path;
        std :: cout << "A previous installation of SRAToolkit was not found." << std :: endl
                    << "If you have previously installed SRAToolkit, please indicate"
                    << "where it is located. Otherwise type NA." << std :: endl
                    << "Location: ";
        while ( 1 )
        {
            std :: getline ( std :: cin, path );
            if ( path . compare ( "NA" ) == 0 )
                break; // at this point the user has indicated tha the installation is not located anywhere else or they are done searching
                       // should we prompt the user to tell us where he wants it installed, or exit.
            else
            {
                rc = KDirectoryOpenFileRead ( wd, &file, "%s", path . c_str () );
                if ( rc != 0 )
                {
                    std :: cout << "Unable to locate SRAToolkit at: '"
                                << path
                                << '\'' << std :: endl
                                << "Please indicate another location. Otherwise type NA" << std :: endl
                                << "Location: ";
                    continue;
                }
                else
                {
                    KFileRelease ( file );
                    
                    // if user gave a new place, update prefs_data and set dirty
                    //if ( ! PrefsDataSetPathToInstallation ( path . c_str () ) )
                    //  throw Exception ( rc, __FILE__, __LINE__, "Failed to update path to installation" );

                    dirty = true;
                    return true;
                }
            }
        }
        return false;
    }

#if 0 //
    static
    rc_t RunInstaller ()
    {
        // cases:
        //  1. linux & mac: *.Kar.gz
        //     read first few bytes ( <= 256K )
        //     detect gz file, wrap in gunzip file ( <kfs/gzip.h> KFileMakeGzipForRead )
        //     read first few bytes again
        //     detect kar file
        //     convert to KDirectory ( <kfs/tar.h> KDirectoryOpenTarArchiveRead_silent_preopened ? )
        //     extract directory to target location ( <kfs/directory.h> KDirectoryCopy )
        //  2. windows:
        //     the downloaded file should be executable
        //     you will run
        rc_t rc;

#if WINDOWS
#else
        const KFile *file = NULL;
        rc = KDirectoryOpenFileRead ( wd, &file
                                      , "/tmp/sratoolkit.%s-%s%u%s"
                                      , latest_vers_string . c_str ()
                                      , os
                                      , ( uint32_t ) _ARCH_BITS
                                      , ext
            );
        if ( rc == 0 )
        {
            char buffer [ 10 ];
            size_t num_read;
            rc = KFileRead ( file, 0, buffer, sizeof buffer, &num_read );
            if ( rc == 0 && num_read == 10 )
            {
                if ( buffer [ 0 ] == 0x1f && buffer [ 1 ] == 0x8b )// anything else to check?
                {
                    // we have have a gzip file
                    const KFile *gz = NULL;
                    rc = KFileMakeGzipForRead ( &gz, file );
                    if ( rc == 0 )
                    {
                        //somehow use kar code.
                        // Make a directory from kar file
                        KDirectory *kardir = NULL;

                        rc = KDirectoryCopy ( kardir, wd, true, kardir, wd );
                        if ( rc == 0 )
                        {
                            // success
                        }

                        KFileRelease ( gz );
                    }
                }
            }
            KFileRelease ( file );
        }
#endif


#if 0
        /* fork to child process and run installer*/
        rc_t rc = 0;
        int lerrno, status;

        pid_t pid = fork ();
        switch ( pid )
        {
        case -1:
            rc = RC ( rcExe, rcNoTarg, rcValidating, rcNoObj, rcError );
            break;
        case 0:            /* name of file to execute, name associated with file being executed, sratoolkit package ( buffer )*/
            status = execl ( "copycat", "copycat", buffer,  NULL );
            
            /* having returned here, execl failed */
            lerrno = errno;
            printf("This print is after execl() and should not have been executed if execl were successful! \n\n");
            rc = RC ( rcExe, rcNoTarg, rcReading, rcNoObj, rcError );
            PLOGERR (klogErr,
                     (klogErr, rc, "unknown system error '$(S)($(E))'",
                      "S=%!,E=%d", lerrno, lerrno));
            
            /* MUST exit here to kill the forked child process */
            exit ( status );
        default:
            /* NOW - you need to wait until your child exits */
            pid = waitpid ( pid, & status, 0 );
            
            /* pid should be valid */
            if ( pid < 0 )
                rc = RC ( rcExe, rcNoTarg, rcValidating, rcNoObj, rcError );
            else if ( status != 0 )
            {
                /* you can display some info here */
                rc = RC ( rcExe, rcNoTarg, rcValidating, rcNoObj, rcError );
            }
        }
#endif
        return rc;
    }

#endif //
    static 
    void SetupToolkitConfig ()
    {
    }

    static
    bool DownloadAndInstallLatestToolkit ()
    {
        bool success = false;

        // test for download location
        if ( prefs_data . path_to_installation . size () == 0 )
        {
            // ask user for preferred location
            // suggest something by default
        }

        const char *os, *ext;
#if LINUX
        os = "centos_linux";
        ext = ".tar.gz";
#elif MAC
        os = "mac";
        ext = ".tar.gz";
#elif WINDOWS
        os = "win";
        ext = ".zip";
#else
#error "this OS is not supported"
#endif

        // build a URL based upon:
        //  1. base URL
        //  2. version
        //  3. OS
        //  4. architecture
        char download_url [ 4096 ];
        rc_t rc = string_printf ( download_url, sizeof download_url, NULL,
                                  "%s/%s/sratoolkit.%s-%s%u%s"
                                  , prefs_data . download_url . c_str ()
                                  , latest_vers_string . c_str ()
                                  , latest_vers_string . c_str ()
                                  , os
                                  , ( uint32_t ) _ARCH_BITS
                                  , ext
            );

        // retrieve latest toolkit package by version
        // NB - on Linux and Mac, this is normally a *.tar.gz file
        // on Windows, it may be one, too, or it may become an installer...
        const KFile *in_file;
        rc = KNSManagerMakeReliableHttpFile ( kns, &in_file, NULL, 0x01010000, download_url );
        if ( rc == 0 )
        {
            // TBD - is path-to-installation the directory of the toolkit,
            // or is it the directory of its binaries? should be directory
            // of the toolkit itself, but this complicates browsing...
            // EXAMPLE: "$HOME/sratoolkit.2.3.4/bin/fastq-dump" is an executable
            // then "prefs_data.path_to_installation" should be "$HOME/sratoolkit.2.3.4"
            // which brings up another issue - do we want to recognize this pattern
            // and rename it to remove the version number? auto download wants to
            // either use a consistent directory, or introduce parallel directories.
            //
            // SO - either install a new directory altogether,
            // or use the same directory but erase prior contents,
            // or use the same directory and leave prior contents.

            // DOWNLOAD LOCATION - different by platform
            KFile *out_file;
            rc = KDirectoryCreateFile ( wd, &out_file, false, 0664, kcmInit | kcmParents,
#if WINDOWS
                                        // TBD - this should involve the username and process id
                                        "%s/tmp/sratoolkit.%s-%s%u%s"
                                        , prefs_data . path_to_installation . c_str ()
                                        , latest_vers_string . c_str ()
                                        , os
                                        , ( uint32_t ) _ARCH_BITS
                                        , ext
#else
                                        // TBD - this should involve the username and process id
                                        "/tmp/sratoolkit.%s-%s%u%s"
                                        , latest_vers_string . c_str ()
                                        , os
                                        , ( uint32_t ) _ARCH_BITS
                                        , ext
#endif
                );
            if ( rc == 0 )
            {
                uint64_t in_fsize;
                rc = KFileSize ( in_file, &in_fsize );
                if ( rc == 0 )
                {
                    // allocate a buffer
                    size_t bsize = 1024 * 1024;
                    void *buffer = malloc ( bsize );
                    if ( buffer == NULL )
                        rc = RC ( rcExe, rcNoTarg, rcAllocating, rcNoObj, rcNull );
                    else
                    {
                        uint64_t pos;
                        size_t num_read;
                        for ( pos = 0, num_read = 0 ; rc == 0 && pos < in_fsize ; pos += num_read )
                        {
                            uint64_t to_read = in_fsize - pos;
                            if ( to_read > ( uint64_t ) bsize )
                                to_read = bsize;
                            rc = KFileReadAll ( in_file, pos, buffer, ( size_t ) to_read, &num_read );
                            if ( rc != 0 )
                                printf ( "Error reading download file" );
                            else
                            {
                                size_t num_writ = 0;
                                rc = KFileWriteAll ( out_file, pos, buffer, num_read, &num_writ );
                                if ( rc != 0 )
                                    printf ( "Error writing to tmp file" );
                            }
                        }

                        free ( buffer );
                    }
                }

                KFileRelease ( out_file );
            }

            KFileRelease ( in_file );
        }

        // NOW, you have to unpack and install the file!
        //rc = RunInstaller ();

        // setup toolkit config
        SetupToolkitConfig ();
        return success;
    }


    static
    bool EnsureEverythingIsThere ()
    {
        bool have_toolkit = false;
        if ( LocateToolkit () )
        {
            // run a special tool ( TBD )
            // capture its output as a vers
            // compare it to stored prefs_data.c_vers
            // if different, update static and set dirty
            have_toolkit = ReadInstallationVersion ();
        }

        if ( prefs_data . auto_download_enabled )
        {
            if ( GetLatestToolkitVersion () )
            {
                if ( prefs_data . l_vers != prefs_data . c_vers )
                {
                    // download and install toolkit
                    if ( DownloadAndInstallLatestToolkit () )
                        have_toolkit = true;
                }
            }
        }

        return have_toolkit;
    }


    static
    void UpdateEnvironment ()
    {
        /* makre sure OS + SHELL environment is set up correctly */
        const char *env = getenv ( "PATH" );
        size_t env_size = strlen ( env );

        char path [ env_size + 4096 ];
        rc_t rc = string_printf ( path, sizeof path, NULL, 
                                  "%s:%s"
                                  , prefs_data . path_to_installation . c_str ()
                                  , env
            );
        if ( rc != 0 )
            ;// some error throw

        if ( putenv ( path ) )
            ;// some error throw

    }

    static
    rc_t DiscoverShell ( char *buffer, size_t bsize )
    {
        rc_t rc = 0;
        
        if ( buffer == NULL )
            rc = RC ( rcExe, rcNoTarg, rcValidating, rcParam, rcNull );
        else
        {
            const char *shell = getenv ( "SHELL" );
            if ( shell == NULL )
                rc = RC ( rcExe, rcNoTarg, rcValidating, rcParam, rcNotFound );
            else
            {
                size_t ssize = strlen ( shell );
                if ( bsize <= ssize )
                    rc = RC ( rcExe, rcNoTarg, rcValidating, rcBuffer, rcInsufficient );
                else
                {
                    size_t num_copied = string_copy ( buffer, bsize, shell, ssize );
                    if ( num_copied == bsize )
                        rc = RC ( rcExe, rcNoTarg, rcCopying, rcBuffer, rcInsufficient );                        
                }
            }
        }

        return rc;
    }

    static
    void WriteTimestamp ( FILE *f, const char *cat, KTime_t ts )
    {
        time_t time = ( time_t ) ts;

        struct tm tm = * gmtime ( &time );
        
        fprintf ( f, "%s %04d-%02d-%02dT%02d:%02d:%02dZ\n", cat
                  , tm . tm_year + 1900
                  , tm . tm_mon + 1
                  , tm . tm_mday
                  , tm . tm_hour
                  , tm . tm_min
                  , tm . tm_sec
            );
    }

    static 
    void WriteVersion ( FILE *f, const char *cat, ver_t v )
    {
        size_t num_writ;
        char buffer [ 256 ];
        string_printf ( buffer, sizeof buffer, & num_writ, "%s %V\n", cat, v );
        fwrite ( buffer, sizeof buffer [ 0 ], num_writ, f );
    }

    static 
    void WriteString ( FILE *f, const char *cat, const char *value )
    {
        fprintf ( f, "%s \"%s\"\n", cat, value );
    }

    static 
    void WriteBOOL ( FILE *f, const char *cat, bool value )
    {
        fprintf ( f, "%s %s\n", cat, value ? "true" : "false" );
    }

    static
    void SavePrefs ()
    {
        if ( dirty )
        {
            std :: string name = path_to_prefs;
            name += ".tmp";

            // open temporary prefs file
            FILE *prefs = fopen ( name . c_str (), "w" );
            if ( prefs != NULL )
            {
                WriteTimestamp ( prefs, "download-date", prefs_data . dl_date );
                WriteTimestamp ( prefs, "last-check-date", prefs_data . last_check_date );

                WriteVersion ( prefs, "latest-version", prefs_data . l_vers );
                WriteVersion ( prefs, "current-version", prefs_data . c_vers );

                WriteString ( prefs, "download-url", prefs_data . download_url . c_str () );
                WriteString ( prefs, "latest-vers-url", prefs_data . latest_vers_url . c_str () );
                WriteString ( prefs, "path-to-installation", prefs_data . path_to_installation . c_str () );

                WriteBOOL ( prefs, "auto-download-enabled", prefs_data . auto_download_enabled );

                // close the temporary prefs file
                fclose ( prefs );

                // rename the temporary file to the real prefs name
                rc_t rc = KDirectoryRename ( wd, true, "prefs.tmp", "prefs.prefs" );
                if ( rc != 0 )
                    throw Exception ( rc, __FILE__, __LINE__, "failed to rename file" );
            }
            dirty = false;
        }
    }

    static 
    rc_t LaunchShell ()
    {
        int lerrno, status;
        
        char buffer [ 256 ];
        rc_t rc = DiscoverShell ( buffer, sizeof buffer );
        if ( rc == 0 )
        {
            /* fork to child process */
            pid_t pid = fork ();
            switch ( pid )
            {
            case -1:
                rc = RC ( rcExe, rcNoTarg, rcValidating, rcNoObj, rcError );
                break;
            case 0:
                status = execl ( buffer, buffer, "-i",  NULL );
                
                /* having returned here, execl failed */
                lerrno = errno;
                printf("This print is after execl() and should not have been executed if execl were successful! \n\n");
                rc = RC ( rcExe, rcNoTarg, rcReading, rcNoObj, rcError );
                PLOGERR (klogErr,
                         (klogErr, rc, "unknown system error '$(S)($(E))'",
                          "S=%!,E=%d", lerrno, lerrno));
                
                /* MUST exit here to kill the forked child process */
                exit ( status );
            default:
                /* NOW - you need to wait until your child exits */
                pid = waitpid ( pid, & status, 0 );
                
                /* pid should be valid */
                if ( pid < 0 )
                    rc = RC ( rcExe, rcNoTarg, rcValidating, rcNoObj, rcError );
                else if ( status != 0 )
                {
                    /* you can display some info here */
                    rc = RC ( rcExe, rcNoTarg, rcValidating, rcNoObj, rcError );
                }
            }
        }

        return rc;
    }

    static
    rc_t RunShell ()
    {
        rc_t rc;
        rc = LaunchShell ( );
        return rc;
    }

    static
    void run ( int argc, char *argv[] )
    {
        // put the tool into a known state
        // all of the information needed to start is known
        Init ( argc, argv );

        try
        {
            // contact NCBI, locate installation
            if ( EnsureEverythingIsThere () )
            {
                UpdateEnvironment ();

                RunShell ();
            }
        }
        catch ( ... )
        {
            Cleanup ();
            throw;
        }

        Cleanup ();
    }
}

extern "C"
{

    void PrefsDataSetDownloadDate ( const Token  *download_date )
    {
        if ( download_date -> val . t >= 0 /* TBD - some master date */ 
             || download_date -> val . t >= workspace :: prefs_data . dl_date ) // >= very small chance double
                                                                     // download in < time than registered interval
            workspace :: prefs_data . dl_date = download_date -> val . t;
    }

    void PrefsDataSetLastCheckDate ( const Token *last_check_date )
    {
        if ( last_check_date -> val . t >= workspace :: prefs_data . last_check_date ) // again slight chance that the time interval 
                                                                            // between check dates wont register as different
            workspace :: prefs_data . last_check_date = last_check_date -> val . t;
    }

    void PrefsDataSetLatestVersion ( const Token *latest_version )
    {
        if ( latest_version -> val . v >= workspace :: prefs_data . l_vers )
             workspace :: prefs_data . l_vers = latest_version -> val . v;
    }

    void PrefsDataSetCurrentVersion ( const Token *current_version )
    {
        if ( current_version -> val . v >= workspace :: prefs_data . c_vers )
            workspace :: prefs_data . c_vers = current_version -> val . v;
    }

    void PrefsDataSetDownloadURL ( const Token *download_url )
    {
        if ( download_url -> val . c != NULL )
            workspace :: prefs_data . download_url = std :: string ( download_url -> val . c );
    }

    void PrefsDataSetLatestVersionURL ( const Token *latest_version_url )
    {
        if ( latest_version_url -> val .c != NULL )
            workspace :: prefs_data . latest_vers_url = std :: string ( latest_version_url -> val . c );
    }

    void PrefsDataSetPathToInstallation ( const Token *path_to_installation )
    {
        if ( path_to_installation -> val . c != NULL )
            workspace :: prefs_data . path_to_installation = std :: string ( path_to_installation -> val . c );
    }

    void PrefsDataSetAutoDownloadEnabled ( bool which )
    {
        workspace :: prefs_data . auto_download_enabled = which;
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
        
        return 0;
    }
    
    /* KMain
     *  C entrypoint
     *  call C++ function
     *  prevent any exceptions from leaking out
     */
    rc_t CC KMain ( int argc, char *argv [] )
    {
        rc_t rc = 0;

        try
        {
            workspace :: run ( argc, argv );
        }
        catch ( workspace :: Exception & x )
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
