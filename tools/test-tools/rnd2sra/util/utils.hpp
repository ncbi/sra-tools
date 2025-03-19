#pragma once

#include <string>
#include <vector>
#include <iostream>
#include <fstream>
#include <sstream>
#include <regex>
#include <optional>
#include <mutex>
#include <map>
#include <random>
#include <cstring>
#include <stdarg.h>

#include "subprocess.h"

#if linux
// ===== OS is LINUX =====
    #if __clang__
        // clang on linux ( because of clang LSP! )
        #if __clang_major__ <= 6
            #include <experimental/filesystem>
            namespace fs = std::experimental::filesystem;
        #else
            #include <filesystem>
            namespace fs = std::filesystem;
        #endif

    #else
        #if GCC_VERSION <= 7
            #if __GNUC__ <= 7
                #include <experimental/filesystem>
                namespace fs = std::experimental::filesystem;
            #else
                #include <filesystem>
                namespace fs = std::filesystem;
            #endif

        #else
            #include <filesystem>
            namespace fs = std::filesystem;
        #endif
    #endif

#else
    // ===== OS is MAC or WINDOWS =====
    #include <filesystem>
    namespace fs = std::filesystem;
#endif


namespace util {

class FileTool {

    public :

        static std::string location( const std::string& path ) {
            if ( ! path . empty() ) {
                fs::path fs_path{ path };
                fs::path parent_path{ fs_path . parent_path() };
                if ( parent_path . empty() ) return std::string();
                if ( !exists( parent_path . string() ) ) return std::string();
                fs::path abs_parent_path{ fs::absolute( parent_path ) };
                return fs::canonical( abs_parent_path ).string();
            }
            return std::string();
        }

        static bool exists( const std::string& path ) {
            bool res = ! path . empty();
            if ( res ) {
                fs::path fs_path{ path };
                res = fs::exists( fs_path );
            }
            return res;
        }

        static bool make_dir_if_not_exists( const std::string& path ) {
            bool res = exists( path );
            if ( !res ) {
                std::error_code ec;
                res = fs::create_directories( path, ec );
            }
            return res;
        }

        static std::string remove_traling_separator( const std::string& path ) {
            auto res{ path };
            while ( ( ! path . empty() ) &&
                    ( ( res . back() == '/') || ( res.back() == '\\' ) ) ) {
                    res . erase( res . size() - 1 );
            }
            return res;
        }

        static std::string current_dir( void ) {
            return fs::current_path().string();
        }
};

class FileRename {
    private :
        static bool move_file_to_file( const std::string& src,
                                       const std::string& dst,
                                       bool ignore_err ) {
            bool res = true;
            try {
                fs::rename( src, dst );
            } catch ( fs::filesystem_error& e ) {
                if ( !ignore_err ) {
                    std::cerr << e.what() << '\n';
                    res = false;
                }
            }
            return res;
        }

        static bool move_file_patterns( std::vector< std::string >& args,
                                        bool ignore_err )  {
            bool res = true;
            size_t idx = 0;
            size_t count = args . size();
            std::string& src_head = args[ idx++ ];
            std::string& dst_head = args[ idx++ ];
            std::string& ext = args[ idx++ ];
            while ( ( res ) && ( idx < count ) ) {
                std::string& tail = args[ idx++ ];
                std::string src = src_head + tail + ext;
                std::string dst = dst_head + tail + ext;
                res = move_file_to_file( src, dst, ignore_err );
            }
            return res;
        }

    public :
        static bool move_files( std::vector< std::string >& args,
                                bool ignore_err ) {
            bool res = false;
            switch( args . size() ) {
                case 0  : if ( ignore_err ) { res = true; } break;
                case 1  : if ( ignore_err ) { res = true; } break;
                case 2  : res = move_file_to_file( args[ 0 ], args[ 1 ], ignore_err ); break;
                default : res = move_file_patterns( args, ignore_err ); break;
            }
            return res;
        }
};

class FileDiff {

    private :
        static bool diff_file_to_file( const std::string& file1,
                                       const std::string& file2,
                                       bool ignore_err ) {
            std::ifstream f1( file1, std::ifstream::binary|std::ifstream::ate );
            std::ifstream f2( file2, std::ifstream::binary|std::ifstream::ate );

            // if both files cannot be opened AND we want to ignore that --> SUCCESS
            if ( f1 . fail() && f2 . fail() && ignore_err ) { return true; }

            // if one or both of the files cannot be opened --> FAILURE
            if ( f1 . fail() || f2 . fail() ) { return false; }

            // if the sizes do not match --> FAILURE
            if ( f1 . tellg() != f2 . tellg() ) { return false; }

            // seek back to beginning
            f1 . seekg( 0, std::ifstream::beg );
            f2 . seekg( 0, std::ifstream::beg );

            // let std::equal do the comparison ...
            return std::equal( std::istreambuf_iterator<char>( f1 . rdbuf() ),
                               std::istreambuf_iterator<char>(),
                               std::istreambuf_iterator<char>( f2 . rdbuf() ) );
        }

        static bool diff_patterns( std::vector< std::string >& args,
                                   bool ignore_err ) {
            bool res = true;
            size_t idx = 0;
            size_t count = args . size();
            std::string& src_head = args[ idx++ ];
            std::string& dst_head = args[ idx++ ];
            std::string& ext = args[ idx++ ];
            while ( ( res ) && ( idx < count ) ) {
                std::string& tail = args[ idx++ ];
                std::string src = src_head + tail + ext;
                std::string dst = dst_head + tail + ext;
                res = diff_file_to_file( src, dst, ignore_err );
            }
            return res;
        }

    public :

        static bool diff( std::vector< std::string >& args,
                          bool ignore_err )  {
            bool res = false;
            switch( args . size() ) {
                case 0 : if ( ignore_err ) { res = true; } break;
                case 1 : if ( ignore_err ) { res = true; } break;
                case 2 : res = diff_file_to_file( args[ 0 ], args[ 1 ], ignore_err ); break;
                default : res = diff_patterns( args, ignore_err ); break;
            }
            return res;
        }
};

/* ---- filters for file-iterators ----------------------------------------------- */

// abstract base class for the filters
class entry_filter;
typedef std::shared_ptr< entry_filter > entry_filter_ptr;
class entry_filter {
    public :
        virtual ~entry_filter() = default;
        virtual bool ok( const fs::directory_entry& e ) { return false; };
        virtual bool valid( void ) const { return true; }

        // >>>>> public Factory <<<<<
        static entry_filter_ptr make( void ) {
            return entry_filter_ptr( new entry_filter() );
        }
};

// a simple filter: lets only regular files pass ( no directories etc. )
class file_entry_filter : public entry_filter {
    public :
        virtual ~file_entry_filter() = default;

        virtual bool ok( const fs::directory_entry& e ) override {
            return e . is_regular_file();
        }

        // >>>>> public Factory <<<<<
        static entry_filter_ptr make( void ) {
            return entry_filter_ptr( new file_entry_filter() );
        }
};

// a simple filter: lets only directories pass ( no files etc. )
class dir_entry_filter : public entry_filter {
    public :
        virtual ~dir_entry_filter() = default;

        bool ok( const fs::directory_entry& e ) override {
            return e . is_directory();
        }

        // >>>>> public Factory <<<<<
        static entry_filter_ptr make( void ) {
            return entry_filter_ptr( new dir_entry_filter() );
        }
};

class count_filter : public entry_filter {
    private :
        size_t f_max;
        size_t f_cur;
        count_filter( size_t max ) : f_max{ max }, f_cur{ 0 } {}

    public :
        virtual ~count_filter() = default;

        bool ok( const fs::directory_entry& e ) override {
            return ( f_cur++ < f_max );
        }

        // >>>>> public Factory <<<<<
        static entry_filter_ptr make( size_t max ) {
            return entry_filter_ptr( new count_filter( max ) );
        }
};

// exclude entries ( that are ending in the given string )
class ending_filter : public entry_filter {
    private :
        std::vector< std::string > f_endings;
        bool f_include;

        static bool ends_in( std::string const &s, std::string const &e ) {
            if ( e . size() > s . size() ) return false;
            return std::equal( e . rbegin(), e . rend(), s . rbegin() );
        }

    public :
        virtual ~ending_filter() = default;

        virtual bool ok( const fs::directory_entry& e ) override {
            for ( auto& item : f_endings ) {
                std::string p = e . path() . string();
                if ( ends_in( p, item ) ) { return f_include; }
            }
            return !f_include;
        }

        void add( const std::string& s ) { f_endings . push_back( s ); }
        bool valid( void ) const override { return !f_endings . empty(); }

        // >>>>> public Factory <<<<<
        static entry_filter_ptr make( const std::string& s, bool include = true ) {
            auto filter = new ending_filter();
            filter -> add( s );
            filter -> f_include = include;
            return entry_filter_ptr( filter );
        }
};

class glob_to_regex {
    private:
        const std::string f_pattern;
        const std::string_view SPECIAL_CHARACTERS = std::string_view{ "()[]{}?*+-|^$\\.&~# \t\n\r\v\f" };
        const std::regex ESCAPE_SET_OPER = std::regex( std::string{R"([&~|])" } );
        const std::string ESCAPE_REPL_STR = std::string{ R"(\\\1)" };

