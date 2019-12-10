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
#include <sysexits.h>

#include "../../shared/toolkit.vers.h"
#include "cmdline.hpp"
#include "proc.hpp"
#include "run-source.hpp"
#include "debug.hpp"
#include "which.hpp"
#include "globals.hpp"

namespace sratools2
{
    struct Args
    {
        int const argc;
        char **const argv;
        char *const orig_argv0;

        Args ( int argc_, char * argv_ [], char * test_imp )
        : argc( argc_ )
        , argv( argv_ )
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
        
        char * add_string( const std::string &src )
        {
            size_t l = src.length();
            char * dst = ( char * )malloc( l + 1 );
            if ( dst != nullptr )
            {
                strncpy( dst, src . c_str(), l );
                dst[ l ] = 0;
            }
            return dst;
        }

        char * add_string( const ncbi::String &src )
        {
            return add_string( src.toSTLString() );
        }

        char ** generate_argv( int &argc, const std::string &argv0 )
        {
            argc = 0;
            auto cnt = options.size() + 2;
            char ** res = ( char ** )malloc( cnt * ( sizeof * res ) );
            if ( res != nullptr )
            {
                res[ argc++ ] = add_string( argv0 );
                for ( auto const &value : options )
                    res[ argc++ ] = add_string( value );
                res[ argc ] = nullptr;
            }
            return res;
        }

        char ** generate_argv( int &argc, std::vector< ncbi::String > const &args )
        {
            argc = 0;
            auto cnt = options.size() + args.size() + 1;
            char ** res = ( char ** )malloc( cnt * ( sizeof * res ) );
            if ( res != nullptr )
            {
                for ( auto const &value : options )
                    res[ argc++ ] = add_string( value );
                for ( auto const &value : args )
                    res[ argc++ ] = add_string( value );
                res[ argc ] = nullptr;
            }
            return res;
        }

        void free_argv( int argc, char ** argv )
        {
            if ( argv != nullptr )
            {
                for ( int i = 0; i < argc; ++i )
                {
                    if ( argv[ i ] != nullptr )
                        free( argv[ i ] );
                }
                free( argv );
            }
        }
    };

    enum class Imposter { SRAPATH, PREFETCH, FASTQ_DUMP, FASTERQ_DUMP, SRA_PILEUP, SAM_DUMP, INVALID };

    struct WhatImposter
    {
        public :
            const std::string _runpath;
            const std::string _basename;
            const std::string _requested_version;
            const std::string _toolkit_version;
            const Imposter _imposter;
            const bool _version_ok;

        private :
            std::string extract_path( const char * src )
            {
                std::string res( src );
                auto const t1 = res . find_last_of( '/' );
                res.resize(t1 == std::string::npos ? 0 : t1);
                return res;
            }

            std::string extract_basename( const char * src )
            {
                std::string res( src );
                auto const t1 = res . find_last_of( '/' );
                if ( t1 != std::string::npos )
                    res.assign( res . substr( t1 + 1 ) );
                auto const t2 = res . find_first_of( '.' );
                if ( t2 != std::string::npos )
                    res.assign( res . substr( 0, t2 ) ); 
                return res;
            }

            std::string extract_version( const char * src )
            {
                std::string res( src );
                auto const t1 = res . find_last_of( '/' );
                if ( t1 != std::string::npos )
                    res.assign( res . substr( t1 + 1 ) );
                auto const t2 = res . find_first_of( '.' );
                if ( t2 != std::string::npos )
                    res.assign( res . substr( t2 + 1 ) );
                else
                    res.clear();
                return res;
            }

            std::string get_toolkit_version( void )
            {
                auto const major = ((TOOLKIT_VERS) >> 24) & 0xFF;
                auto const minor = ((TOOLKIT_VERS)>> 16) & 0xFF;
                auto const subvers = ( TOOLKIT_VERS ) & 0xFFFF;
                return std::to_string(major) + '.' + std::to_string(minor) + '.' + std::to_string(subvers);
            }

