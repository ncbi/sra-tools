/* ===========================================================================
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
#pragma once

#include <iostream>

#include <stdlib.h>
#include <string.h>
#include <sstream>
#include <cstdarg>
#include <assert.h>

#if WINDOWS
/// source: https://github.com/openbsd/src/blob/master/include/sysexits.h
#define EX_USAGE	64	/* command line usage error */
#define EX_NOINPUT	66	/* cannot open input */
#define EX_SOFTWARE	70	/* internal software error */
#define EX_TEMPFAIL	75	/* temp failure; user is invited to retry */
#define EX_CONFIG	78	/* configuration error */
#else
#include <sysexits.h>
#endif

#include "../../shared/toolkit.vers.h"
#include "cmdline.hpp"
#include "proc.hpp"
#include "run-source.hpp"
#include "debug.hpp"
#include "globals.hpp"
#include "service.hpp"
#include "tool-path.hpp"
#include "sratools.hpp"

namespace sratools2
{
    struct Args
    {
        int const argc;
        char const **const argv;
        char const *const orig_argv0;

        Args ( int argc_, char * argv_ [], char const * test_imp )
        : argc( argc_ )
        , argv( (char const **)(&argv_[0]))
        , orig_argv0( argv_[0] )
        {
            if (test_imp && test_imp[0]) {
                argv[0] = test_imp;
            }
        }

        void print( void )
        {
            std::cout << "main2() ( orig_argv0 = " << orig_argv0 << " )" << std::endl;
            for ( int i = 0; i < argc; ++i )
                std::cout << "argv[" << i << "] = " << argv[ i ] << std::endl;
        }
    };

    struct ArgvBuilder
    {
        std::vector < std::string > options;

        void add_option( const std::string &o ) { options.push_back( o ); }
        template < class T > void add_option( const std::string &o, const T &v )
        {
            options.push_back( o );
            std::stringstream ss;
            ss << v;
            options.push_back( ss.str() );
        }
        void add_option_list( const std::string &o, std::vector < ncbi::String > const &v )
        {
            for ( auto const &value : v )
            {
                options.push_back( o );
                options.push_back( value.toSTLString() );
            }
        }
        char ** generate_argv( int &argc, const std::string &argv0 );
        char const **generate_argv(std::vector< ncbi::String > const &args );
        void free_argv(char const **argv);
    private:
        char * add_string( const std::string &src );
        char * add_string( const ncbi::String &src );
    };

    enum class Imposter { SRAPATH, PREFETCH, FASTQ_DUMP, FASTERQ_DUMP, SRA_PILEUP, SAM_DUMP, VDB_DUMP, INVALID };

    struct WhatImposter
    {
        public :
            sratools::ToolPath const &toolpath;
            const Imposter _imposter;
            const bool _version_ok;

            struct InvalidVersionException : public std::exception {
                const char * what() const noexcept override {
                    return "Invalid tool version";
                }
            };
            struct InvalidToolException : public std::exception {
                const char * what() const noexcept override {
                    return "Invalid tool requested";
                }
            };

        private :

            Imposter detect_imposter( const std::string &source )
            {
#if WINDOWS
                const std::string ext = ".exe";
                const std::string src = ends_with(ext, source) ? source.substr( 0, source.size() - ext.size() ) : source;
#else
                const std::string & src = source;
#endif
                if ( src.compare( "srapath" ) == 0 ) return Imposter::SRAPATH;
                else if ( src.compare( "prefetch" ) == 0 ) return Imposter::PREFETCH;
                else if ( src.compare( "fastq-dump" ) == 0 ) return Imposter::FASTQ_DUMP;
                else if ( src.compare( "fasterq-dump" ) == 0 ) return Imposter::FASTERQ_DUMP;
                else if ( src.compare( "sra-pileup" ) == 0 ) return Imposter::SRA_PILEUP;
                else if ( src.compare( "sam-dump" ) == 0 ) return Imposter::SAM_DUMP;
                else if ( src.compare( "vdb-dump" ) == 0 ) return Imposter::VDB_DUMP;
                return Imposter::INVALID;
            }

            std::string imposter_2_string( const Imposter &value )
            {
                switch( value )
                {
                    case Imposter::INVALID : return "INVALID"; break;
                    case Imposter::SRAPATH : return "SRAPATH"; break;
                    case Imposter::PREFETCH : return "PREFETCH"; break;
                    case Imposter::FASTQ_DUMP : return "FASTQ_DUMP"; break;
                    case Imposter::FASTERQ_DUMP : return "FASTERQ_DUMP"; break;
                    case Imposter::SRA_PILEUP : return "SRA_PILEUP"; break;
                    case Imposter::SAM_DUMP : return "SAM_DUMP"; break;
                    default : return "UNKNOWN";
                }
            }