        static bool string_replace( std::string &str,
                                    std::string_view from,
                                    std::string_view into ) {
            std::size_t start_pos = str . find( from );
            if ( start_pos == std::string::npos ) { return false; }
            str . replace( start_pos, from . length(), into );
            return true;
        }

    public :
        std::string translate( std::string_view pattern ) {
            size_t i = 0;
            size_t n = pattern . size();
            std::string result_string;

            while ( i < n ) {
                auto c = pattern[ i ];
                i += 1;
                if ( c == '*' ) {
                    result_string += ".*";
                } else if ( c == '?' ) {
                    result_string += ".";
                } else if ( c == '[' ) {
                    auto j = i;
                    if ( j < n && pattern[ j ] == '!' ) { j += 1; }
                    if ( j < n && pattern[ j ] == ']') { j += 1; }
                    while ( j < n && pattern[ j ] != ']') { j += 1; }
                    if ( j >= n ) {
                        result_string += "\\[";
                    } else {
                        auto stuff = std::string( pattern . begin() + i, pattern . begin() + j );
                        if ( stuff . find( "--" ) == std::string::npos ) {
                            string_replace( stuff, std::string_view{ "\\" }, std::string_view{ R"(\\)" } );
                        } else {
                            std::vector< std::string > chunks;
                            size_t k = 0;
                            if ( pattern[ i ] == '!' ) {
                                k = i + 2;
                            } else {
                                k = i + 1;
                            }
                            while ( true ) {
                                k = pattern . find( "-", k, j );
                                if ( k == std::string_view::npos ) { break; }
                                chunks . push_back( std::string( pattern . begin() + i, pattern . begin() + k ) );
                                i = k + 1;
                                k = k + 3;
                            }
                            chunks . push_back( std::string( pattern . begin() + i, pattern . begin() + j ) );
                            // Escape backslashes and hyphens for set difference (--).
                            // Hyphens that create ranges shouldn't be escaped.
                            bool first = true;
                            for ( auto &chunk : chunks ) {
                                string_replace( chunk, std::string_view{ "\\" }, std::string_view{ R"(\\)" } );
                                string_replace( chunk, std::string_view{ "-" }, std::string_view{ R"(\-)" } );
                                if ( first ) {
                                    stuff += chunk;
                                    first = false;
                                } else {
                                    stuff += "-" + chunk;
                                }
                            } // for ( auto &chunk : chunks )
                        }
                        // Escape set operations (&&, ~~ and ||).
                        std::string result{};
                        std::regex_replace( std::back_inserter( result ), // result
                                            stuff . begin(), stuff . end(), // string
                                            ESCAPE_SET_OPER,  // pattern
                                            ESCAPE_REPL_STR); // repl
                        stuff = result;
                        i = j + 1;
                        if ( stuff[ 0 ] == '!' ) {
                            stuff = "^" + std::string( stuff . begin() + 1, stuff . end() );
                        } else if (stuff[ 0 ] == '^' || stuff[ 0 ] == '[' ) {
                            stuff = "\\\\" + stuff;
                        }
                        result_string = result_string + "[" + stuff + "]";
                    } // if ( j >= n )
                } else {
                    // SPECIAL_CHARS
                    // closing ')', '}' and ']'
                    // '-' (a range in character set)
                    // '&', '~', (extended character set operations)
                    // '#' (comment) and WHITESPACE (ignored) in verbose mode
                    static std::map< int, std::string > special_characters_map;
                    if ( special_characters_map . empty() ) {
                        for ( auto &&sc : SPECIAL_CHARACTERS ) {
                            special_characters_map . emplace( static_cast< int >( sc ),
                                std::string{ "\\" } + std::string( 1, sc ) );
                        }
                    }
                    if ( SPECIAL_CHARACTERS . find( c ) != std::string_view::npos ) {
                        result_string += special_characters_map[ static_cast< int >( c ) ];
                    } else {
                        result_string += c;
                    }
                }
            }
            return std::string{ "((" } + result_string + std::string{ R"()|[\r\n])$)" };
        }
};

// a regex-based filter: lets only entries passt tha match the regex
// used to simulate globbing ( which is posix-only! )
typedef std::shared_ptr< std::regex > regex_ptr;
class regex_filter : public entry_filter {
    private:
        const regex_ptr f_regex;

        // >>>>> private Ctor <<<<<
        regex_filter( const regex_ptr regex ) : f_regex{ regex } {}

    public :
        virtual ~regex_filter() = default;

        virtual bool ok( const fs::directory_entry& e ) override {
            bool res = false;
            if ( nullptr != f_regex . get() ) {
                try {
                    res = std::regex_match( e . path() . string(), *( f_regex . get() ) );
                } catch ( const std::regex_error& e ) {
                    std::cerr << "regex match error: " << e . what() << std::endl;
                }
            }
            return res;
        }

        // >>>>> public Factory <<<<<
        static entry_filter_ptr make( const std::string pattern ) {
            try {
                regex_ptr rp = regex_ptr( new std::regex( pattern ) );
                return entry_filter_ptr( new regex_filter( rp ) );
            } catch ( const std::regex_error& e ) {
                std::cerr << "regex create error in : " << e . what() << std::endl;
            }
            return entry_filter::make();
        }
};

class glob_filter : public entry_filter {
    private:
        entry_filter_ptr f_filter;

        // >>>>> private Ctor <<<<<
        glob_filter( entry_filter_ptr filter ) : f_filter{ filter } {}

    public :
        virtual ~glob_filter() = default;

        bool ok( const fs::directory_entry& e ) override {
            return f_filter -> ok( e );
        }

        bool valid( void ) const override { return f_filter -> valid(); }

        // >>>>> public Factory <<<<<
        static entry_filter_ptr make( const std::string& pattern ) {
            std::string regex_pattern = glob_to_regex().translate( pattern );
            auto filter = regex_filter::make( regex_pattern );
            return entry_filter_ptr( new glob_filter( filter ) );
        }
};

/* ---- file-iterators ----------------------------------------------------------- */

// abstract base class for the file-iters
class fs_iter;
typedef std::shared_ptr< fs_iter > fs_iter_ptr;
class fs_iter {
    public :
        virtual ~fs_iter() = default;

        virtual std::optional< fs::directory_entry > next( void ) = 0;

        /* helpers to fill/produce a string-vector */
        void fill_list( std::vector< std::string >& lst ) {
            while( auto entry = next() ) {
                lst . push_back( entry -> path() . string() );
            }
        }

        std::vector< std::string > to_list( void ) {
            std::vector< std::string > lst;
            fill_list( lst );
            return lst;
        }

        void for_each( void( *callback )( const fs::directory_entry& e ) ) {
            if ( nullptr != callback ) {
                while( auto entry = next() ) {
                    callback( entry . value() );
                }
            }
        }

        void for_each( void( *callback )( const fs::path& p ) ) {
            if ( nullptr != callback ) {
                while( auto entry = next() ) {
                    callback( entry . value() . path() );
                }
            }
        }

        void for_each( void( *callback )( const std::string& s ) ) {
            if ( nullptr != callback ) {
                while( auto entry = next() ) {
                    callback( entry . value() . path() . string() );
                }
            }
        }

        void for_each( void( *callback )( const fs::directory_entry& e, uint32_t idx ) ) {
            if ( nullptr != callback ) {
                uint32_t idx = 0;
                while( auto entry = next() ) {
                    callback( entry . value(), idx++ );
                }
            }
        }

        void for_each( void( *callback )( const fs::path& p, uint32_t idx ) ) {
            if ( nullptr != callback ) {
                uint32_t idx = 0;
                while( auto entry = next() ) {
                    callback( entry . value() . path(), idx++ );
                }
            }
        }

        void for_each( void( *callback )( const std::string& s, uint32_t idx ) ) {
            if ( nullptr != callback ) {
                uint32_t idx = 0;
                while( auto entry = next() ) {
                    callback( entry . value() . path() . string(), idx++ );
                }
            }
        }
};

// wrap any file-iter into a range to make it possible to use them
// in a for-loop
class file_iter_range {
    private :
        fs_iter_ptr f_iter;
        std::optional< fs::directory_entry > f_item;

    public :
        // Ctor
        file_iter_range( fs_iter_ptr iter ) : f_iter( iter ) { f_item = f_iter -> next(); }

        // 5 methods that delegate to the wrapped virtual iterator for range-based for-loops
        auto begin() { return *this; }
        auto end() { return *this; }
        std::optional< fs::directory_entry > operator*() { return f_item; }
        auto& operator++() { f_item = f_iter -> next(); return *this; }
        bool operator!=( const file_iter_range &rhs )  { return f_item . has_value(); }
};

// combine a given file_iter and a filter to create a new filtered file_iter
// makes it possible to chain them together
class filter_iter : public fs_iter {
    private :
        fs_iter_ptr f_it;
        entry_filter_ptr f_filter;

        // >>>>> private Ctor <<<<<
        filter_iter( fs_iter_ptr it, entry_filter_ptr filter )
            : f_it( it ), f_filter( filter ) { }

    public :

        virtual ~filter_iter() = default;

        std::optional< fs::directory_entry > next( void ) override {
            while ( true ) {
                auto item = f_it -> next();
                if ( item . has_value() ) {
                    if ( f_filter -> ok( item . value() ) ) {
                        return item;
                    }
                } else {
                    break;
                }
            }
            return {};
        };

