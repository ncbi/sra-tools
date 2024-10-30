#pragma once

#include <memory>
#include <iostream>
#include <sys/wait.h>
#include <fcntl.h>
#include "values.hpp"
#include "fd_reader.hpp"

using namespace std;

namespace sra_convert {

class process;
typedef std::shared_ptr< process > process_ptr;
class process {
    private:
        char ** f_arg_ptr_list;
        KV_Map_Ptr f_values;
        string f_exe;
        list< string > f_args;
        list< string > f_processed_args; // we need this to keep the strings alive for f_arg_ptr_list
        string f_out;
        string f_err;
        int f_stdout_pipe[ 2 ];
        int f_stderr_pipe[ 2 ];
        bool f_silent;

        // >>>>> Ctor <<<<<
        process( KV_Map_Ptr values ) : f_arg_ptr_list( nullptr ), f_values( values ) {
            f_silent = true;
        }

        void show_arglist_ptr( void ) const {
            uint32_t idx = 0;
            char * a;
            do {
                a = f_arg_ptr_list[ idx ];
                if ( nullptr != a ) {
                    cout << "ARG[ " << idx << " ] = >" << a << "<\n";
                }
                idx++;
            } while ( nullptr != a );
        }

        void prepare_arg_ptr_list( void ) {
            uint32_t idx = 0;
            uint32_t arg_count = f_args . size() + 2; // +2 because 1st arg is executable, last arg is NULL
            f_arg_ptr_list = ( char ** )malloc( sizeof( char * ) * arg_count );
            f_arg_ptr_list[ idx++ ] = ( char * )f_exe . c_str();
            for ( const auto& arg : f_args ) {
                string s = StrTool::unquote( arg, '"', '\'' );
                f_processed_args . push_back( s );
                f_arg_ptr_list[ idx++ ] = ( char * )f_processed_args . back() . c_str();
            }
            f_arg_ptr_list[ idx ] = nullptr; // terminate the arg-ptr-list !
        }

        int execute( void ) {
            int res = 0;
            prepare_arg_ptr_list();
            // execvp takes over... this function only returns if execvp cannot execute
            int status_code = execvp( f_exe . c_str(), f_arg_ptr_list );
            if ( status_code != 0 ) { res = status_code; } // only executed if execvp() failed
            return res;
        }

        /* -------------------------------------------------------------------
         * outfile ... empty --> fd_to_null_reader    ( pipe -> NULL )
         * outfile ... "-"   --> fd_to_stdout_reader  ( pipe -> stdout )
         * outfile ... "--"  --> fd_to_stderr_reader  ( pipe -> stderr )
         * outfile ... ".:name" --> fd_to_md5_reader  ( pipe -> md5 -> value )
         * outfile ... ".name"  --> fd_to_md5_reader  ( pipe -> md5 -> file )
         * outfile ... ":name"  --> fd_to_file_reader ( pipe -> value )
         * outfile ... "name"   --> fd_to_file_reader ( pipe -> file )
        ------------------------------------------------------------------- */
        fd_reader_ptr make_reader( int fd, const string& outfile ) const {
            if ( outfile . empty() ) {
                return fd_to_null_reader::make( fd ); // pipe -> NULL
            } else if ( outfile . compare( "-" ) == 0 ) {
                return fd_to_stdout_reader::make( fd ); // pipe -> stdout
            } else if ( outfile . compare( "--" ) == 0 ) {
                return fd_to_stderr_reader::make( fd ); // pipe -> stderr
            } else if ( StrTool::startsWith( outfile, ".:" ) ) {
                return fd_to_md5_value_reader::make( fd, outfile . substr( 2 ), f_values ); // pipe -> md5 -> value
            } else if ( StrTool::startsWith( outfile, "." ) ) {
                return fd_to_md5_reader::make( fd, outfile . substr( 1 ) ); // pipe -> md5 -> file
            } else if ( StrTool::startsWith( outfile, ":" ) ) {
                return fd_to_value_reader::make( fd, outfile . substr( 1 ), f_values ); // pipe -> value
            } else {
                return fd_to_file_reader::make( fd, outfile ); // pipe -> file
            }
        }

