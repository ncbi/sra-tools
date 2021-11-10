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
#include "support2.hpp"

#include <klib/status.h> /* KStsLevelSet */

namespace sratools2
{
    char * ArgvBuilder::add_string( const std::string &src )
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
    char * ArgvBuilder::add_string( const ncbi::String &src )
    {
        return add_string( src.toSTLString() );
    }
    char ** ArgvBuilder::generate_argv( int &argc, const std::string &argv0 )
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

    char const **ArgvBuilder::generate_argv(std::vector< ncbi::String > const &args )
    {
        auto argc = 0;
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
        return (char const **)res;
    }

    void ArgvBuilder::free_argv(char const **argv)
    {
        auto p = (void **)argv;
        while (*p) {
            free(*p++);
        }
        free((void *)argv);
    }

    void OptionBase::print_vec( std::ostream &ss, std::vector < ncbi::String > const &v, std::string const &name )
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

    bool OptionBase::is_one_of( const ncbi::String &value, int count, ... )
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

    void OptionBase::print_unsafe_output_file_message(char const *const toolname, char const *const extension, std::vector<ncbi::String> const &accessions)
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

    void CmnOptAndAccessions::add( ncbi::Cmdline &cmdline )
    {
        cmdline . addParam ( accessions, 0, 256, "accessions(s)", "list of accessions to process" );
        cmdline . addOption ( ngc_file, nullptr, "", "ngc", "<path>", "<path> to ngc file" );
        cmdline . addOption ( perm_file, nullptr, "", "perm", "<path>", "<path> to permission file" );
        cmdline . addOption ( location, nullptr, "", "location", "<location>", "location in cloud" );
        cmdline . addOption ( cart_file, nullptr, "", "cart", "<path>", "<path> to cart file" );

        if (!no_disable_mt)
            cmdline . addOption ( disable_multithreading, "", "disable-multithreading", "disable multithreading" );
        cmdline . addOption ( version, "V", "version", "Display the version of the program" );

        cmdline.addOption(verbosity, "v", "verbose", "Increase the verbosity of the program "
                          "status messages. Use multiple times for more verbosity.");
        /*
        // problem: 'q' could be used by the tool already...
        cmdline . addOption ( quiet, "q", "quiet",
            "Turn off all status messages for the program. Negated by verbose." );
        */
#if _DEBUGGING || DEBUG
        cmdline . addDebugOption( debugFlags, ',', 255,
            "+", "debug", "<Module[-Flag]>",
            "Turn on debug output for module. All flags if not specified." );
#endif
        cmdline . addOption ( log_level, nullptr, "L", "log-level", "<level>",
            "Logging level as number or enum string. One of (fatal|sys|int|err|warn|info|debug) or "
            "(0-6) Current/default is warn" );
        cmdline . addOption ( option_file, nullptr, "", "option-file", "file",
            "Read more options and parameters from the file." );
    }

    std::ostream &CmnOptAndAccessions::show(std::ostream &ss) const
    {
        for ( auto & value : accessions )
            ss << "acc  = " << value << std::endl;
        if ( !ngc_file.isEmpty() )  ss << "ngc-file : " << ngc_file << std::endl;
        if ( !perm_file.isEmpty() ) ss << "perm-file: " << perm_file << std::endl;
        if ( !location.isEmpty() )  ss << "location : " << location << std::endl;
        if ( disable_multithreading ) ss << "disable multithreading" << std::endl;
        if ( version ) ss << "version" << std::endl;
        if (verbosity) ss << "verbosity: " << verbosity << std::endl;
        print_vec( ss, debugFlags, "debug modules:" );
        if ( !log_level.isEmpty() ) ss << "log-level: " << log_level << std::endl;
        if ( !option_file.isEmpty() ) ss << "option-file: " << option_file << std::endl;
        return ss;
    }