        // >>>>> public Factory <<<<<
        static fs_iter_ptr make( fs_iter_ptr it, entry_filter_ptr filter ) {
            auto p1 = new filter_iter( it, filter );
            auto p2 = dynamic_cast< fs_iter* >( p1 );
            return fs_iter_ptr( p2 );
        }
};

// interface for dir_iter or rec_dir_iter ( aka recursive or not )
// because fs::directory_iterator and fs::recursive_directory_iterator
// do not have a common ancestor!
class any_dir_iter;
typedef std::shared_ptr< any_dir_iter > any_dir_iter_ptr;
class any_dir_iter {
    public :
        virtual std::optional< fs::directory_entry > next( void ) = 0;
};

class flat_dir_iter : public any_dir_iter {
    private :
        fs::directory_iterator f_end;
        std::optional< fs::directory_iterator > f_iter;
        bool f_valid;

        // >>>>> private Ctor <<<<<
        flat_dir_iter( const fs::path& loc ) : f_end() {
            try {
                f_iter = fs::directory_iterator( loc );
                f_valid = true;
            } catch ( const fs::filesystem_error& e ) {
                f_iter = {};
                f_valid = false;
            }
        }

    public :
        virtual ~flat_dir_iter() = default;

        std::optional< fs::directory_entry > next( void ) override {
            if ( f_valid && f_iter . has_value() ) {
                auto res = *( f_iter . value()++ );
                // we have to make sure: f_iter is not advanced beyond f_end!
                f_valid = ( f_iter . value() != f_end );
                return res;
            }
            return {};
        }

        // >>>>> public Factory <<<<<
        static any_dir_iter_ptr make( const fs::path& loc ) {
            return any_dir_iter_ptr( new flat_dir_iter( loc ) );
        }
};

class rec_dir_iter : public any_dir_iter {
    private :
        fs::recursive_directory_iterator f_end;
        std::optional< fs::recursive_directory_iterator > f_iter;
        bool f_valid;

        // >>>>> private Ctor <<<<<
        rec_dir_iter( const fs::path& loc ) : f_end() {
            try {
                f_iter = fs::recursive_directory_iterator( loc );
                f_valid = true;
            } catch ( const fs::filesystem_error& e ) {
                f_iter = {};
                f_valid = false;
            }
        }

    public :
        virtual ~rec_dir_iter() = default;

        std::optional< fs::directory_entry > next( void ) override {
            if ( f_valid && f_iter . has_value() ) {
                auto res = *( f_iter . value()++ );
                // we have to make sure: f_iter is not advanced beyond f_end!
                f_valid = ( f_iter . value() != f_end );
                return res;
            }
            return {};
        }

        // >>>>> public Factory <<<<<
        static any_dir_iter_ptr make( const fs::path& loc ) {
            return any_dir_iter_ptr( new rec_dir_iter( loc ) );
        }
};

class dir_iter : public fs_iter {
    private:
        any_dir_iter_ptr f_iter;

        // >>>>> private Ctor <<<<<
        dir_iter( const fs::path& loc, bool recursive ) : fs_iter() {
            if ( recursive ) {
                f_iter = rec_dir_iter::make( loc );
            } else {
                f_iter = flat_dir_iter::make( loc );
            }
        }

    public :
        virtual ~dir_iter() = default;

        std::optional< fs::directory_entry > next( void ) override {
            return f_iter -> next();
        }

        // >>>>> public Factory <<<<<
        static fs_iter_ptr make( const fs::path& loc, bool recursive = false ) {
            return fs_iter_ptr( new dir_iter( loc, recursive ) );
        }

        static fs_iter_ptr make_globed( const std::string& loc, bool recursive = false ) {
            fs::path loc_path{ loc };
            fs::path filename = loc_path . filename();
            if ( filename . empty() ) {
                filename = ".*";
            }
            fs::path parent = loc_path . parent_path();
            if ( parent . empty() ) {
                parent = fs::current_path();
            }
            fs_iter_ptr files = make( parent, recursive );
            if ( files ) {
                auto e_filter = file_entry_filter::make();
                if ( e_filter ) {
                    auto e_files = filter_iter::make( files, e_filter );
                    if ( e_files ) {
                        std::string filename_s{ filename . string() };
                        auto r_filter = glob_filter::make( filename_s );
                        if ( r_filter ) {
                            return filter_iter::make( e_files, r_filter );
                        }
                    }
                }
            }
            return nullptr;
        }
};

class deleter {
        static std::vector< std::string > glob_pattern( const std::string& pattern ) {
            auto iter = dir_iter::make_globed( pattern );
            return iter -> to_list();
        }

        static bool del_stop_on_err( const std::string& item,
                                     bool silent ) {
            bool res = true;
            if ( fs::is_regular_file( item ) ) {
                try {
                    fs::remove( item );
                    if ( !silent ) { std::cout << "removed: >" << item << "<\n"; }
                } catch ( fs::filesystem_error &e ) {
                    std::cerr << "removing: >" << item << "< " << e.what() << std::endl;
                    res = false;
                }
            } else if ( fs::is_directory( item ) ) {
                try {
                    fs::remove_all( item );
                    if ( !silent ) { std::cout << "removed: >" << item << "<\n"; }
                } catch ( fs::filesystem_error &e ) {
                    std::cerr << "removing: >" << item << "< " << e.what() << std::endl;
                    res = false;
                }
            } else {
                std::cerr << "removing: >" << item << "< is not a file or directory" << std::endl;
                res = false;
            }
            return res;
        }

        static void del_ignore_err( const std::string& item,
                                    bool silent ) {
            if ( fs::is_regular_file( item ) ) {
                try {
                    fs::remove( item );
                    if ( !silent ) { std::cout << "removed: >" << item << "<\n"; }
                } catch ( fs::filesystem_error &e ) {
                }
            } else if ( fs::is_directory( item ) ) {
                try {
                    fs::remove_all( item );
                    if ( !silent ) { std::cout << "removed: >" << item << "<\n"; }
                } catch ( fs::filesystem_error &e ) {
                }
            }
        }

        static bool del_stop_on_err( const std::vector< std::string >& items,
                                     bool silent ) {
            bool res = true;
            for ( auto& item : items ) {
                if ( res ) {
                    auto files = glob_pattern( item );
                    for ( auto& item2 : files ) {
                        if ( res ) {
                            res = del_stop_on_err( item2, silent );
                        }
                    }
                }
            }
            return res;
        }

        static void del_ignore_err( const std::vector< std::string >& items,
                                    bool silent ) {
            for ( auto& item : items ) {
                auto files = glob_pattern( item );
                for ( auto& item2 : files ) {
                    del_ignore_err( item2, silent );
                }
            }
        }

    public :

        static bool del( const std::vector< std::string >& items,
                         bool silent, bool ignore_err ) {
            bool res = true;
            if ( ignore_err ) {
                del_ignore_err( items, silent );
            } else {
                res = del_stop_on_err( items, silent );
            }
            return res;
        }
};

class Log;
typedef std::shared_ptr< Log > LogPtr;
class Log {
    private :
        std::mutex m;

    public :
        Log() {}

        void write( const std::string s, ... ) {
            std::lock_guard<std::mutex> lk( m );
            va_list args;
            va_start( args, s );
            vfprintf( stderr, s . c_str(), args );
            va_end( args );
        }

        void write( const std::stringstream& s ) { write( s . str() ); }

        void write( const char *s, ... ) {
            std::lock_guard<std::mutex> lk( m );
            va_list args;
            va_start( args, s );
            vfprintf( stderr, s, args );
            va_end( args );
        }

        static LogPtr make( void ) { return LogPtr( new Log() ); }
};

typedef std::multimap< const std::string, std::string > str2strmap;
typedef std::pair< const std::string, std::string > strpair;

class StrTool {
    public :

        static void ltrim( std::string &s ) {
            s . erase( s . begin(),
                       find_if( s . begin(),
                                s . end(),
                                []( unsigned char ch ) { return !::isspace( ch ); } ) );
        }

        static void rtrim( std::string &s ) {
            s . erase( find_if( s . rbegin(),
                                s . rend(),
                                [](unsigned char ch) { return !::isspace( ch ); } ) . base(),
                                s . end() );
        }

        static void trim_line( std::string& s ) {
            ltrim( s );
            rtrim( s );
        }

        static std::string trim( const std::string& s ) {
            std::string tmp( s );
            trim_line( tmp );
            return tmp;
        }

        static bool is_comment( const std::string_view& line ) {
            if ( startsWith( line, "/" ) ) return true;
            if ( startsWith( line, "#" ) ) return true;
            return false;
        }

        static bool startsWith( const std::string_view& s, const std::string& prefix ) {
            return ( s . rfind( prefix, 0 ) == 0 );
        }

        static bool startsWith( const std::string_view& s, char c ) {
            if ( s . empty() ) return false;
            return ( s . at( 0 ) == c );
        }

        static bool startsWith( const std::string_view& s, char c1, char c2 ) {
            if ( s . empty() ) return false;
            char c = s . at( 0 );
            return ( c == c1 ) || ( c == c2 );
        }

        static bool endsWith( const std::string_view &s, std::string const &postfix ) {
            if ( s . length() >= postfix . length() ) {
                return ( 0 == s . compare( s . length() - postfix . length(), postfix . length(), postfix ) );
            } else {
                return false;
            }
        }