            bool is_version_ok( void )
            {
                return toolpath.version() == toolpath.toolkit_version();
            }

        public :
            WhatImposter( sratools::ToolPath const &toolpath )
            : toolpath(toolpath)
            , _imposter( detect_imposter( toolpath.basename() ) )
            , _version_ok( is_version_ok() )
            {
                if (!_version_ok)
                    throw InvalidVersionException();
                if (_imposter == Imposter::INVALID)
                    throw InvalidToolException();
            }

            std::string as_string( void )
            {
                std::stringstream ss;
                ss << imposter_2_string( _imposter );
                ss << " _runpath:" << toolpath.fullpath();
                ss << " _basename:" << toolpath.basename();
                ss << " _requested_version:" << toolpath.version();
                ss << " _toolkit_version:" << toolpath.toolkit_version();
                ss << " _version_ok: " << ( _version_ok ? "YES" : "NO" );
                return ss.str();
            }

            bool invalid( void )
            {
                return ( _imposter == Imposter::INVALID );
            }
            
            bool invalid_version( void )
            {
                return ( !_version_ok );
            }
    };

    struct CmnOptAndAccessions;

    struct OptionBase
    {
        virtual ~OptionBase() {}
        
        virtual std::ostream &show(std::ostream &os) const = 0;
        virtual void populate_argv_builder( ArgvBuilder & builder, int acc_index, std::vector<ncbi::String> const &accessions ) const = 0;
        virtual void add( ncbi::Cmdline &cmdline ) = 0;
        virtual bool check() const = 0;
        virtual int run() const = 0;

        static void print_vec( std::ostream &ss, std::vector < ncbi::String > const &v, std::string const &name );
        static bool is_one_of( const ncbi::String &value, int count, ... );
        static void print_unsafe_output_file_message(char const *const toolname, char const *const extension, std::vector<ncbi::String> const &accessions);
    };

    struct CmnOptAndAccessions : OptionBase
    {
        WhatImposter const &what;
        std::vector < ncbi::String > accessions;
        ncbi::String ngc_file;
        ncbi::String perm_file;
        ncbi::String location;
        ncbi::String cart_file;
        bool disable_multithreading, version, quiet, no_disable_mt;
        std::vector < ncbi::String > debugFlags;
        ncbi::String log_level;
        ncbi::String option_file;
        ncbi::U32 verbosity;

        CmnOptAndAccessions(WhatImposter const &what)
        : what(what)
        , disable_multithreading( false )
        , version( false )
        , quiet( false )
        , no_disable_mt(false)
        , verbosity(0)
        {
            switch (what._imposter) {
            case Imposter::FASTERQ_DUMP:
            case Imposter::PREFETCH:
            case Imposter::SRAPATH:
                no_disable_mt = true;
            default:
                break;
            }
        }

        virtual bool preferNoQual() const { return false; }
        
        void add( ncbi::Cmdline &cmdline ) override;

        std::ostream &show(std::ostream &ss) const override;

        enum VerbosityStyle {
            standard,
            fastq_dump
        };
        
        void populate_common_argv_builder( ArgvBuilder & builder, int acc_index, std::vector<ncbi::String> const &accessions, VerbosityStyle verbosityStyle = standard ) const;

        bool check() const override;
    };

    struct ToolExecNoSDL {
        static int run(char const *toolname, std::string const &toolpath, std::string const &theirpath, CmnOptAndAccessions const &tool_options, std::vector<ncbi::String> const &accessions);
    };

    struct ToolExec {
        static int run(char const *toolname, std::string const &toolpath, std::string const &theirpath, CmnOptAndAccessions const &tool_options, std::vector<ncbi::String> const &accessions);
    };

    // TODO: the impersonators are declared here.
    int impersonate_fasterq_dump( Args const &args, WhatImposter const &what );
    int impersonate_fastq_dump( Args const &args, WhatImposter const &what );
    int impersonate_srapath( Args const &args, WhatImposter const &what );
    int impersonate_prefetch( Args const &args, WhatImposter const &what );
    int impersonate_sra_pileup( Args const &args, WhatImposter const &what );
    int impersonate_sam_dump( Args const &args, WhatImposter const &what );
    int impersonate_vdb_dump( Args const &args, WhatImposter const &what );
    
    struct Impersonator
    {
    private:
        static inline void preparse(ncbi::Cmdline &cmdline) { cmdline.parse(true); }
        static inline void parse(ncbi::Cmdline &cmdline) { cmdline.parse(); }
    public:
        static int run(Args const &args, CmnOptAndAccessions &tool_options);
    };

} // namespace...
