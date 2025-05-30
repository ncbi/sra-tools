
#pragma once

#include <cstdint>
#include <cstdarg>
#include <memory>
#include <string>
#include <list>
#include <algorithm>
#include <iostream>
#include <sstream>

namespace kapp {

extern "C" {
    typedef uint32_t rc_t;
    typedef uint32_t ver_t;

    typedef struct Args Args;

    typedef void ( * WhackParamFnP ) (void * object);
    typedef rc_t ( * ConvertParamFnP ) (const Args * self, uint32_t arg_index, const char * arg,
                                        std::size_t arg_len, void ** result, WhackParamFnP * whack);

    typedef struct OptDef {
        const char *  name;           	/* UTF8/ASCII NUL terminated long name */
        const char *  aliases;        	/* UTF8/ASCII NUL terminated set of single utf8/ASCII character names: may be NULL or "" */
        void ( * help_gen) (const char **);	/* function to generate help string */
        const char ** help;                	/* help-gen can treat these as non-const */
        uint16_t      max_count;      	/* maximum allowed; 0 is unlimited */
        #define OPT_UNLIM 0
        bool          needs_value;    	/* does this require an argument value? */
        bool          required;             /* is this a required parameter?  Not supported yet. */
        ConvertParamFnP convert_fn;   /* function to convert option. can perform binary conversions. may be NULL */
    } OptDef;

    typedef struct ParamDef ParamDef;
    struct ParamDef {
        ConvertParamFnP convert_fn; /* function to convert option. can perform binary conversions. may be NULL */
    };

    extern OptDef StandardOptions [];

    #define ALIAS_DEBUG     "+"
    #define ALIAS_LOG_LEVEL "L"
    #define ALIAS_HELP      "h?"
    #define ALIAS_HELP1     "h"
    #define ALIAS_VERSION   "V"
    #define ALIAS_VERBOSE   "v"
    #define ALIAS_QUIET     "q"
    #if USE_OPTFILE
    #define ALIAS_OPTFILE   ""
    #endif

    #define OPTION_DEBUG     "debug"
    #define OPTION_LOG_LEVEL "log-level"
    #define OPTION_HELP      "help"
    #define OPTION_VERSION   "version"
    #define OPTION_VERBOSE   "verbose"
    #define OPTION_QUIET     "quiet"
    #if USE_OPTFILE
    #define OPTION_OPTFILE   "option-file"
    #endif
    #define OPTION_NO_USER_SETTINGS "no-user-settings"

    #define ALIAS_REPORT    ""
    #define OPTION_REPORT   "ncbi_error_report"

    /* Make
     *  create the empty object
     */
    rc_t ArgsMake ( Args ** pself );

    /* Whack
     *  undo all object and owned object construction
     */
    rc_t ArgsWhack ( Args * self );

    #ifndef ArgsRelease
    #define ArgsRelease(self) ArgsWhack(self)
    #endif

    rc_t ArgsAddOption ( Args * self, const OptDef * option );
    rc_t ArgsAddLongOption ( Args * self, const char * opt_short_names, const char * long_name,
                             const char * opt_param_names, const char * help_text, uint32_t max_count, bool required );

    rc_t ArgsAddParam ( Args * self, const ParamDef * param_def );
    rc_t ArgsAddLongParam ( Args * self, const char * param_name, const char * help_text, ConvertParamFnP opt_cvt );

    rc_t ArgsAddParamArray ( Args * self, const ParamDef * param, uint32_t count );
    rc_t ArgsAddStandardOptions ( Args * self );

    rc_t ArgsParse ( Args * self, int argc, const char *argv[] );

    rc_t Args_tokenize_file_into_argv( const char * filename, int * argc, char *** argv );
    rc_t Args_tokenize_file_and_progname_into_argv( const char * filename, const char * progname,
                                                    int * argc, char *** argv );

    void Args_free_token_argv( int argc, char * argv[] );
    rc_t Args_parse_inf_file( Args * args, const char * file_option );
    rc_t Args_find_option_in_argv( int argc, char * argv[],
                                      const char * option_name,
                                      char * option, std::size_t option_len );

    rc_t ArgsOptionCount( const Args * self, const char * option_name, uint32_t * count );
    rc_t ArgsOptionValue( const Args * self, const char * option_name,
                          uint32_t iteration, const void ** value );
    rc_t ArgsOptionValueExt( const Args * self, const char * option_name,
                             uint32_t iteration, const void ** value, bool * called_as_alias );

    rc_t ArgsParamCount( const Args * self, uint32_t * count );
    rc_t ArgsParamValue( const Args * self, uint32_t iteration, const void ** value );
    rc_t ArgsArgvValue( const Args * self, uint32_t iteration, const char ** value_string );

    rc_t ArgsMakeStandardOptions (Args** pself);
    rc_t ArgsHandleHelp( Args * self );
    rc_t ArgsHandleVersion( Args * self );
    rc_t ArgsHandleOptfile( Args * self );

    rc_t ArgsHandleLogLevel( const Args * self );
    rc_t ArgsHandleStatusLevel( const Args * self );
    rc_t ArgsHandleDebug( const Args * self );
    rc_t ArgsHandleStandardOptions( Args * self );

    rc_t ArgsMakeAndHandle( Args ** pself, int argc, const char ** argv, uint32_t table_count, ... );
    rc_t ArgsMakeAndHandle2( Args ** pself, int argc, const char ** argv,
                             ParamDef * params, uint32_t param_count, uint32_t table_count, ... );
    rc_t ArgsOptionSingleString( const Args * self, const char * option, const char ** value );
    rc_t ArgsProgram( const Args * args, const char ** fullpath, const char ** progname );
    rc_t ArgsCheckRequired( Args * args );

    rc_t UsageSummary( const char * prog_name );

    /*
     * A program should define this which will be used only of the actual
     * program name as called is somehow irretrievable
     */
    extern const char UsageDefaultName[];

    void HelpVersion( const char * fullpath, ver_t version );

    void HelpOptionLine( const char * alias, const char * option, const char * param, const char ** msgs );

    void HelpParamLine( const char * param, const char * const * msgs );

    void HelpOptionsStandard( void );

    rc_t MiniUsage( const Args * args );
    rc_t Usage( const Args * args );


    uint32_t ArgsGetGlobalTries (bool *isSet );
    bool Is32BitAndDisplayMessage( void );


    #define OPTION_APPEND_OUTPUT    "append_output"
    rc_t ArgsAddAppendModeOption( Args * self );
    rc_t ArgsHandleAppendMode( const Args * self );
    bool ArgsIsAppendModeSet( void );
    void ArgsAppendModeSet( bool AppendMode );
};

class DeclaredOption;
typedef std::shared_ptr< DeclaredOption > DeclaredOptionPtr;
class DeclaredOption {
    private :
        const std::string f_name;
        const std::string f_short_name;
        const std::string f_params;
        const std::string f_help;
        const uint32_t f_max_count;
        const bool f_required;
        const bool f_show_in_help;