        static bool endsWith( const std::string_view &s, char c ) {
            if ( s . empty() ) return false;
            return ( s . at( s . length() - 1 ) == c );
        }

        static bool endsWith( const std::string_view &s, char c1, char c2 ) {
            if ( s . empty() ) return false;
            char c = s . at( s . length() - 1 );
            return ( c == c1 ) || ( c == c2 );
        }

        static std::string unquote( const std::string_view& s, char c1, char c2 ) {
            if ( startsWith( s, c1, c2 ) ) {
                if ( endsWith( s, c1, c2 ) ) {
                    std::string tmp{ s };
                    return tmp . substr( 1, tmp . length() - 2 );
                }
            }
            return std::string( s );
        }

        static std::vector< std::string > tokenize( const std::string& s,
                                                    char delimiter = ' ' ) {
            std::vector< std::string > res;
            std::stringstream ss( s );
            std::string token, temp;
            while ( getline( ss, token, delimiter ) ) {
                std::string trimmed = trim( token );
                if ( temp . empty() ) {
                    // we ARE NOT in a quoted list of tokens...
                    if ( startsWith( trimmed, '"', '\'' ) ) {
                        temp = trimmed;
                    } else {
                        res . push_back( trimmed );
                    }
                } else {
                    // we ARE in a quoted list of tokens...
                    temp += delimiter;
                    temp . append( trimmed );
                    if ( endsWith( trimmed, '"', '\'' ) ) {
                        res . push_back( temp );
                        temp . clear();
                    }
                }
            }
            if ( ! temp . empty() ) {
                res . push_back( trim( temp ) );
            }
            return res;
        }

        static void to_stream( std::ostream& os,
                               const std::vector< std::string >& src,
                               const std::string& prefix ) {
            uint32_t idx = 0;
            for ( auto& item : src ) {
                os << prefix << "[ " << idx++ << " ] = " << item << std::endl;
            }
        }

        template <typename T> static T convert( const std::string_view& s, T dflt ) {
            if ( s . empty() ) { return dflt; }
            std::stringstream ss;
            ss << s;
            T res;
            ss >> res;
            return res;
        }

}; // end of class StrTool

class KV_Map;
typedef std::shared_ptr< KV_Map > KV_Map_Ptr;
class KV_Map {
    private:
        util::str2strmap f_kv;

        // Ctor
        KV_Map( void ) {}

        std::string replace_string_items( const std::string_view src,
                                     const std::string_view pattern,
                                     const std::string_view replacement ) {
            std::string tmp{ src };
            bool done = false;
            while ( !done ) {
                auto pos = tmp . find( pattern );
                done = ( std::string::npos == pos );
                if ( !done ) {
                    tmp . replace( pos, pattern . length(), replacement );
                }
            }
            return tmp;
        }

    public:
        // Factory
        static KV_Map_Ptr make( void ) { return KV_Map_Ptr( new KV_Map() ); }

        bool empty( void ) const { return f_kv . empty(); }

        void insert( const std::string& key, const std::string& value ) {
            util::strpair p( key, value );
            f_kv .insert( p );
        }

        bool insert_tokens( const std::string& line, char separator ) {
            bool res = false;
            if ( std::string::npos != line . find( separator ) ) {
                auto tokens = util::StrTool::tokenize( line, separator );
                switch ( tokens . size() ) {
                    case 0  : break;

                    case 1  : insert( tokens[ 0 ], std::string( "" ) );
                              res = true;
                              break;

                    default : insert( tokens[ 0 ], tokens[ 1 ] );
                              res = true;
                              break;
                }
            }
            return res;
        }

        void parse_values( const std::vector< std::string >& src,
                           char separator = ':' ) {
            for ( auto& item : src ) { insert_tokens( item, separator ); }
        }

        void import_values( KV_Map_Ptr other ) {
            for ( const auto& item : other -> f_kv ) {
                insert( item . first, item . second );
            }
        }

        const std::string& get( const std::string& key,
                                const std::string& dflt = "" ) const {
            auto it = f_kv . find( key );
            if ( it == f_kv . end() ) { return dflt; }
            return it -> second;
        }

        bool has( const std::string& key ) const {
            return ( f_kv . find( key ) != f_kv . end() );
        }

        std::vector< std::string > get_values_of( const std::string& key ) const {
            std::vector< std::string > res;
            auto range = f_kv . equal_range( key );
            for ( auto i = range . first; i != range . second; ++i ) {
                res . push_back( i -> second );
            }
            return res;
        }

        std::string replace1( const std::string_view src, char key = '$' ) {
            std::string tmp{ src };
            for ( const auto& kv : f_kv ) {
                std::string pattern{ key };
                pattern += '{';
                pattern += kv . first;
                pattern += '}';
                tmp = replace_string_items( tmp, pattern, kv . second );
            }
            return tmp;
        }

        std::string replace2( const std::string_view src, char key = '$' ) {
            std::string tmp{ src };
            for ( const auto& kv : f_kv ) {
                std::string pattern{ key };
                pattern += kv . first;
                tmp = replace_string_items( tmp, pattern, kv . second );
            }
            return tmp;
        }

        std::string replace( const std::string_view src, char key = '$' ) {
            return replace2( replace1( src ) );
        }

        std::vector< std::string > replace( const std::vector< std::string >& src,
                                            char key = '$' ) {
            std::vector< std::string > res;
            for ( auto& item : src ) { res . push_back( replace( item, key ) ) ; }
            return res;
        }

        void to_stream( std::ostream& os, const std::string& prefix ) {
            for ( auto& item : f_kv ) {
                os << prefix << "[ " << item . first << " ] = " << item . second << std::endl;
            }
        }

        friend auto operator<<( std::ostream& os, KV_Map_Ptr o ) -> std::ostream& {
            o -> to_stream( os, "" );
            return os;
        }
}; // end of class KV_Map

class Ini;
typedef std::shared_ptr< Ini > IniPtr;
class Ini {
    private:
        util::KV_Map_Ptr f_values;
        util::KV_Map_Ptr f_definitions;

        // Ctor
        Ini( void ) : f_values{ util::KV_Map::make() },
                      f_definitions{ util::KV_Map::make() } { }

        void load_line( const std::string& line ) {
            size_t colon_pos = line . find( ':' );
            size_t equ_pos = line . find( '=' );
            if ( colon_pos != std::string::npos ) {
                // we do have a colon
                if ( equ_pos != std::string::npos ) {
                    // we have a colon and a equ
                    if ( colon_pos < equ_pos ) {
                        // colon comes first :
                        f_values -> insert_tokens( line, ':' );
                    } else {
                        // equ comes first :
                        f_definitions -> insert_tokens( line, '=' );
                    }
                } else {
                    // we only have a colon
                    f_values -> insert_tokens( line, ':' );
                }
            } else {
                // we do NOT have a colon
                if ( equ_pos != std::string::npos ) {
                    // we only have a equ
                    f_definitions -> insert_tokens( line, '=' );
                } else {
                    // we have neither a colon nor a equ
                }
            }
        }

        void load( const std::string_view& filename ) {
            std::string fn{ filename };
            std::ifstream f( fn );
            if ( f ) {
                std::string line;
                while ( getline( f, line ) ) {
                    util::StrTool::trim_line( line );
                    if ( ! util::StrTool::is_comment( line ) ) {
                        load_line( line );
                    }
                }
            }
        }

    public:
        static IniPtr make( void ) { return IniPtr( new Ini ); }

        static IniPtr make( const std::string_view& filename ) {
            auto res = IniPtr( new Ini );
            res -> load( filename );
            return res;
        }

        util::KV_Map_Ptr get_values( void ) const { return f_values; }

        std::vector< std::string > get_definitions_of( const std::string& key ) const {
            return f_definitions -> get_values_of( key );
        }

        const std::string& get( const std::string& key,
                                const std::string& dflt = "" ) const {
            return f_definitions -> get( key, dflt );
        }

        void insert_definition( const std::string& key,
                                const std::string& value ) {
            f_definitions -> insert( key, value );
        }

        bool has( const std::string& key ) const { return f_definitions -> has( key ); }

        bool has_value( const std::string& key, const std::string& value ) const {
            return ( 0 == get( key ) . compare( value ) );
        }

        uint8_t get_u8( const std::string& key, uint8_t dflt ) const {
            return util::StrTool::convert< uint8_t >( get( key, "" ), dflt );
        }

        int8_t get_i8( const std::string& key, int8_t dflt ) const {
            return util::StrTool::convert< int8_t >( get( key, "" ), dflt );
        }

        uint16_t get_u16( const std::string& key, uint16_t dflt ) const {
            return util::StrTool::convert< uint16_t >( get( key, "" ), dflt );
        }

        int16_t get_i16( const std::string& key, int16_t dflt ) const {
            return util::StrTool::convert< int16_t >( get( key, "" ), dflt );
        }

        uint32_t get_u32( const std::string& key, uint32_t dflt ) const {
            return util::StrTool::convert< uint32_t >( get( key, "" ), dflt );
        }

        int32_t get_i32( const std::string& key, int32_t dflt ) const {
            return util::StrTool::convert< int32_t >( get( key, "" ), dflt );
        }

