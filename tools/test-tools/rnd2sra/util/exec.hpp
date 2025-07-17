#pragma once

#include <memory>
#include <cstdlib>
#include <string>
#include <map>
#include <list>
#include <stdexcept>
#include <iostream>
#include <chrono>

using namespace std;

namespace testing {

#if WINDOWS
#else

#include <signal.h>
#include <sys/wait.h>
#include <sysexits.h>
#include <unistd.h>

    #define CASE_SIGNAL( SIG ) case SIG: return "" # SIG

    typedef map< string, string > Dictionary;
    typedef list< string > Arguments;

    class Argv {
        private:
            const int f_argc;
            const char * f_argv[ 128 ];   // 128 should be enough...

        public :
            Argv( const string& toolpath, const Arguments& args ) : f_argc( args . size() + 1 ) {
                f_argv[ 0 ] = toolpath . c_str();
                int idx = 1;
                for ( auto& s : args ) {
                    if ( idx < 127 ) {
                        f_argv[ idx++ ] = s . c_str();
                    }
                }
                f_argv[ idx ] = nullptr;
            }

            int argc( void ) const { return f_argc; }
            char * const* args( void ) const { return (char * const*) &( f_argv[ 0 ] ); }
            const char* arg( int idx ) const {
                if ( idx < f_argc ) { return f_argv[ idx ]; }
                return nullptr;
            }

            friend auto operator<<( std::ostream& os, Argv const& obj ) -> std::ostream& {
                os << "argc = " << obj . f_argc << endl;
                int idx = 0;
                for ( int idx = 0; idx < obj . f_argc; idx++ ) {
                    os << "[ " << idx << " ] = " << obj . f_argv[ idx ] << endl;
                }
                return os;
            }
    };

    class ExitStatus {
        private:
            int32_t  f_value;
            uint64_t f_millisecs;

        public:
            bool didExit( void ) const { return WIFEXITED( f_value ) ? true : false; }
            int32_t exitCode( void ) const { return didExit() ? WEXITSTATUS( f_value ) : EXIT_FAILURE; }
            bool wasSignaled( void ) const { return WIFSIGNALED( f_value ) ? true : false; }
            int32_t signal( void ) const { return WTERMSIG( f_value ); }
            uint64_t millisecs( void ) const { return f_millisecs; }

            char const *signalName( void ) const {
                switch ( signal() ) {
                    CASE_SIGNAL( SIGHUP );
                    CASE_SIGNAL( SIGINT );
                    CASE_SIGNAL( SIGQUIT );
                    CASE_SIGNAL( SIGILL );
                    CASE_SIGNAL( SIGTRAP );
                    CASE_SIGNAL( SIGABRT );
                    CASE_SIGNAL( SIGBUS );
                    CASE_SIGNAL( SIGFPE );
                    CASE_SIGNAL( SIGKILL );
                    CASE_SIGNAL( SIGUSR1 );
                    CASE_SIGNAL( SIGSEGV );
                    CASE_SIGNAL( SIGUSR2 );
                    CASE_SIGNAL( SIGPIPE );
                    CASE_SIGNAL( SIGALRM );
                    CASE_SIGNAL( SIGTERM );
                    CASE_SIGNAL( SIGCHLD );
                    CASE_SIGNAL( SIGCONT );
                    CASE_SIGNAL( SIGSTOP );
                    CASE_SIGNAL( SIGTSTP );
                    CASE_SIGNAL( SIGTTIN );
                    CASE_SIGNAL( SIGTTOU );
                    CASE_SIGNAL( SIGURG );
                    CASE_SIGNAL( SIGXCPU );
                    CASE_SIGNAL( SIGXFSZ );
                    CASE_SIGNAL( SIGVTALRM );
                    CASE_SIGNAL( SIGPROF );
                    CASE_SIGNAL( SIGWINCH );
                    CASE_SIGNAL( SIGIO );
                    CASE_SIGNAL( SIGSYS );
                    default: return "unknown";
                }
            }

            bool didCoreDump( void ) const { return WCOREDUMP( f_value ) ? true : false; }

            bool isStopped( void ) const { return WIFSTOPPED( f_value ) ? true : false; }
            int32_t stopSignal( void ) const { return WSTOPSIG( f_value ); }

            ExitStatus( int32_t value ) : f_value( value ), f_millisecs( 0 ) {}
            ExitStatus( int32_t value, uint64_t millisecs ) : f_value( value ), f_millisecs( millisecs ) {}
            ExitStatus( ExitStatus const & ) = default;
            ExitStatus &operator =( ExitStatus const & ) = default;
            ExitStatus( ExitStatus && ) = default;
            ExitStatus &operator =( ExitStatus && ) = default;
    };

    class Proc;
    typedef std::shared_ptr< Proc > ProcPtr;
    class Proc {
        typedef chrono::steady_clock::time_point time_point;

        private :
            pid_t pid;

            uint64_t millisecs( const time_point& t_start, const time_point& t_end ) const {
                return chrono::duration_cast< chrono::milliseconds >( t_end - t_start ) . count();
            }

            ExitStatus wait( void ) const {
                time_point t_start = std::chrono::steady_clock::now();

                do {
                    // loop if wait is interrupted
                    int status = 0;
                    int options = 0;
                    const pid_t rc = waitpid( pid, &status, options );
                    if ( rc > 0 ) {
                        time_point t_end = std::chrono::steady_clock::now();
                        return ExitStatus( status, millisecs( t_start, t_end ) ); ///< normal return is here
                    }
                } while ( errno == EINTR );
                throw std::logic_error( "waitpid failed" );
            }

        public :

            static ExitStatus run(  const string& toolpath,
                                    char *const argv[],
                                    const Dictionary& env ) {
                auto const pid = fork();
                if ( pid < 0 ) {
                    throw std::logic_error( "fork failed" );
                }
                if ( pid == 0 ) {
                    for ( const auto& v : env ) {
                        setenv( v . first . c_str(), v . second . c_str(), 1 );
                    }
                    execv( toolpath . c_str(), argv );
                }
                return Proc( pid ) . wait();
            }

            static ExitStatus run(  const string& toolpath,
                                    char *const argv[] ) {
                auto const pid = fork();
                if ( pid < 0 ) {
                    throw std::logic_error( "fork failed" );
                }
                if ( pid == 0 ) {
                    execv( toolpath . c_str(), argv );
                }
                return Proc( pid ) . wait();
            }

            static ExitStatus run(  const string& toolpath,
                                    const Arguments& args ) {
                auto const pid = fork();
                if ( pid < 0 ) {
                    throw std::logic_error( "fork failed" );
                }
                if ( pid == 0 ) {
                    Argv argv( toolpath, args );
                    execv( toolpath . c_str(), argv . args() );
                }
                return Proc( pid ) . wait();
            }

            Proc( Proc const & ) = default;
            Proc &operator =( Proc const & ) = default;
            Proc( Proc && ) = default;
            Proc &operator =( Proc &&) = default;

            static Proc self() { return Proc( 0 ) ; }
            Proc( pid_t pid ) : pid( pid ) {}
    };

#endif // if WINDOWS

} // namespace testing