            Imposter detect_imposter( const std::string &src )
            {
                if ( src.compare( "srapath" ) == 0 ) return Imposter::SRAPATH;
                else if ( src.compare( "prefetch" ) == 0 ) return Imposter::PREFETCH;
                else if ( src.compare( "fastq-dump" ) == 0 ) return Imposter::FASTQ_DUMP;
                else if ( src.compare( "fasterq-dump" ) == 0 ) return Imposter::FASTERQ_DUMP;
                else if ( src.compare( "sra-pileup" ) == 0 ) return Imposter::SRA_PILEUP;
                else if ( src.compare( "sam-dump" ) == 0 ) return Imposter::SAM_DUMP;
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
                if ( _requested_version.empty() ) return true;
                else if ( _requested_version.compare( _toolkit_version ) == 0 ) return true;
                return false;
            }

        public :
            WhatImposter( const char * argv0 )
                : _runpath( extract_path(argv0) )
                , _basename( extract_basename( argv0 ) )
                , _requested_version( extract_version( argv0 ) )
                , _toolkit_version( get_toolkit_version() )
                , _imposter( detect_imposter( _basename ) )
                , _version_ok( is_version_ok() )
            {
            }

            std::string const &effective_version() const {
                return _requested_version.empty() ? _toolkit_version : _requested_version;
            }

            std::string toolpath() const {
                return sratools::which(_runpath, _basename, _basename + "-orig", effective_version());
            }

            std::string as_string( void )
            {
                std::stringstream ss;
                ss << imposter_2_string( _imposter );
                ss << " _runpath:" << _runpath;
                ss << " _basename:" << _basename;
                ss << " _requested_version:" << _requested_version;
                ss << " _toolkit_version:" << _toolkit_version;
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

        static void print_vec( std::ostream &ss, std::vector < ncbi::String > const &v, std::string const &name )
        {
            if ( v.size() > 0 )
            {
                ss << name;
                int i = 0;
                for ( auto const &value : v )
                {
                    if ( i++ > 0 ) ss << ',';
                    ss << value;
                }
                ss << std::endl;
            }
        }

        static bool is_one_of( const ncbi::String &value, int count, ... )
        {
            bool res = false;
            int i = 0;
            va_list args;
            va_start( args, count );
            while ( !res && i++ < count )
            {
                ncbi::String s_item( va_arg( args, char * ) );
                res = value . equal( s_item );
            }
            va_end( args );
            return res;
        }

        static void print_unsafe_output_file_message(char const *const toolname, char const *const extension, std::vector<ncbi::String> const &accessions)
        {
            // since we know the user asked that tool output go to a file,
            // we can safely use cout to talk to the user.
            std::cout
            << toolname << " can not produce valid output from more than one\n"
            "run into a single file.\n"
            "The following output files will be created instead:\n";
            for (auto const &acc : accessions) {
                std::cout << "\t" << acc << extension << "\n";
            }
            std::cout << std::endl;
        }
    };

    struct CmnOptAndAccessions : OptionBase
    {
        WhatImposter const &what;
        std::vector < ncbi::String > accessions;
        ncbi::String ngc_file;
        ncbi::String perm_file;
        ncbi::String location;
        ncbi::String cart_file;
        bool disable_multithreading, version, quiet;
        std::vector < ncbi::String > debugFlags;
        ncbi::String log_level;
        ncbi::String option_file;

        CmnOptAndAccessions(WhatImposter const &what)
        : what(what)
        , disable_multithreading( false )
        , version( false )
        , quiet( false )
        {

        }
        
        void add( ncbi::Cmdline &cmdline ) override
        {
            cmdline . addParam ( accessions, 0, 256, "accessions(s)", "list of accessions to process" );
            cmdline . addOption ( ngc_file, nullptr, "", "ngc", "<path>", "<path> to ngc file" );
            cmdline . addOption ( perm_file, nullptr, "", "perm", "<path>", "<path> to permission file" );
            cmdline . addOption ( location, nullptr, "", "location", "<location>", "location in cloud" );
            cmdline . addOption ( cart_file, nullptr, "", "cart", "<path>", "<path> to cart file" );

            cmdline . addOption ( disable_multithreading, "", "disable-multithreading", "disable multithreading" );
            cmdline . addOption ( version, "V", "version", "Display the version of the program" );

            /*
            // problem: 'q' could be used by the tool already...
            cmdline . addOption ( quiet, "q", "quiet",
                "Turn off all status messages for the program. Negated by verbose." );
            */
#if _DEBUGGING || DEBUG
            cmdline . addListOption( debugFlags, ',', 255, "+", "debug", "<Module[-Flag]>",
                "Turn on debug output for module. All flags if not specified." );
#endif
            cmdline . addOption ( log_level, nullptr, "L", "log-level", "<level>",
                "Logging level as number or enum string. One of (fatal|sys|int|err|warn|info|debug) or "
                "(0-6) Current/default is warn" );
            cmdline . addOption ( option_file, nullptr, "", "option-file", "file",
                "Read more options and parameters from the file." );
        }