        uint64_t get_u64( const std::string& key, uint64_t dflt ) const {
            return util::StrTool::convert< uint64_t >( get( key, "" ), dflt );
        }

        int64_t get_i64( const std::string& key, int64_t dflt ) const {
            return util::StrTool::convert< int64_t >( get( key, "" ), dflt );
        }

        friend auto operator<<( std::ostream& os, IniPtr const& o ) -> std::ostream& {
            os << "DEFS : \n" << o -> f_definitions;
            os << "VALS : \n" << o -> f_values;
            return os;
        }
}; // end of class Ini

class Random;
typedef std::shared_ptr< Random > RandomPtr;
class Random {
    private:
        std::default_random_engine f_generator;
        std::uniform_int_distribution< short > f_bio_base_distribution;
        std::uniform_int_distribution< short > f_bio_qual_distribution;
        std::uniform_int_distribution< short >  f_bio_qual_diff_distribution;
        std::uniform_int_distribution< short >  f_string_distribution;
        std::uniform_int_distribution< short >  f_lower_char_distribution;
        std::uniform_int_distribution< short >  f_upper_char_distribution;

        char f_bases[ 4 ];
        short f_last_qual;

        Random( int seed ) : f_generator( seed ),
            f_bio_base_distribution( 0, 3 ),
            f_bio_qual_distribution( 0, 63 ),
            f_bio_qual_diff_distribution( -5, +5 ),
            f_string_distribution( (short)'A', (short)'z' ),
            f_lower_char_distribution( (short)'a', (short)'z' ),
            f_upper_char_distribution( (short)'A', (short)'Z' )
        { f_bases[ 0 ] = 'A';
          f_bases[ 1 ] = 'C';
          f_bases[ 2 ] = 'G';
          f_bases[ 3 ] = 'T';
          f_last_qual = random_qual();
        }

        uint8_t random_qual( void ) {
            return f_bio_qual_distribution( f_generator );
        }

        uint8_t random_qual_diff( void ) {
            short diff = f_bio_qual_diff_distribution( f_generator );
            if ( diff > 0 ) {
                if ( f_last_qual + diff > 63 ) { diff = -diff; }
            }
            if ( diff < 0 ) {
                if ( f_last_qual < abs( diff ) ) { diff = -diff; }
            }
            f_last_qual += diff;
            return (uint8_t)f_last_qual;
        }

    public:
        static RandomPtr make( int seed ) { return RandomPtr( new Random( seed ) ); }

        char random_char( void ) {
            return f_string_distribution( f_generator );
        }

        char random_lower_char( void ) {
            return f_lower_char_distribution( f_generator );
        }

        char random_upper_char( void ) {
            return f_upper_char_distribution( f_generator );
        }

        std::string random_string( size_t length ) {
            std::string res;
            for ( size_t i = 0; i < length; ++i ) { res += random_char(); }
            return res;
        }

        char random_base( void ) {
            uint8_t x = f_bio_base_distribution( f_generator );
            return f_bases[ x ];
        }

        std::string random_bases( size_t length ) {
            std::string res;
            for ( size_t i = 0; i < length; ++i ) { res += random_base(); }
            return res;
        }

        void random_quals( uint8_t * buffer, size_t length ) {
            for ( size_t i = 0; i < length; ++i ) { buffer[ i ] = random_qual(); }
        }

        void random_diff_quals( uint8_t * buffer, size_t length ) {
            for ( size_t i = 0; i < length; ++i ) { buffer[ i ] = random_qual_diff(); }
        }

        uint32_t random_u32( uint32_t min, uint32_t max ) {
            std::uniform_int_distribution< uint32_t > dist( min, max );
            return dist( f_generator );
        }
}; // end of class Random



#define T_MASK ( ( uint32_t ) ~ 0 )

#define T1 /* 0xd76aa478 */ ( T_MASK ^ 0x28955b87 )
#define T2 /* 0xe8c7b756 */ ( T_MASK ^ 0x173848a9 )
#define T3    0x242070db
#define T4 /* 0xc1bdceee */ ( T_MASK ^ 0x3e423111 )
#define T5 /* 0xf57c0faf */ ( T_MASK ^ 0x0a83f050 )
#define T6    0x4787c62a
#define T7 /* 0xa8304613 */ ( T_MASK ^ 0x57cfb9ec )
#define T8 /* 0xfd469501 */ ( T_MASK ^ 0x02b96afe )
#define T9    0x698098d8
#define T10 /* 0x8b44f7af */ ( T_MASK ^ 0x74bb0850 )
#define T11 /* 0xffff5bb1 */ ( T_MASK ^ 0x0000a44e )
#define T12 /* 0x895cd7be */ ( T_MASK ^ 0x76a32841 )
#define T13    0x6b901122
#define T14 /* 0xfd987193 */ ( T_MASK ^ 0x02678e6c )
#define T15 /* 0xa679438e */ ( T_MASK ^ 0x5986bc71 )
#define T16    0x49b40821
#define T17 /* 0xf61e2562 */ ( T_MASK ^ 0x09e1da9d )
#define T18 /* 0xc040b340 */ ( T_MASK ^ 0x3fbf4cbf )
#define T19    0x265e5a51
#define T20 /* 0xe9b6c7aa */ ( T_MASK ^ 0x16493855 )
#define T21 /* 0xd62f105d */ ( T_MASK ^ 0x29d0efa2 )
#define T22    0x02441453
#define T23 /* 0xd8a1e681 */ ( T_MASK ^ 0x275e197e )
#define T24 /* 0xe7d3fbc8 */ ( T_MASK ^ 0x182c0437 )
#define T25    0x21e1cde6
#define T26 /* 0xc33707d6 */ ( T_MASK ^ 0x3cc8f829 )
#define T27 /* 0xf4d50d87 */ ( T_MASK ^ 0x0b2af278 )
#define T28    0x455a14ed
#define T29 /* 0xa9e3e905 */ ( T_MASK ^ 0x561c16fa )
#define T30 /* 0xfcefa3f8 */ ( T_MASK ^ 0x03105c07 )
#define T31    0x676f02d9
#define T32 /* 0x8d2a4c8a */ ( T_MASK ^ 0x72d5b375 )
#define T33 /* 0xfffa3942 */ ( T_MASK ^ 0x0005c6bd )
#define T34 /* 0x8771f681 */ ( T_MASK ^ 0x788e097e )
#define T35    0x6d9d6122
#define T36 /* 0xfde5380c */ ( T_MASK ^ 0x021ac7f3 )
#define T37 /* 0xa4beea44 */ ( T_MASK ^ 0x5b4115bb )
#define T38    0x4bdecfa9
#define T39 /* 0xf6bb4b60 */ ( T_MASK ^ 0x0944b49f )
#define T40 /* 0xbebfbc70 */ ( T_MASK ^ 0x4140438f )
#define T41    0x289b7ec6
#define T42 /* 0xeaa127fa */ ( T_MASK ^ 0x155ed805 )
#define T43 /* 0xd4ef3085 */ ( T_MASK ^ 0x2b10cf7a )
#define T44    0x04881d05
#define T45 /* 0xd9d4d039 */ ( T_MASK ^ 0x262b2fc6 )
#define T46 /* 0xe6db99e5 */ ( T_MASK ^ 0x1924661a )
#define T47    0x1fa27cf8
#define T48 /* 0xc4ac5665 */ ( T_MASK ^ 0x3b53a99a )
#define T49 /* 0xf4292244 */ ( T_MASK ^ 0x0bd6ddbb )
#define T50    0x432aff97
#define T51 /* 0xab9423a7 */ ( T_MASK ^ 0x546bdc58 )
#define T52 /* 0xfc93a039 */ ( T_MASK ^ 0x036c5fc6 )
#define T53    0x655b59c3
#define T54 /* 0x8f0ccc92 */ ( T_MASK ^ 0x70f3336d )
#define T55 /* 0xffeff47d */ ( T_MASK ^ 0x00100b82 )
#define T56 /* 0x85845dd1 */ ( T_MASK ^ 0x7a7ba22e )
#define T57    0x6fa87e4f
#define T58 /* 0xfe2ce6e0 */ ( T_MASK ^ 0x01d3191f )
#define T59 /* 0xa3014314 */ ( T_MASK ^ 0x5cfebceb )
#define T60    0x4e0811a1
#define T61 /* 0xf7537e82 */ ( T_MASK ^ 0x08ac817d )
#define T62 /* 0xbd3af235 */ ( T_MASK ^ 0x42c50dca )
#define T63    0x2ad7d2bb
#define T64 /* 0xeb86d391 */ ( T_MASK ^ 0x14792c6e )

    class MD5;
    typedef std::shared_ptr< MD5 > MD5_ptr;
    class MD5 {
        private :
            uint32_t count [ 2 ];
            uint32_t abcd [ 4 ];
            uint8_t buf [ 64 ];

            // private Ctor
            MD5( void ) {
                count [ 0 ] = count [ 1 ] = 0;
                abcd [ 0 ] = 0x67452301;
                abcd [ 1 ] = /*0xefcdab89*/ T_MASK ^ 0x10325476;
                abcd [ 2 ] = /*0x98badcfe*/ T_MASK ^ 0x67452301;
                abcd [ 3 ] = 0x10325476;
                std::memset( buf, 0, sizeof( buf ) );
            }