        // this method runs in the parent process
        int wait_for_child_process( pid_t child_pid ) {
            int res = 0;

            close( f_stdout_pipe[ 1 ] );
            close( f_stderr_pipe[ 1 ] );
            fd_reader_ptr stdout_reader = make_reader( f_stdout_pipe[ 0 ], f_out );
            fd_reader_ptr stderr_reader = make_reader( f_stderr_pipe[ 0 ], f_err );

            // now we can read from stdout_pipe[ 0 ], and what comes out
            // is the stdout-stream from the child...
            int status;
            bool waiting = true;
            do {
                pid_t w = waitpid( child_pid, &status, WNOHANG );
                if ( w == -1 ) {
                    // something went wrong with the waitpid-call itself...
                    res = EXIT_FAILURE;
                    break;
                }

                if ( w == 0 ) {
                    stdout_reader -> read_loop();
                    stderr_reader -> read_loop();
                }

                if ( w == child_pid ) {
                    if ( WIFEXITED( status ) ) {
                        res = WEXITSTATUS( status );
                        //cout << "wait_for_child.exited = " << res << endl;
                        waiting = false;
                    } else if ( WIFSIGNALED( status )) {
                        //cout << "wait_for_child.killed, signal = " << WTERMSIG( status ) << endl;
                        waiting = false;
                    } else if ( WIFSTOPPED( status ) ) {
                        //cout << "wait_for_child.stopped, signal = " << WSTOPSIG( status ) << endl;
                    } else if ( WIFCONTINUED( status ) ) {
                        //cout << "wait_for_child.continued" << endl;
                    }
                }
            } while ( waiting );

            while( stdout_reader -> would_block() ) { stdout_reader -> read_loop(); }
            while( stderr_reader -> would_block() ) { stderr_reader -> read_loop(); }

            // we are done with them now
            close( f_stdout_pipe[ 0 ] );
            close( f_stderr_pipe[ 0 ] );

            return res;
        }

        // this method runs in the child process
        int run_child_process( void ) {
            // prepare to make the child write to stdout_pipe
            dup2( f_stdout_pipe[ 1 ], STDOUT_FILENO );
            close( f_stdout_pipe[ 0 ] );
            close( f_stdout_pipe[ 1 ] );

            // prepare to make the child write to stderr_pipe
            dup2( f_stderr_pipe[ 1 ], STDERR_FILENO );
            close( f_stderr_pipe[ 0 ] );
            close( f_stderr_pipe[ 1 ] );

            return execute(); // <<<< executing in the child-process
        }


    public:
        static process_ptr make( KV_Map_Ptr values ) { return process_ptr( new process( values ) ); }

        ~process( void ) {
            if ( f_arg_ptr_list != nullptr ) { free( f_arg_ptr_list ); }
        }

        void set_exe( const string_view& exe ) { f_exe = exe; }
        void set_stdout_file( const string_view& file_name ) { f_out = file_name; }
        void set_stderr_file( const string_view& file_name ) { f_err = file_name; }
        void set_silent( bool silent ) { f_silent = silent; }
        void set_args( const vector< string >& args ) {
            f_args . clear();
            for ( auto& item : args ) {
                if ( ! item . empty() ) {
                    f_args . push_back( item );
                }
            }
        }

        int run( void ) {
            int res = 0;
            if ( pipe( f_stdout_pipe ) == -1 ) { res = EXIT_FAILURE; }
            if ( ( 0 == res ) && pipe( f_stderr_pipe ) == -1 ) { res = EXIT_FAILURE; }
            if ( 0 == res ) {
                auto pid = fork();
                switch ( pid ) {
                    case -1 :
                        // for failed because?
                        res = EXIT_FAILURE;
                        break;

                    case 0 :
                        // we are in the child process now...
                        res = run_child_process();
                        break;

                    default:
                        // we are ( still ) in the parent process...
                        res = wait_for_child_process( pid );
                        break;
                }
            }
            return res;
        }

        friend auto operator<<( ostream& os, process_ptr o ) -> ostream& {
            os << "process:\n";
            os << "exe : >" << o -> f_exe << "<\n";
            for ( const auto& arg : o -> f_args ) {
                os << "arg : >" << arg << "<\n";
            }
            return os;
        }
};

}