        std::ostream &show(std::ostream &ss) const override
        {
            for ( auto & value : accessions )
                ss << "acc  = " << value << std::endl;
            if ( !ngc_file.isEmpty() )  ss << "ngc-file : " << ngc_file << std::endl;
            if ( !perm_file.isEmpty() ) ss << "perm-file: " << perm_file << std::endl;
            if ( !location.isEmpty() )  ss << "location : " << location << std::endl;
            if ( disable_multithreading ) ss << "disable multithreading" << std::endl;
            if ( version ) ss << "version" << std::endl;
            print_vec( ss, debugFlags, "debug modules:" );
            if ( !log_level.isEmpty() ) ss << "log-level: " << log_level << std::endl;
            if ( !option_file.isEmpty() ) ss << "option-file: " << option_file << std::endl;
            return ss;
        }

        void populate_argv_builder( ArgvBuilder & builder, int acc_index, std::vector<ncbi::String> const &accessions ) const override
        {
            builder . add_option_list( "-+", debugFlags );
            if ( disable_multithreading ) builder . add_option( "--disable-multithreading" );
            if ( !log_level.isEmpty() ) builder . add_option( "-L", log_level );
            if ( !option_file.isEmpty() ) builder . add_option( "--option-file", option_file );
            if (!ngc_file.isEmpty()) builder.add_option("--ngc", ngc_file);

            (void)(acc_index); (void)(accessions);
        }
        
        bool check() const override
        {
            int problems = 0;
            if ( !log_level.isEmpty() )
            {
                if ( !is_one_of( log_level, 14,
                                 "fatal", "sys", "int", "err", "warn", "info", "debug",
                                 "0", "1", "2", "3", "4", "5", "6" ) )
                {
                    std::cerr << "invalid log-level: " << log_level << std::endl;
                    problems++;
                }
            }
            // we could check if ngc/kar/perm-files do actually exist...
            return ( problems == 0 );
        }
    };

    struct ToolExec {
    private:
        static std::vector<std::string> convert(std::vector<ncbi::String> const &other)
        {
            auto result = std::vector<std::string>();
            result.reserve(other.size());
            for (auto & s : other) {
                result.emplace_back(s.toSTLString());
            }
            return result;
        }
        static bool exec_wait(char const *toolpath, char const *toolname, char **argv, sratools::data_source const &src)
        {
            auto const child = sratools::process::run_child([&]() {
                src.set_environment();
                sratools::exec(toolpath, toolname, argv);
            });
            auto const result = child.wait();
            if (result.exited()) {
                if (result.exit_code() == 0) { // success, process next run
                    return true;
                }

                if (result.exit_code() == EX_TEMPFAIL)
                    return false; // try next source

                std::cerr << toolname << " (PID " << child.get_pid() << ") quit with error code " << result.exit_code() << std::endl;
                exit(result.exit_code());
            }

            if (result.signaled()) {
                std::cerr << toolname << " (PID " << child.get_pid() << ") was killed (signal " << result.termsig() << ")";
                std::cerr << std::endl;
                abort();
            }
            assert(!"reachable");
            abort();
        }
    public:
        static int run(char const *toolpath, char const *toolname, CmnOptAndAccessions const &tool_options, std::vector<ncbi::String> const &accessions)
        {
            auto const s_location = tool_options.location.toSTLString();
            auto const s_perm = tool_options.perm_file.toSTLString();
            auto const s_ngc = tool_options.ngc_file.toSTLString();

            sratools::location = s_location.empty() ? nullptr : &s_location;
            sratools::perm = s_perm.empty() ? nullptr : &s_perm;
            sratools::ngc = s_ngc.empty() ? nullptr : &s_ngc;

            // talk to SDL
            auto all_sources = sratools::data_sources::preload(convert(accessions));

            sratools::location = nullptr;
            sratools::perm = nullptr;
            sratools::ngc = nullptr;

            all_sources.set_ce_token_env_var();

            int i = 0;
            for (auto const &acc : accessions) {
                auto const &sources = all_sources.sourcesFor(acc.toSTLString());
                if (sources.empty())
                    continue; // data_sources::preload already complained

                ArgvBuilder builder;
                tool_options . populate_argv_builder( builder, i++, accessions );
                builder.add_option(acc.toSTLString());

                int argc = 0;
                char **argv = builder.generate_argv(argc, std::string(toolname));
                auto success = false;

                for (auto &src : sources) {
                    // run tool and wait for it to exit
                    success = exec_wait(toolpath, toolname, argv, src);
                    if (success) {
                        LOG(2) << "Processed " << acc << " with data from " << src.service() << std::endl;
                        break;
                    }
                    LOG(1) << "Failed to get data for " << acc << " from " << src.service() << std::endl;
                }

                builder.free_argv(argc, argv);

                if (!success) {
                    std::cerr << "Could not get any data for " << acc << ", tried to get data from:" << std::endl;
                    for (auto i : sources) {
                        std::cerr << '\t' << i.service() << std::endl;
                    }
                    std::cerr << "This may be temporary, you should retry later." << std::endl;
                    return EX_TEMPFAIL;
                }
            }
            return 0;
        }
    };