            void process ( const uint8_t *data /* [ 64 ] */ ) {
                uint32_t t;

                uint32_t a = abcd [ 0 ];
                uint32_t b = abcd [ 1 ];
                uint32_t c = abcd [ 2 ];
                uint32_t d = abcd [ 3 ];

#if __BYTE_ORDER == __LITTLE_ENDIAN

                uint32_t xbuf [ 16 ];

                /* assume proper alignment */
                const uint32_t *X = ( const uint32_t* ) data;

                /* check alignment */
                if ( ( ( size_t ) X & 3 ) != 0 )
                {
                    /* use buffer instead */
                    X = xbuf;
                    memmove ( xbuf, data, 64 );
                }

#elif __BYTE_ORDER == __BIG_ENDIAN

                int i;
                uint32_t X [ 16 ];
                if ( ( ( size_t ) data & 3 ) == 0 )
                {
                    const uint32_t *xp = ( const uint32_t* ) data;
                    for ( i = 0; i < 16; ++ i )
                        X [ i ] = bswap_32 ( xp [ i ] );
                }

                else
                {
                    const uint8_t *xp = data;
                    for ( i = 0; i < 16; xp += 4, ++ i )
                        X [ i ] = xp [ 0 ] + ( xp [ 1 ] << 8 ) + ( xp [ 2 ] << 16 ) + ( xp [ 3 ] << 24 );
                }

#else
#error "only big or little endian is supported"
#endif

#define ROTATE_LEFT( x, n ) \
    ( ( ( x ) << ( n ) ) | ( ( x ) >> ( 32 - ( n ) ) ) )

    /* Round 1. */
    /* Let  [ abcd k s i ] denote the operation
       a = b + ( ( a + F ( b,c,d ) + X [ k ] + T [ i ] ) <<< s ) . */
#define F( x, y, z ) \
    ( ( ( x ) & ( y ) ) | ( ~ ( x ) & ( z ) ) )
#define SET( a, b, c, d, k, s, Ti ) \
    t = a + F ( b,c,d ) + X [ k ] + Ti;\
    a = ROTATE_LEFT ( t, s ) + b

    /* Do the following 16 operations. */
    SET ( a, b, c, d,  0,  7,  T1 );
    SET ( d, a, b, c,  1, 12,  T2 );
    SET ( c, d, a, b,  2, 17,  T3 );
    SET ( b, c, d, a,  3, 22,  T4 );
    SET ( a, b, c, d,  4,  7,  T5 );
    SET ( d, a, b, c,  5, 12,  T6 );
    SET ( c, d, a, b,  6, 17,  T7 );
    SET ( b, c, d, a,  7, 22,  T8 );
    SET ( a, b, c, d,  8,  7,  T9 );
    SET ( d, a, b, c,  9, 12, T10 );
    SET ( c, d, a, b, 10, 17, T11 );
    SET ( b, c, d, a, 11, 22, T12 );
    SET ( a, b, c, d, 12,  7, T13 );
    SET ( d, a, b, c, 13, 12, T14 );
    SET ( c, d, a, b, 14, 17, T15 );
    SET ( b, c, d, a, 15, 22, T16 );
#undef SET
#undef F

    /* Round 2. */
    /* Let  [ abcd k s i ] denote the operation
       a = b + ( ( a + G ( b,c,d ) + X [ k ] + T [ i ] ) <<< s ) . */
#define G( x, y, z ) \
    ( ( ( x ) & ( z ) ) | ( ( y ) & ~ ( z ) ) )
#define SET( a, b, c, d, k, s, Ti ) \
    t = a + G ( b,c,d ) + X [ k ] + Ti;\
    a = ROTATE_LEFT ( t, s ) + b

    /* Do the following 16 operations. */
    SET ( a, b, c, d,  1,  5, T17 );
    SET ( d, a, b, c,  6,  9, T18 );
    SET ( c, d, a, b, 11, 14, T19 );
    SET ( b, c, d, a,  0, 20, T20 );
    SET ( a, b, c, d,  5,  5, T21 );
    SET ( d, a, b, c, 10,  9, T22 );
    SET ( c, d, a, b, 15, 14, T23 );
    SET ( b, c, d, a,  4, 20, T24 );
    SET ( a, b, c, d,  9,  5, T25 );
    SET ( d, a, b, c, 14,  9, T26 );
    SET ( c, d, a, b,  3, 14, T27 );
    SET ( b, c, d, a,  8, 20, T28 );
    SET ( a, b, c, d, 13,  5, T29 );
    SET ( d, a, b, c,  2,  9, T30 );
    SET ( c, d, a, b,  7, 14, T31 );
    SET ( b, c, d, a, 12, 20, T32 );
#undef SET
#undef G

    /* Round 3. */
    /* Let  [ abcd k s t ] denote the operation
       a = b + ( ( a + H ( b,c,d ) + X [ k ] + T [ i ] ) <<< s ) . */
#define H( x, y, z ) \
    ( ( x ) ^ ( y ) ^ ( z ) )
#define SET( a, b, c, d, k, s, Ti ) \
    t = a + H ( b,c,d ) + X [ k ] + Ti;\
    a = ROTATE_LEFT ( t, s ) + b

    /* Do the following 16 operations. */
    SET ( a, b, c, d,  5,  4, T33 );
    SET ( d, a, b, c,  8, 11, T34 );
    SET ( c, d, a, b, 11, 16, T35 );
    SET ( b, c, d, a, 14, 23, T36 );
    SET ( a, b, c, d,  1,  4, T37 );
    SET ( d, a, b, c,  4, 11, T38 );
    SET ( c, d, a, b,  7, 16, T39 );
    SET ( b, c, d, a, 10, 23, T40 );
    SET ( a, b, c, d, 13,  4, T41 );
    SET ( d, a, b, c,  0, 11, T42 );
    SET ( c, d, a, b,  3, 16, T43 );
    SET ( b, c, d, a,  6, 23, T44 );
    SET ( a, b, c, d,  9,  4, T45 );
    SET ( d, a, b, c, 12, 11, T46 );
    SET ( c, d, a, b, 15, 16, T47 );
    SET ( b, c, d, a,  2, 23, T48 );
#undef SET
#undef H

    /* Round 4. */
    /* Let  [ abcd k s t ] denote the operation
       a = b + ( ( a + I ( b,c,d ) + X [ k ] + T [ i ] ) <<< s ) . */
#define I( x, y, z )                            \
    ( ( y ) ^ ( ( x ) | ~ ( z ) ) )
#define SET( a, b, c, d, k, s, Ti ) \
    t = a + I ( b,c,d ) + X [ k ] + Ti;\
    a = ROTATE_LEFT ( t, s ) + b

    /* Do the following 16 operations. */
    SET ( a, b, c, d,  0,  6, T49 );
    SET ( d, a, b, c,  7, 10, T50 );
    SET ( c, d, a, b, 14, 15, T51 );
    SET ( b, c, d, a,  5, 21, T52 );
    SET ( a, b, c, d, 12,  6, T53 );
    SET ( d, a, b, c,  3, 10, T54 );
    SET ( c, d, a, b, 10, 15, T55 );
    SET ( b, c, d, a,  1, 21, T56 );
    SET ( a, b, c, d,  8,  6, T57 );
    SET ( d, a, b, c, 15, 10, T58 );
    SET ( c, d, a, b,  6, 15, T59 );
    SET ( b, c, d, a, 13, 21, T60 );
    SET ( a, b, c, d,  4,  6, T61 );
    SET ( d, a, b, c, 11, 10, T62 );
    SET ( c, d, a, b,  2, 15, T63 );
    SET ( b, c, d, a,  9, 21, T64 );
#undef SET
#undef I

            /* Then perform the following additions. ( That is increment each
                of the four registers by the value it had before this block
                was started. ) */
                abcd [ 0 ] += a;
                abcd [ 1 ] += b;
                abcd [ 2 ] += c;
                abcd [ 3 ] += d;
            }

            void finish( uint8_t digest[ 16 ] ) {
                int i;
                uint8_t data [ 8 ];
                static const uint8_t pad [ 64 ] = {
                    0x80, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
                };

                /* save the length before padding in little-endian order */
                for ( i = 0; i < 8; ++ i ) {
                    data [ i ] = ( uint8_t ) ( count [ i >> 2 ] >> ( ( i & 3 ) << 3 ) );
                }

                /* Pad to 56 bytes mod 64. */
                append( pad, ( ( 55 - ( count [ 0 ] >> 3 ) ) & 63 ) + 1 );

                /* Append the length. */
                append( data, 8 );

                /* create digest */
                for ( i = 0; i < 16; ++ i ) {
                    digest [ i ] = ( uint8_t ) ( abcd [ i >> 2 ] >> ( ( i & 3 ) << 3 ) );
                }
            }

        public :
            static MD5_ptr make( void ) {
                return MD5_ptr( new MD5() );
            }