        DeclaredOption( const char * name, const char * short_name,
                        const char * params, const char * help,
                        uint32_t max_count, bool required, bool show_in_help ) :
            f_name( nullptr != name ? name : "" ),
            f_short_name( nullptr != short_name ? short_name : "" ),
            f_params( nullptr != params ? params : "" ),
            f_help( nullptr != help ? help : "" ),
            f_max_count( max_count ),
            f_required( required ),
            f_show_in_help( show_in_help ) {}

    public :
        static DeclaredOptionPtr make( const char * name, const char * short_name,
                                       const char * params, const char * help,
                                       uint32_t max_count = 1, bool required = false,
                                       bool show_in_help = true ) {
            return DeclaredOptionPtr(
                new DeclaredOption( name, short_name, params, help, max_count, required, show_in_help ) );
        }

        rc_t add( Args * args ) {
            return ArgsAddLongOption( args, f_short_name.c_str(), f_name.c_str(), f_params.c_str(),
                                      f_help.c_str(), f_max_count, f_required );
        }

        const char * short_name( void ) const { return f_short_name.c_str(); }
        const char * name( void ) const { return f_name.c_str(); }
        const char * params( void ) const { return f_params.c_str(); }
        const char * help( void ) const { return f_help.c_str(); }
        uint32_t max_count( void ) const { return f_max_count; }
        bool required( void ) const { return f_required; }
        bool show_in_help( void ) const { return f_show_in_help; }
};

class ArgsParser;
typedef std::shared_ptr< ArgsParser > ArgsParserPtr;
class ArgsParser {
    private :
        Args * f_args;
        bool parsed;
        std::list< DeclaredOptionPtr > declared;