    void CmnOptAndAccessions::populate_common_argv_builder( ArgvBuilder & builder, int acc_index, std::vector<ncbi::String> const &accessions, VerbosityStyle verbosityStyle ) const
    {
        builder . add_option_list( "-+", debugFlags );
        if ( disable_multithreading ) builder . add_option( "--disable-multithreading" );
        if ( !log_level.isEmpty() ) builder . add_option( "-L", log_level );
        if ( !option_file.isEmpty() ) builder . add_option( "--option-file", option_file );
        if (!ngc_file.isEmpty()) builder.add_option("--ngc", ngc_file);

        if (verbosity > 0) {
            switch (verbosityStyle) {
            case fastq_dump: /* fastq-dump can't handle -vvv, must repeat "-v" */
                for (ncbi::U32 i = 0; i < verbosity; ++i)
                    builder.add_option("-v");
                break;
            default:
                builder.add_option(std::string("-") + std::string(verbosity, 'v'));
                break;
            }
        }
        (void)(acc_index); (void)(accessions);
    }

    bool CmnOptAndAccessions::check() const
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

        if (!perm_file.isEmpty()) {
            if (!ngc_file.isEmpty()) {
                ++problems;
                std::cerr << "--perm and --ngc are mutually exclusive. Please use only one." << std::endl;
            }
            if (!pathExists(perm_file.toSTLString())) {
                ++problems;
                std::cerr << "--perm " << perm_file << "\nFile not found." << std::endl;
            }
            if (!vdb::Service::haveCloudProvider()) {
                ++problems;
                std::cerr << "Currently, --perm can only be used from inside a cloud computing environment.\nPlease run inside of a supported cloud computing environment, or get an ngc file from dbGaP and reissue the command with --ngc <ngc file> instead of --perm <perm file>." << std::endl;
            }
            else if (!sratools::config->canSendCEToken()) {
                ++problems;
                std::cerr << "--perm requires a cloud instance identity, please run vdb-config --interactive and enable the option to report cloud instance identity." << std::endl;
            }
        }
        if (!ngc_file.isEmpty()) {
            if (!pathExists(ngc_file.toSTLString())) {
                ++problems;
                std::cerr << "--ngc " << ngc_file << "\nFile not found." << std::endl;
            }
        }
        if (!cart_file.isEmpty()) {
            if (!pathExists(cart_file.toSTLString())) {
                ++problems;
                std::cerr << "--cart " << cart_file << "\nFile not found." << std::endl;
            }
        }

        auto containers = 0;
        for (auto & Acc : accessions) {
            auto const &acc = Acc.toSTLString();
            if (pathExists(acc)) continue; // skip check if it's a file system object

            auto const type = sratools::accessionType(acc);
            if (type == sratools::unknown || type == sratools::run)
                continue;

            ++problems;
            ++containers;

            std::cerr << acc << " is not a run accession. For more information, see https://www.ncbi.nlm.nih.gov/sra/?term=" << acc << std::endl;
        }
        if (containers > 0) {
            std::cerr << "Automatic expansion of container accessions is not currently available. See the above link(s) for information about the accessions." << std::endl;
        }
        if (problems == 0)
            return true;