            void append( const void *data, size_t size ) {
                if ( data != NULL && size > 0 ) {
                    const uint8_t *p = ( const uint8_t* )data;
                    size_t left = size;
                    size_t offset = ( count [ 0 ] >> 3 ) & 63;
                    uint32_t nbits = ( uint32_t ) ( size << 3 );

                    /* update the message length. */
                    count [ 1 ] += ( uint32_t ) size >> 29;
                    count [ 0 ] += nbits;

                    /* detect roll-over */
                    if ( count [ 0 ] < nbits ) { ++count [ 1 ]; }

                    /* process an initial partial block. */
                    if ( offset )
                    {
                        /* bytes to copy from input data are from offset up to 64 */
                        size_t copy = ( offset + size > 64 ? 64 - offset : size );
                        memmove ( buf + offset, p, copy );

                        /* don't process a tiny partial block */
                        if ( offset + copy < 64 ) return;

                        /* trim off initial bytes */
                        p += copy;
                        left -= copy;

                        /* process full state buffer */
                        process( buf );
                    }

                    /* continue processing blocks directly from input */
                    for ( ; left >= 64; p += 64, left -= 64 ) {
                        process( p );
                    }

                    /* buffer any remainder */
                    if ( left )
                        memmove( buf, p, left );
                }
            }

            std::string finish( void ) {
                uint8_t digest[ 16 ];
                finish( digest );
                std::stringstream ss;
                ss << std::hex;
                for ( uint32_t i = 0; i < 16; ++i ) {
                    uint32_t x = digest[ i ];
                    ss << x;
                }
                return ss . str();
            }
    }; // end of class MD5

struct Data {
    uint64_t id;
    std::string read;

    void set_ids( int64_t seq_spot_id, uint32_t seq_read_id ) {
        id = compress_lookup_id( seq_spot_id, seq_read_id );
    }

    void get_idx( int64_t& seq_spot_id, uint32_t& seq_read_id ) {
        decompress_lookup_id( id, seq_spot_id, seq_read_id );
    }

    // seq_read_id is 1-based ( valid values are 1 and 2 )
    static uint64_t compress_lookup_id( int64_t seq_spot_id, uint32_t seq_read_id ) {
        uint64_t x = seq_spot_id;
        x <<= 1;
        if ( 1 == seq_read_id ) { return ( x & 0xFFFFFFFFFFFFFFFE ); }
        return ( x | 1 );
    }

    static void decompress_lookup_id( uint64_t id, int64_t& seq_spot_id, uint32_t& seq_read_id ) {
        seq_spot_id = id >> 1;
        if ( 1 == ( id & 1 ) ) {
            seq_read_id = 2;
        } else {
            seq_read_id = 1;
        }
    }
};

struct Batch;
typedef std::shared_ptr< Batch > BatchPtr;
struct Batch {
    size_t curr;
    size_t entries;
    Data * data;

    Batch( size_t a_entries ) : curr( 0 ), entries( a_entries ), data( nullptr ) {
        data = new Data[ a_entries ];
    }

    ~Batch() { if ( nullptr != data ) { delete[] data; } }

    bool is_full( void ) const { return curr >= entries; }

    void reset( void ) { curr = 0; }

    static BatchPtr make( size_t a_entries ) { return BatchPtr( new Batch( a_entries ) ); }
};

class Errors;
typedef std::shared_ptr< Errors > ErrorsPtr;
class Errors {
    private :
        uint32_t value;
        std::mutex m;

    public :
        Errors() : value( 0 ) {}

        void inc( void ) {
            std::lock_guard<std::mutex> lk( m );
            value++;
        }

        uint32_t get( void ) {
            std::lock_guard<std::mutex> lk( m );
            return value;
        }

        static ErrorsPtr make( void ) { return ErrorsPtr( new Errors() ); }
};

class out_stream;
typedef std::shared_ptr< out_stream > out_stream_ptr;
class out_stream {
    public :
        virtual void take( const char* buffer, size_t num_bytes ) = 0;
        virtual void finish( void ) {}
};

class file_out_stream : public out_stream {
    private :
        std::ofstream f_stream;

        // private Ctor...
        file_out_stream( const std::string& filename ) : f_stream() {
            f_stream . open( filename );
        }

    public :
        virtual ~file_out_stream() = default;

        static out_stream_ptr make( const std::string& filename ) {
            return out_stream_ptr ( new file_out_stream( filename ) );
        }

        void take( const char* buffer, size_t num_bytes ) override {
            f_stream . write( buffer, num_bytes );
        }

        void finish( void ) override {
            f_stream . close();
        }
};

class string_out_stream : public out_stream {
    private :
        std::string& f_data;

        // private Ctor...
        string_out_stream( std::string& data ) : f_data( data ) { }

    public :
        virtual ~string_out_stream() = default;

        static out_stream_ptr make( std::string& data ) {
            return out_stream_ptr ( new string_out_stream( data ) );
        }

        void take( const char* buffer, size_t num_bytes ) override {
            f_data . append( buffer, num_bytes );
        }
};

class std_out_stream : public out_stream {
    private :
        // private Ctor...
        std_out_stream( void ) { }

    public :
        virtual ~std_out_stream() = default;

        static out_stream_ptr make( void ) {
            return out_stream_ptr( new std_out_stream() );
        }

        void take( const char* buffer, size_t num_bytes ) override {
            std::cout . write( buffer, num_bytes );
        }
};

class std_err_stream : public out_stream {
    private :
        // private Ctor...
        std_err_stream( void ) { }

    public :
        virtual ~std_err_stream() = default;

        static out_stream_ptr make( void ) {
            return out_stream_ptr( new std_err_stream() );
        }

        void take( const char* buffer, size_t num_bytes ) override {
            std::cerr . write( buffer, num_bytes );
        }
};

class md5_stream : public out_stream {
    private :
        util::MD5_ptr f_md5;
        std::string& f_digest;

        // private Ctor...
        md5_stream( std::string& digest )
            : f_md5{ util::MD5::make() }, f_digest{ digest } { }

    public :
        virtual ~md5_stream() = default;

        static out_stream_ptr make( std::string& digest ) {
            return out_stream_ptr( new md5_stream( digest ) );
        }

        void take( const char* buffer, size_t num_bytes ) override {
            f_md5 -> append( buffer, num_bytes );
        }

        void finish( void ) override { f_digest = f_md5 -> finish(); }
};

    class process;
    typedef std::shared_ptr< process > process_ptr;
    class process {
        private:
            bool f_started;
            bool f_joined;
            int f_options;
            int f_result;
            int f_ret_code;
            struct subprocess_s f_proc;

            process( int options ) : f_started{ false }, f_joined{ false }, f_options{ options } {}

            static process_ptr make( int options ) {
                return process_ptr( new process( options ) );
            }

            static bool vector_to_chars( const std::vector<std::string>& cmd, const char*** out ) {
                bool res = false;
                if ( !cmd . empty() ) {
                    size_t n = cmd . size() + 1;
                    const char ** tmp = ( const char ** )malloc( sizeof tmp[ 0 ] * n );
                    if ( nullptr != tmp ) {
                        uint32_t idx = 0;
                        for ( const std::string& s : cmd ) {
                            tmp[ idx++ ] = s . c_str();
                        }
                        tmp[ idx ] = nullptr;
                        *out = tmp;
                        res = true;
                    }
                }
                return res;
            }

            unsigned int read_stdout( char* buffer, size_t buffer_size ) {
                unsigned int bytes_read = 0;
                if ( f_started ) {
                    bytes_read = subprocess_read_stdout( &f_proc, buffer, buffer_size );
                    buffer[ bytes_read ] = 0;
                }
                return bytes_read;
            }

            unsigned int read_stderr( char* buffer, size_t buffer_size ) {
                unsigned int bytes_read = 0;
                if ( f_started ) {
                    bytes_read = subprocess_read_stderr( &f_proc, buffer, buffer_size );
                    buffer[ bytes_read ] = 0;
                }
                return bytes_read;
            }

            // takes a C-style array of char* pointers
            static std::optional< process_ptr > make( const char * cmd[], int options = 0 ) {
                auto p = make( options );
                p -> f_result = subprocess_create( cmd, p -> f_options, &( p -> f_proc ) );
                p -> f_started = ( 0 == p -> f_result );
                if ( p -> f_started ) { return p; }
                return {};
            }

            static std::optional< int > execute( const char * cmd[] ) {
                auto p = process::make( cmd );
                if ( p . has_value() ) {
                    auto pp = p . value();
                    if ( pp -> join() ) {
                        return pp -> f_ret_code;
                    }
                }
                return {};
            }

            // takes a C++-style vector of strings
            static std::optional< process_ptr > make( const std::vector<std::string>& cmd,
                    int options = 0 ) {
                const char ** tmp;
                if ( vector_to_chars( cmd, &tmp ) ) {
                    auto res = make( tmp, options );
                    free( tmp );
                    return res;
                }
                return {};
            }

            bool is_alive( void ) {
                if ( f_started && !f_joined ) {
                    return subprocess_alive( &f_proc );
                }
                return false;
            }

            bool join( void ) {
                if ( f_started ) {
                    f_joined = ( 0 == subprocess_join( &f_proc, &f_ret_code ) );
                }
                return f_joined;
            }

        public:
            ~process( void ) {
                if ( f_started ) {
                    if ( !f_joined ) {
                        join();
                    }
                    subprocess_destroy( &f_proc );
                }
            }

            static std::optional< int > execute( const std::vector<std::string>& cmd ) {
                const char ** tmp;
                if ( vector_to_chars( cmd, &tmp ) ) {
                    auto res = execute( tmp );
                    free( tmp );
                    return res;
                }
                return {};
            }