    protected :
        ArgsParser( void )
            : f_args( nullptr ), parsed( false ) { }

    public :
        ~ArgsParser() {
            if ( nullptr != f_args ) { ArgsWhack( f_args ); }
        }

        bool parse( int argc, char **argv, bool with_std_opt = true ) {
            if ( nullptr == f_args ) {
                rc_t rc = ArgsMake( &f_args );
                if ( 0 != rc ) { return false; }
                if ( with_std_opt ) {
                    rc = ArgsAddStandardOptions( f_args );
                    if ( 0 != rc ) {
                        ArgsWhack( f_args );
                        f_args = nullptr;
                        return false;
                    }
                }
            }
            if ( !parsed ) {
                for ( auto decl : declared ) { decl -> add( f_args ); }
                rc_t rc = ArgsParse( f_args, argc, argv );
                if ( 0 != rc ) { return false; }
                parsed = true;
            }
            return true;
        }

        void report( std::ostream& o ) const {
            uint32_t n = get_param_count();
            o << "param-count = " << n << std::endl;
            for ( uint32_t i = 0; i < n; ++i ) {
                auto s = get_param( i );
                o << "param[ " << i << " ] = " << s << std::endl;
            }
            for ( auto decl : declared ) {
                o << decl -> name() << "\t= " << get_option( decl -> name() ) << std::endl;
            }
        }

        void print_help( bool show_std_options = true ) {
            for ( auto decl : declared ) {
                if ( decl -> show_in_help() ) {
                    const char * msg[] = { decl->help(), nullptr };
                    HelpOptionLine( decl -> short_name(), decl -> name(), decl -> params(), msg );
                }
            }
            if ( show_std_options ) { HelpOptionsStandard(); }
        }

        /* ----- OPTIONS ------------------------------------------------------------- */

        void declare( const char * name, const char * short_name, const char * params,
                      const char * help, uint32_t max_count = 1, bool required = false,
                      bool show_in_help = true ) {
            auto d = DeclaredOption::make( name, short_name, params, help,
                                           max_count, required, show_in_help );
            declared . push_back( d );
        }

        uint32_t get_option_count( const char * name ) const {
            if ( nullptr == f_args ) { return 0; }
            uint32_t res = 0;
            bool b = ( 0 == ArgsOptionCount( f_args, name, &res ) );
            return ( b ) ? res : 0;
        }

        std::string get_option( const char * name, uint32_t idx = 0, std::string dflt = "" ) const {
            if ( nullptr == f_args ) { return dflt; }
            const void * value;
            bool b = ( 0 == ArgsOptionValue( f_args, name, idx, &value ) );
            return ( b ) ? std::string( ( const char * ) value ) : dflt;
        }

        template< typename T > T get( const char * name, uint32_t idx, T dflt ) const {
            std::string o = get_option( name, idx );
            if ( o . empty() ) { return dflt; }
            std::stringstream ss( o );
            T res;
            if ( !( ss >> res ) ) { res = dflt; }
            return res;
        }

        uint32_t get_u32( const char * name, uint32_t idx = 0, uint32_t dflt = 0 ) const {
            return get<uint32_t>( name, idx, dflt );
        }

        uint64_t get_u64( const char * name, uint32_t idx = 0, uint64_t dflt = 0 ) const {
            return get<uint64_t>( name, idx, dflt );
        }

        bool get_bool( const char * name ) const {
            return get_option_count( name ) > 0;
        }

        bool find( const char * name ) const { return ( get_option_count( name ) > 0 ); }

        /* ----- PARAMS -------------------------------------------------------------- */
        uint32_t get_param_count( void ) const {
            if ( nullptr == f_args ) { return 0; }
            uint32_t res = 0;
            bool b = ( 0 == ArgsParamCount( f_args, &res ) );
            return ( b ) ? res : 0;
        }

        std::string get_param( uint32_t idx, std::string dflt = "" ) const {
            if ( nullptr == f_args ) { return 0; }
            const void * value;
            bool b = ( 0 == ArgsParamValue( f_args, idx, &value ) );
            return ( b ) ? std::string( ( const char * ) value ) : dflt;
        }
};

} // namespace kapp