    struct ToolExecNoSDL {
        static int run(char const *toolpath, char const *toolname, OptionBase const &tool_options, std::vector<ncbi::String> const &accessions)
        {
            ArgvBuilder builder;

            builder.add_option(toolname);
            tool_options . populate_argv_builder( builder, (int)accessions.size(), accessions );
            int argc = 0;
            char **argv = builder.generate_argv(argc, accessions);

            sratools::exec(toolpath, toolname, argv);

            // exec returned! something went wrong
            auto const error = std::error_code(errno, std::system_category());

            builder.free_argv(argc, argv);

            throw std::system_error(error, std::string("Failed to exec ")+toolpath);
        }
    };

    int impersonate_fasterq_dump( Args const &args, WhatImposter const &what );
    int impersonate_fastq_dump( Args const &args, WhatImposter const &what );
    int impersonate_srapath( Args const &args, WhatImposter const &what );
    int impersonate_prefetch( Args const &args, WhatImposter const &what );
    int impersonate_sra_pileup( Args const &args, WhatImposter const &what );
    int impersonate_sam_dump( Args const &args, WhatImposter const &what );

    struct Impersonator
    {
    private:
        static inline void preparse(ncbi::Cmdline &cmdline) { cmdline.parse(true); }
        static inline void parse(ncbi::Cmdline &cmdline) { cmdline.parse(); }
    public:
        static int run(Args const &args, CmnOptAndAccessions &tool_options)
        {
            try {
                // Cmdline is a class defined in cmdline.hpp
                auto const version = tool_options.what.effective_version();
                ncbi::Cmdline cmdline(args.argc, args.argv, version.c_str());

                // let the parser parse the original args,
                // and let the parser handle help,
                // and let the parser write all values into cmn and params

                // add all the tool-specific options to the parser ( first )
                tool_options.add(cmdline);

                preparse(cmdline);
                parse(cmdline);

                // pre-check the options, after the input has been parsed!
                // give the tool-specific class an opportunity to check values
                if (!tool_options.check())
                    return EX_USAGE;

                if (tool_options.version) {
                    cmdline.version();
                    return 0;
                }
                return tool_options.run();
            }
            catch ( ncbi::Exception const &e )
            {
                // std::cerr << "An error occured: " << e.what() << std::endl;
                return EX_USAGE;
            }
            catch (std::exception const &e) {
                std::cerr << "An error occured: " << e.what() << std::endl;
                return EX_TEMPFAIL;
            }
            catch (...) {
                std::cerr << "An error occured" << std::endl;
                return EX_TEMPFAIL;
            }
        }
    };

} // namespace...