            static std::optional< int > execute( const std::vector<std::string>& cmd,
                    out_stream_ptr sout,
                    out_stream_ptr serr ) {
                const char ** tmp;
                if ( vector_to_chars( cmd, &tmp ) ) {
                    auto p = process::make( cmd,  subprocess_option_enable_async );
                    if ( p . has_value() ) {
                        auto pp = p . value();
                        char buffer[ 4096 ];
                        unsigned int read;
                        while ( pp -> is_alive() ) {
                            if ( sout ) {
                                read = pp -> read_stdout( buffer, sizeof buffer );
                                if ( read > 0 ) { sout -> take( buffer, read ); }
                            }
                            if ( serr ) {
                                read = pp -> read_stderr( buffer, sizeof buffer );
                                if ( read > 0 ) { serr -> take( buffer, read ); }
                            }
                        }
                        if ( sout ) {
                            read = 1;
                            while ( read > 0 ) {
                                read = pp -> read_stdout( buffer, sizeof buffer );
                                if ( read > 0 ) { sout -> take( buffer, read ); }
                            }
                        }
                        if ( serr ) {
                            read = 1;
                            while ( read > 0 ) {
                                read = pp -> read_stderr( buffer, sizeof buffer );
                                if ( read > 0 ) { serr -> take( buffer, read ); }
                            }
                        }
                        pp -> join();
                        if ( sout ) { sout -> finish(); }
                        if ( serr ) { serr -> finish(); }
                        return pp -> f_ret_code;
                    }
                    free( tmp );
                }
                return {};
            }

    };

struct channel {
    std::string selector;
    std::string key;
    std::string value;
};

class execute;
typedef std::shared_ptr< execute > execute_ptr;
class execute {
    private :
        util::KV_Map_Ptr f_values;
        std::string f_cmd;
        channel f_stdout;
        channel f_stderr;
        std::vector< std::string > f_args;
        bool f_silent;

        // private Ctor
        execute( util::KV_Map_Ptr values ) : f_values( values ) {}

        /* -------------------------------------------------------------------
        * outfile ... empty --> fd_to_null_reader    ( pipe -> NULL )
        * outfile ... "-"   --> fd_to_stdout_reader  ( pipe -> stdout )
        * outfile ... "--"  --> fd_to_stderr_reader  ( pipe -> stderr )
        * outfile ... ".:name" --> fd_to_md5_reader  ( pipe -> md5 -> value )
        * outfile ... ".name"  --> fd_to_md5_reader  ( pipe -> md5 -> file )
        * outfile ... ":name"  --> fd_to_file_reader ( pipe -> value )
        * outfile ... "name"   --> fd_to_file_reader ( pipe -> file )
        ------------------------------------------------------------------- */
        out_stream_ptr make_stream( channel& c ) {
            c . value . clear();
            c . key . clear();

            if ( c . selector . empty() ) { return nullptr; } // process-class can handle that

            if ( 0 == c . selector . compare( "-" ) ) {
                return std_out_stream::make(); // pipe -> stdout
            }

            if ( 0 == c . selector . compare( "--" ) ) {
                return std_err_stream::make(); // pipe -> stderr
            }

            if ( util::StrTool::startsWith( c . selector, ".:" ) ) {
                c . key = c . selector . substr( 2 );
                return md5_stream::make( c . value ); // pipe -> md5 -> value
                // the post-process has to write the value into f_values!
            }

            if ( util::StrTool::startsWith( c . selector, "." ) ) {
                c . key = c . selector . substr( 1 );
                return md5_stream::make( c . value ); // pipe -> md5 -> file
                // the post-process has to write the value into a file name f_value_k!
            }

            if ( util::StrTool::startsWith( c . selector, ":" ) ) {
                c . key = c . selector . substr( 1 );
                return string_out_stream::make( c . value ); // pipe -> value
                // the post-process has to write the value into f_values!
            }

            // if the selector does not have a special prefix, and is not empty
            return file_out_stream::make( c . selector );
        }

        void postprocess( channel& c ) {
            if ( util::StrTool::startsWith( c . selector, ".:" ) ) {
                f_values ->insert( c . key, c . value );
            } else if ( util::StrTool::startsWith( c . selector, "." ) ) {
                std::ofstream out( c . key );
                out << c . value;
                out . close();

            } else if ( util::StrTool::startsWith( c . selector, ":" ) ) {
                f_values ->insert( c . key, c . value );
            }
        }

    public :
        static execute_ptr make( util::KV_Map_Ptr values ) {
            return execute_ptr( new execute( values ) );
        }

        void set_exe( const std::string& cmd ) { f_cmd = cmd; }
        void set_stdout( const std::string_view& selector ) { f_stdout . selector = selector; }
        void set_stderr( const std::string_view& selector ) { f_stderr . selector = selector; }
        void set_silent( bool silent ) { f_silent = silent; }

        void set_args( std::vector< std::string > args ) {
            f_args . clear();
            for ( auto& item : args ) {
                if ( ! item . empty() ) {
                    f_args . push_back( item );
                }
            }
        }

        int run( void ) {
            std::vector< std::string > cmd;
            cmd . push_back( f_cmd );
            for ( auto& item : f_args ) { cmd . push_back( item ); }
            auto sout = make_stream( f_stdout );
            auto serr = make_stream( f_stderr );
            auto res = process::process::execute( cmd, sout, serr );
            postprocess( f_stdout );
            postprocess( f_stderr );
            if ( res . has_value() ) {
                return res . value();
            }
            return -1;
        }
};

}; // end of namespace >>>util<<<

/* ======================================================================================== */

namespace util_test {

class fs_iter_test {
    protected :
        void header( const std::string& s, bool recursive ) {
            std::cout << "----- ";
            if ( recursive ) {
                std::cout << "recursive";
            } else {
                std::cout << "flat";
            }
            std::cout << "_iter_" << s << " -----------------\n";
        }

        util::fs_iter_ptr make( const std::string& s, bool recursive = false ) {
            return util::dir_iter::make( s, recursive );
        }

        void show( util::fs_iter_ptr iter ) {
            if ( iter ) {
                for( auto e : util::file_iter_range( iter ) ) {
                    std::cout << "[" << e . value() . path() . string() << "] ";
                };
                std::cout << std::endl << std::endl;
            }
        }
};

class test_iter : public fs_iter_test {
    public:
        void run( const std::string& s, bool recursive = false ) {
            header( "", recursive );
            show( make( s, recursive ) );
        }
};

class test_iter_files : public fs_iter_test {
    public:
        void run( const std::string& s, bool recursive = false ) {
            header( "files", recursive );
            auto filter = util::file_entry_filter::make();
            show( util::filter_iter::make( make( s, recursive ), filter ) );
        }
};

class test_iter_dirs : public fs_iter_test {
    public:
        void run( const std::string& s, bool recursive = false ) {
            header( "dirs", recursive );
            auto filter = util::dir_entry_filter::make();
            show( util::filter_iter::make( make( s, recursive ), filter ) );
        }
};

class test_iter_exclude : public fs_iter_test {
    public:
        void run( const std::string& s, bool recursive = false ) {
            header( "exclude", recursive );
            auto filter = util::ending_filter::make( ".hpp", false );
            show( util::filter_iter::make( make( s, recursive ), filter ) );
        }
};

class test_iter_include : public fs_iter_test {
    public:
        void run( const std::string& s, bool recursive = false ) {
            header( "include", recursive );
            auto filter = util::ending_filter::make( ".hpp" );
            show( util::filter_iter::make( make( s, recursive ), filter ) );
        }
};

class test_iter_regex : public fs_iter_test {
    public:
        void run( const std::string& s, bool recursive = false ) {
            header( "regex", recursive );
            auto filter = util::regex_filter::make( ".*[hc]pp" );
            show( util::filter_iter::make( make( s, recursive ), filter ) );
        }
};

class test_iter_regex_bad : public fs_iter_test {
    public:
        void run( const std::string& s, bool recursive = false ) {
            header( "regex_bad", recursive );
            auto filter = util::regex_filter::make( "*" );
            show( util::filter_iter::make( make( s, recursive ), filter ) );
        }
};

class test_iter_glob : public fs_iter_test {
    public:
        void run( const std::string& s, bool recursive = false ) {
            header( "glob", recursive );
            auto filter = util::glob_filter::make( "*.?pp" );
            show( util::filter_iter::make( make( s, recursive ), filter ) );
        }
};

class test_globed : public fs_iter_test {
    public:
        void run( const std::string& s, bool recursive = false ) {
            header( "globed", recursive );
            std::string glob_path { s + "/*.?pp" };
            show( util::dir_iter::make_globed( glob_path, recursive ) );
        }
};

class fs_iter_tests {
        void suite( const std::string& s, bool recursive ) {
            test_iter() . run( s, recursive );
            test_iter_files() . run( s, recursive );
            test_iter_dirs() . run( s, recursive );
            test_iter_exclude() . run( s, recursive );
            test_iter_include() . run( s, recursive );
            test_iter_regex() . run( s, recursive );
            test_iter_regex_bad() . run( s, recursive );
            test_iter_glob() . run( s, recursive );
            test_globed() . run( s , recursive );
        }
    public:
        void run( const std::string& s ) {
            suite( s, false );
            suite( s, true );
        }
};

}; // end of namespace util_test