        if (logging_state::is_dry_run()) {
            std::cerr << "Problems allowed for testing purposes!" << std::endl;
            return true;
        }
        return false;
    }

    int ToolExecNoSDL::run(char const *toolname, std::string const &toolpath, std::string const &theirpath, CmnOptAndAccessions const &tool_options, std::vector<ncbi::String> const &accessions)
    {
        ArgvBuilder builder;

#if WINDOWS
        // make sure we got all hard-coded POSIX path seperators
        assert(theirpath.find('/') == std::string::npos);
#endif
        builder.add_option(theirpath);
        tool_options . populate_argv_builder( builder, (int)accessions.size(), accessions );

        auto argv = builder.generate_argv(accessions);

        sratools::process::run_child(toolpath.c_str(), toolname, argv);

        // exec returned! something went wrong
        auto const error = std::error_code(errno, std::system_category());

        builder.free_argv(argv);

        throw std::system_error(error, std::string("Failed to exec ")+toolname);
    }

    static std::vector<std::string> convert(std::vector<ncbi::String> const &other)
    {
        auto result = std::vector<std::string>();
        result.reserve(other.size());
        for (auto & s : other) {
            result.emplace_back(s.toSTLString());
        }
        return result;
    }
    static bool exec_wait(char const *toolpath, char const *toolname, char const **argv, sratools::data_source const &src)
    {
        auto const result = sratools::process::run_child_and_wait(toolpath, toolname, argv, src.get_environment());
        if (result.exited()) {
            if (result.exit_code() == 0) { // success, process next run
                return true;
            }

            if (result.exit_code() == EX_TEMPFAIL)
                return false; // try next source

            std::cerr << toolname << " quit with error code " << result.exit_code() << std::endl;
            exit(result.exit_code());
        }

        if (result.signaled()) {
            auto const signame = result.termsigname();
            std::cerr << toolname << " was killed (signal " << result.termsig();
            if (signame) std::cerr << " " << signame;
            std::cerr << ")" << std::endl;
            exit(3);
        }
        assert(!"reachable");
        abort();
    }
    int ToolExec::run(char const *toolname, std::string const &toolpath, std::string const &theirpath, CmnOptAndAccessions const &tool_options, std::vector<ncbi::String> const &accessions)
    {
        static char const *const fullQualityName = "Normalized Format";
        static char const *const zeroQualityName = "Lite";
        static char const *const fullQualityDesc = "full base quality scores";
        static char const *const zeroQualityDesc = "simplified base quality scores";

        if (accessions.empty())
            return ToolExecNoSDL::run(toolname, toolpath, theirpath, tool_options, accessions);

        auto const verbose = tool_options.verbosity > 0;
        auto const s_location = tool_options.location.toSTLString();
        auto const s_perm = tool_options.perm_file.toSTLString();
        auto const s_ngc = tool_options.ngc_file.toSTLString();

        sratools::location = s_location.empty() ? nullptr : &s_location;
        sratools::perm = s_perm.empty() ? nullptr : &s_perm;
        sratools::ngc = s_ngc.empty() ? nullptr : &s_ngc;

        // set the verbosity of the program status messages
        KStsLevelSet(tool_options.verbosity);

        auto const qualityPreference = sratools::data_sources::qualityPreference();
        if (verbose && qualityPreference.isSet) {
            auto const name = qualityPreference.isFullQuality ? fullQualityName : zeroQualityName;
            auto const desc = qualityPreference.isFullQuality ? fullQualityDesc : zeroQualityDesc;
            std::cerr << "Preference setting is: Prefer SRA " << name << " files with " << desc << " if available.\n";
        }

        if (tool_options.preferNoQual()) {
            if (verbose && qualityPreference.isSet && qualityPreference.isFullQuality) {
                std::cerr << toolname << " requests temporary preference change: Prefer SRA " << zeroQualityName << " files with " << zeroQualityDesc << " if available.\n";
            }
            sratools::data_sources::preferNoQual();
        }

        // NOTE: this is where we ask SDL about the accessions and/or look
        // for them in the file system.
        auto const all_sources = sratools::data_sources::preload(convert(accessions));

        sratools::location = nullptr;
        sratools::perm = nullptr;
        sratools::ngc = nullptr;

        all_sources.set_ce_token_env_var();
#if WINDOWS
        // make sure we got all hard-coded POSIX path seperators
        assert(theirpath.find('/') == std::string::npos);
#endif

        int i = 0;
        for (auto const &acc : accessions) {
            auto const &sources = all_sources.sourcesFor(acc.toSTLString());
            if (sources.empty())
                continue; // data_sources::preload already complained

            ArgvBuilder builder;

            builder.add_option(theirpath);
            tool_options . populate_argv_builder( builder, i++, accessions );

            auto const argv = builder.generate_argv({ acc });
            auto success = false;

            for (auto &src : sources) {
                if (verbose && src.haveQualityType()) {
                    auto const name = src.haveFullQuality() ? fullQualityName : zeroQualityName;
                    auto const desc = src.haveFullQuality() ? fullQualityDesc : zeroQualityDesc;
                    std::cerr << acc << " is an SRA " << name << " file with " << desc << ".\n";
                }
                // run tool and wait for it to exit
                success = exec_wait(toolpath.c_str(), toolname, argv, src);
                if (success) {
                    LOG(2) << "Processed " << acc << " with data from " << src.service() << std::endl;
                    break;
                }
                LOG(1) << "Failed to get data for " << acc << " from " << src.service() << std::endl;
            }

            builder.free_argv(argv);

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
    int Impersonator::run(Args const &args, CmnOptAndAccessions &tool_options)
    {
        try {
            // Cmdline is a class defined in cmdline.hpp
            auto const version = tool_options.what.toolpath.version();
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
            std::cerr << e.what() << std::endl;
            return EX_USAGE;
        }
        catch (...) {
            throw;
        }
    }
} // namespace...
