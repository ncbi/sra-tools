#include <iostream>
#include <fstream>
#include <string>
#include <map>
#include <vector>
#include <algorithm>
#include <sstream>
#include <memory>
#include <cstring>

/* -----------------------------------------------------------------
 * a tool to create SAM output from input-files( s ) or stdin
 * SAM output will be produced on stdout
 * ----------------------------------------------------------------- */

const int FLAG_MULTI = 0x01;
const int FLAG_PROPPER = 0x02;
const int FLAG_UNMAPPED = 0x04;
const int FLAG_NEXT_UNMAPPED = 0x08;
const int FLAG_REVERSED = 0x010;
const int FLAG_NEXT_REVERSED = 0x020;
const int FLAG_FIRST = 0x040;
const int FLAG_LAST = 0x080;
const int FLAG_SECONDARY = 0x0100;
const int FLAG_BAD = 0x0200;
const int FLAG_PCR = 0x0400;

/* -----------------------------------------------------------------
 * cigar - functions
 * ----------------------------------------------------------------- */

struct cigar_t
{
    char * op;
    int * count;
    size_t size, length;
};

static void init_cigar_t( struct cigar_t * c, size_t size )
{
    if ( c != NULL ) {
        c -> size = 0;
        c -> length = 0;
        c -> op = ( char * )malloc( sizeof( c -> op[ 0 ] ) * size );
        if ( c -> op != NULL ) {
            c -> count = ( int * )malloc( sizeof( c -> count[ 0 ] ) * size );
            if ( c -> count != NULL ) {
                c -> size = size;
            } else {
                free( ( void * ) c -> op );
            }
        }
    }
}

static void resize_cigar_t( struct cigar_t * c, size_t new_size )
{
    if ( c != NULL ) {
        if ( c -> size == 0 ) {
            init_cigar_t( c, new_size );
        } else if ( c->size < new_size ) {
            char * temp_op = c -> op;
            c -> op = ( char * )realloc( c -> op, sizeof( c -> op[ 0 ] ) * new_size );
            if ( c -> op != NULL ) {
                int * temp_count = c -> count;
                c -> count = ( int * )realloc( c -> count, sizeof( c -> count[ 0 ] ) * new_size );
                if ( c -> count != NULL ) {
                    c -> size = new_size;
                } else {
                    c->count = temp_count;
                }
            } else {
                c -> op = temp_op;
            }
        }
    }
}

static void append_to_cigar_t( struct cigar_t * c, char op, int count )
{
    if ( c -> length < c -> size ) {
        c->op[ c -> length ] = op;
        c->count[ c -> length ++ ] = count;
    }
}

void parse_cigar_t( struct cigar_t * c, const char * cigar_str )
{
    if ( c != NULL && cigar_str != NULL && cigar_str[ 0 ] != 0 ) {
        resize_cigar_t( c, strlen( cigar_str ) );
        if ( c -> size > 0 ) {
            int count = 0;
            while ( *cigar_str != 0 && c -> length < c -> size ) {
                if ( isdigit( *cigar_str ) ) {
                    count = ( count * 10 ) + ( *cigar_str - '0' );
                } else {
                    if ( count == 0 ) count = 1;
                    append_to_cigar_t( c, *cigar_str, count );
                    count = 0;
                }
                cigar_str++;
            }
        }
    }
}

struct cigar_t * make_cigar_t( const char * cigar_str ) {
    struct cigar_t * res = ( struct cigar_t * )malloc( sizeof * res );
    if ( res != NULL ) {
        size_t size;
        if ( cigar_str != NULL && cigar_str[ 0 ] != 0 ) {
            size = strlen( cigar_str );
        } else {
            size = 1024;
        }
        init_cigar_t( res, size );
        if ( res -> size == size ) {
            parse_cigar_t( res, cigar_str );
        }
    }
    return res;
}

void free_cigar_t( struct cigar_t * c ) {
    if ( c != NULL ) {
        if ( c -> op != NULL ) {
            free( ( void * ) c -> op );
            c -> op = NULL;
        }
        if ( c -> count != NULL ) {
            free( ( void * ) c -> count );
            c -> count = NULL;
        }
        free( ( void * ) c );
    }
}

int cigar_t_reflen( const struct cigar_t * c ) {
    int res = 0;
    if ( c != NULL ) {
        for ( size_t i = 0; i < c -> length; ++i ) {
            switch( c -> op[ i ] ) {
                case 'A'    : res += c -> count[ i ]; break;
                case 'C'    : res += c -> count[ i ]; break;
                case 'G'    : res += c -> count[ i ]; break;
                case 'T'    : res += c -> count[ i ]; break;

                case 'D'    : res += c -> count[ i ]; break;
                case 'M'    : res += c -> count[ i ]; break;            
            }
        }
    }
    return res;
}

int cigar_t_readlen( const struct cigar_t * c ) {
    int res = 0;
    if ( c != NULL ) {
        for ( size_t i = 0; i < c -> length; ++i ) {
            if ( c -> op[ i ] != 'D' ) {
                res += c->count[ i ];
            }
        }
    }
    return res;
}

int cigar_t_inslen( const struct cigar_t * c ) {
    int res = 0;
    if ( c != NULL ) {
        for ( size_t i = 0; i < c -> length; ++i ) {
            if ( c -> op[ i ] == 'I' ) {
                res += c -> count[ i ];
            }
        }
    }
    return res;
}

size_t cigar_t_string( char * buffer, size_t buf_len, const struct cigar_t * c ) {
    size_t res = 0;
    if ( buffer != NULL && buf_len > 0 && c != NULL && c -> length > 0 ) {
        for ( size_t i = 0; i < c->length && res < buf_len; ++i ) {
            int num_writ = snprintf( &buffer[ res ], buf_len - res, "%d%c", c->count[ i ], c->op[ i ] );
            res += num_writ;
        }
        if ( res < buf_len ) {
            buffer[ res ] = 0;
        }
    }
    return res;
}

static int can_merge( char op1, char op2 ) {
    char mop1 = op1;
    char mop2 = op2;
    if ( mop1 == 'A' || mop1 == 'C' || mop1 == 'G' || mop1 == 'T' ) { mop1 = 'M'; }
    if ( mop2 == 'A' || mop2 == 'C' || mop2 == 'G' || mop2 == 'T' ) { mop2 = 'M'; }
    return ( mop1 == mop2 );
}

struct cigar_t * merge_cigar_t( const struct cigar_t * c ) {
    struct cigar_t * res = NULL;
    if ( c != NULL && c -> length > 0 ) {
        res = ( cigar_t * )malloc( sizeof * res );
        if ( res != NULL ) {
            init_cigar_t( res, c -> size );
            if ( res -> size == c -> size ) {
                size_t last;
                append_to_cigar_t( res, c -> op[ 0 ], c -> count[ 0 ] );
                for ( size_t i = 1; i < c -> length; ++i ) {
                    last = res -> length - 1;
                    if ( can_merge( c -> op[ i ], res -> op[ last ] ) ) {
                        res -> count[ last ] += c -> count[ i ];
                        res -> op[ last ] = 'M';
                    } else {
                        append_to_cigar_t( res, c -> op[ i ], c -> count[ i ] );
                    }
                }
            }
        }
    }
    return res;
}

static void append_base( char * buffer, size_t buf_len, size_t * buf_idx, int count, char c ) {
    int i;
    for ( i = 0; i < count && *buf_idx < buf_len; ++i ) {
        buffer[ (*buf_idx)++ ] = c;
    }
}

static void append_bases( char * buffer, size_t buf_len, size_t * buf_idx, int count,
                          const char * src, int src_len, int *src_idx )
{
    int i;
    for ( i = 0; i < count && *buf_idx < buf_len && *src_idx < src_len; ++i ) {
        buffer[ (*buf_idx)++ ] = src[ (*src_idx)++ ];
    }
}

size_t cigar_t_2_read( char * buffer, size_t buf_len,
                       const struct cigar_t * c, const char * ref_bases, const char * ins_bases )
{
    size_t res = 0;
    if ( buffer != NULL && buf_len > 0 && c != NULL ) {
        int readlen = cigar_t_readlen( c );
        if ( readlen > 0 ) {
            int needed_ref_bases = cigar_t_reflen( c );
            int available_ref_bases = ref_bases != NULL ? strlen( ref_bases ) : 0;
            if ( available_ref_bases >= needed_ref_bases ) {
                int needed_ins_bases = cigar_t_inslen( c );
                int available_ins_bases = ins_bases != NULL ? strlen( ins_bases ) : 0;    
                if ( available_ins_bases >= needed_ins_bases ) {
                    int ref_idx = 0;
                    int ins_idx = 0;
                    for ( size_t cigar_idx = 0; cigar_idx < c->length; ++cigar_idx ) {
                        int count = c -> count[ cigar_idx ];
                        switch ( c -> op[ cigar_idx ] )
                        {
                            case 'A' : append_base( buffer, buf_len, &res, count, 'A' );
                                       ref_idx += count;
                                       break;

                            case 'C' : append_base( buffer, buf_len, &res, count, 'C' );
                                       ref_idx += count;
                                       break;

                            case 'G' : append_base( buffer, buf_len, &res, count, 'G' );
                                       ref_idx += count;                                                
                                       break;

                            case 'T' : append_base( buffer, buf_len, &res, count, 'T' );
                                       ref_idx += count;                                                
                                       break;

                            case 'D' : ref_idx += count;
                                       break;
                            
                            case 'I' : append_bases( buffer, buf_len, &res, count,
                                                     ins_bases, available_ins_bases, &ins_idx );
                                       break;

                            case 'M' : append_bases( buffer, buf_len, &res, count,
                                                     ref_bases, available_ref_bases, &ref_idx );
                                       break;
                        }
                    }
                    if ( res < buf_len ) { buffer[ res ] = 0; }
                }
            }
        }
    }
    return res;
}

std::string purify_cigar( const std::string &cigar ) {
    std::string res;
    struct cigar_t * cigar_struct = make_cigar_t( cigar.c_str() );
    if ( cigar_struct != NULL )
    {
        char buffer[ 4096 ];
        struct cigar_t * merged_cigar = merge_cigar_t( cigar_struct );
        cigar_t_string( buffer, sizeof buffer, merged_cigar );
        res = buffer;
        free_cigar_t( merged_cigar );
        free_cigar_t( cigar_struct );
    }
    return res;
}

std::string apply_cigar_to_ref_and_generate_read( const std::string &cigar,
                                                  int refpos,
                                                  const std::string &ref_bases,
                                                  const std::string &ins_bases ) {
    std::string res;
    struct cigar_t * cigar_struct = make_cigar_t( cigar.c_str() );
    if ( cigar_struct != NULL ) {
        uint32_t reflen = cigar_t_reflen( cigar_struct );
        std::string ref_slice = ref_bases.substr( refpos - 1, reflen );
        if ( !ref_slice.empty() ) {
            int read_len = cigar_t_readlen( cigar_struct );
            char * buffer = ( char * )malloc( read_len + 1 );
            if ( buffer != NULL ) {
                cigar_t_2_read( buffer, read_len + 1, cigar_struct, ref_slice.c_str(), ins_bases.c_str() );
                res = buffer;
                free( buffer );
            }
        }
        free_cigar_t( cigar_struct );
    }
    return res;
}

uint32_t len_of_special_cigar( const std::string &cigar ) {
    uint32_t res = 0;
    struct cigar_t * cigar_struct = make_cigar_t( cigar.c_str() );
    if ( cigar_struct != NULL ) {
        res = cigar_t_reflen( cigar_struct );
        free_cigar_t( cigar_struct );        
    }
    return res;
}

/* -----------------------------------------------------------------
 * helper - functions
 * ----------------------------------------------------------------- */

bool is_numeric( const std::string &s ) {
    if ( s.empty() ) return false;
    return s.find_first_not_of( "0123456789" ) == std::string::npos;
}

int str_to_int( const std::string &s, int dflt ) {
    if ( is_numeric( s ) ) {
        std::stringstream ss;
        ss << s;
        int res;
        ss >> res;
        return res;
    }
    return dflt;
}

bool str_to_bool( const std::string &s ) {
    return( strcasecmp ( s.c_str (), "true" ) == 0 ||
            strcasecmp ( s.c_str (), "yes" ) == 0 ||                  
            atoi( s.c_str() ) != 0 );
}

int random_int( int min, int max ) {
    int spread = max - min;
    return min + rand() % spread;
}

std::string random_chars( int length, const std::string &charset ) {
    std::string result;
    result.resize( length );

    for ( int i = 0; i < length; i++ ) {
        result[ i ] = charset[ rand() % charset.length() ];
    }
    return result;
}

static std::string bases_charset = "ACGTN";
std::string random_bases( int length ) { return random_chars( length, bases_charset ); }

static std::string qual_charset = "!\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~";
static char min_qual_char = '!';
static char max_qual_char = '~';

std::string random_quality( int length ) { return random_chars( length, qual_charset ); }

std::string random_div_quality( int length, int max_div ) {
    std::string res;
    if ( length > 0 ) {
        res = random_chars( 1, qual_charset );
        if ( length > 1 ) {
            for ( int i = 1; i < length; i++  ) {
                int diff = random_int( -max_div, max_div );
                char c = res[ i - 1 ] + diff;
                if ( c < min_qual_char ) {
                    c = min_qual_char;
                } else if ( c > max_qual_char ) {
                    c = max_qual_char;
                }
                res += c;
            }
        }
    }
    return res;
}

static std::string string_charset = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890";
std::string random_string( int length ) { return random_chars( length, string_charset ); }

std::string pattern_quality( std::string &pattern, size_t len ) {
    std::string res;
    while ( res.length() < len ) { res += pattern; }
    if ( res.length() > len ) { res = res.substr( 0, len ); }
    return res;
}

/* -----------------------------------------------------------------
 * Parameters ( of a prog-line )
 * ----------------------------------------------------------------- */

class t_params;
typedef std::shared_ptr< t_params > t_params_ptr;

class t_params {
    private :
        std::map< std::string, std::string > values;

        void split_kv( const std::string &kv ) {
            int eq_pos = kv.find( '=' );
            if ( eq_pos != -1 ) {
                std::string key = kv.substr( 0, eq_pos );
                std::transform( key.begin(), key.end(), key.begin(), ::tolower);
                std::string value = kv.substr( eq_pos + 1, kv.length() );
                values.insert( { key, value } );                
            } else {
                values.insert( { kv, "" } );
            }
        }

        t_params( const std::string &line ) : values( {} ) {
            int colon_pos = line.find( ':' );
            if ( colon_pos != -1 ) {
                std::string rem = line.substr( colon_pos + 1, line.length() );
                int start = 0;
                int end = rem.find( ',' );
                while ( end != -1 ) {
                    std::string param = rem.substr( start, end - start );
                    split_kv( param );
                    start = end + 1;
                    end = rem.find( ',', start );
                }
                std::string param = rem.substr( start, end - start );
                split_kv( param );                
            }
        }

    public :
        static t_params_ptr make( const std::string &line ) {
            return t_params_ptr( new t_params( line ) );
        }

        const std::string& get_string( const std::string& dflt ) {
            auto entry = values . begin();
            if ( entry != values . end() ) { return entry -> first; }
            return dflt;
        }
        
        const std::string& get_string_key( const std::string& key, const std::string& dflt ) const {
            auto found = values . find( key );
            if ( found != values . end() ) { return found -> second; }
            return dflt;
        }
};

/* -----------------------------------------------------------------
 * the prog-line
 * ----------------------------------------------------------------- */

class t_progline;
typedef std::shared_ptr< t_progline > t_progline_ptr;
typedef std::vector< t_progline_ptr > t_proglines;

class t_progline {
    private :
        const char * filename;
        int line_nr;
        std::string org;
        std::string cmd;
        t_params_ptr kv;
        
        t_progline( const std::string &line, const char * a_file_name, int a_line_nr )
            : filename( a_file_name ), line_nr( a_line_nr ), org( line ), 
              cmd(), kv( t_params::make( line ) ) {
            int colon_pos = line.find( ':' );
            if ( colon_pos != -1 ) {
                cmd = line.substr( 0, colon_pos );
                std::transform( cmd.begin(), cmd.end(), cmd.begin(), ::tolower );
            }
        }

    public :
        static t_progline_ptr make( const std::string &line, const char * filename, int line_nr ) {
            return t_progline_ptr( new t_progline( line, filename, line_nr ) );
        }
        
        bool is_ref( void ) const { return cmd == "ref" || cmd == "r"; }
        bool is_ref_out( void ) const { return cmd == "ref-out"; }
        bool is_sam_out( void ) const { return cmd == "sam-out"; }
        bool is_dflt_cigar( void ) const { return cmd == "dflt-cigar"; }
        bool is_dflt_mapq( void ) const { return cmd == "dflt-mapq"; }
        bool is_dflt_qdiv( void ) const { return cmd == "dflt-qdiv"; }
        bool is_config( void ) const { return cmd == "config"; }
        bool is_prim( void ) const { return cmd == "prim" || cmd == "p"; }
        bool is_sec( void ) const { return cmd == "sec" || cmd == "s"; }
        bool is_unaligned( void ) const { return cmd == "unalig" || cmd == "u"; }        
        bool is_link( void ) const { return cmd == "lnk" || cmd == "l"; }
        bool is_sort_alignments( void ) const { return cmd == "sort"; }

        std::string get_org( void ) const { 
            std::stringstream ss;
            ss << filename << "#" << line_nr << " : " << org;
            return ss.str();
            
        }

        const std::string& get_string( const std::string& dflt ) {
            return kv -> get_string( dflt );
        }

        int get_int( int dflt ) {
            auto value = kv -> get_string( "" );
            if ( value == "" ) { return dflt; }
            return str_to_int( value, dflt );
        }

        int get_bool( bool dflt ) {
            auto value = kv -> get_string( "" );
            if ( value . empty() ) { return dflt; }
            return str_to_bool( value );
        }

        const std::string& get_string_key( const std::string& key, const std::string& dflt ) const {
            return kv -> get_string_key( key, dflt );
        }

        bool get_bool_key( const std::string& key, bool dflt ) const {
            auto value = kv -> get_string_key( key, "" );
            if ( value . empty() ) { return dflt; }
            return str_to_bool( value );
        }
        
        int get_int_key( const std::string& key, int dflt ) const {
            auto value = kv -> get_string_key( key, "" );
            if ( value == "" ) { return dflt; }
            return str_to_int( value, dflt );
        }
        
        static void consume_stream( std::istream &stream, const char * file_name, t_proglines& proglines ) {
            int line_nr = 0;
            for ( std::string line; std::getline( stream, line ); ) {
                if ( !( 0 == line.find( '#' ) ) ) {
                    proglines . push_back( t_progline::make( line, file_name, line_nr ) );
                }
                line_nr++;
            }
        }
        
        static void consume_lines( int argc, char *argv[], t_proglines& proglines ) {
            if ( argc > 1 ) {
                // looping through the file( s ) given at the commandline...
                for ( int i = 1; i < argc; i++ ) {
                    std::fstream inputfile;
                    inputfile.open( argv[ i ], std::ios::in );
                    if ( inputfile.is_open() ) {
                        t_progline::consume_stream( inputfile, argv[ 1 ], proglines );
                        inputfile.close();
                    }
                }
            } else {
                // reading from stdin
                consume_stream( std::cin, "stdin", proglines );
            }
        }
};

/* -----------------------------------------------------------------
 * a list of errors
 * ----------------------------------------------------------------- */

class t_errors {
    private :
        std::vector< std::string > errors;
    
    public :
        const bool empty( void ) const { return errors.empty(); }

        void msg( const char * msg, const std::string &org ) {
            std::stringstream ss;
            ss << msg << org;
            errors . push_back( ss.str() );
        }
        
        bool print( void ) const {
            for ( const std::string &err : errors ) {
                std::cerr << err << std::endl;
            }
            return ( errors.size() == 0 );
        }
};

/* -----------------------------------------------------------------
 * Reference
 * ----------------------------------------------------------------- */

class t_reference;
typedef std::shared_ptr< t_reference > t_reference_ptr;
typedef std::map< std::string, t_reference_ptr > t_reference_map;
        
class t_reference {
    private :
        std::string name;
        std::string alias;
        std::string bases;

        t_reference ( const std::string &a_name, const std::string &a_alias ) 
            : name( a_name ), alias( a_alias ) { }
        
    public :
        static t_reference_ptr make( const std::string &a_name, const std::string &a_alias ) {
            return t_reference_ptr( new t_reference( a_name, a_alias ) );
        }

        static t_reference_ptr make( const std::string &a_name ) {
            return t_reference_ptr( new t_reference( a_name, a_name ) );
        }

        void make_random( int length ) { bases = random_bases( length ); }
        void from_string( const std::string &a_bases ) { bases = a_bases; }
        void write_fasta( std::ofstream &out ) const { out << ">" << name << std::endl << bases << std::endl; }
        void write_config( std::ofstream &out ) const { out << alias << "\t" << name << std::endl; }
        int length( void ) const { return bases.length(); }
        const std::string& get_bases( void ) const { return bases; }
        const std::string& get_alias( void ) const { return alias; }
        const std::string& get_name( void ) const { return name; }
        
        void print_HDR( std::ostream &out ) const {
            int l = length();
            out << "@SQ\tSN:" << name << "\tAS:n/a" << "\tLN:" << l << "\t";
            out << "UR:file:rand_ref.fasta";
            out << std::endl;
        }
  
        void static insert_ref( const t_reference_ptr ref, t_reference_map& references, t_errors& errors ) {
            const std::string& alias = ref -> get_alias();
            auto found = references . find( alias );
            if ( found == references . end() ) {
                references . insert( std::pair< std::string, t_reference_ptr >( alias, ref ) );
            } else {
                errors . msg( "duplicate reference: ", alias );
            }
        }

        static void random_ref( const t_progline_ptr pl, t_reference_map& refs, t_errors& errors ) {
            const std::string& name = pl -> get_string_key( "name", "" );
            if ( name . empty() ) {
                errors . msg( "name missing in: ", pl -> get_org() );                
            } else {
                int length = pl -> get_int_key( "length", 10000 );
                auto ref = t_reference::make( name );
                ref -> make_random( length );
                t_reference::insert_ref( ref, refs, errors );
            }
        }

        static void insert_name_and_bases( const std::string &name, const std::string &bases,
            t_reference_map& refs, t_errors& errors ) {
            if ( !( name . empty() ) && !( bases . empty() ) )
            {
                auto ref = t_reference::make( name );
                ref ->  from_string( bases );
                t_reference::insert_ref( ref, refs, errors );
            }
        }

        static void fasta_ref( const t_progline_ptr pl, t_reference_map& refs, t_errors& errors ) {
            const std::string& file_name = pl -> get_string_key( "file", "" );
            if ( file_name . empty() ) {
                errors . msg( "file_name missing in: ", pl -> get_org() );
            } else {
                std::fstream input;
                input.open( file_name, std::ios::in );
                if ( input.is_open() ) {
                    std::string ref_name;
                    std::string bases;
                    for ( std::string line; std::getline( input, line ); ) {
                        if ( line[ 0 ] == '>' ) {
                            t_reference::insert_name_and_bases( ref_name, bases, refs, errors );
                            ref_name = line.substr( 1, line.length() - 1 );
                            bases = "";
                        } else {
                            bases += line;
                        }
                    }
                    insert_name_and_bases( ref_name, bases, refs, errors );
                    input.close();
                } else {
                    errors . msg( "cannot open: ", pl -> get_org() );                    
                }
            }
        }

        static void refseq_ref( const t_progline_ptr pl, t_reference_map& refs, t_errors& errors ) {
            errors . msg( "not implemented yet: ", pl -> get_org() );
        }

        static std::string get_first_ref_alias( const t_reference_map& refs ) {
            auto ref0 = refs.begin();
            if ( ref0 == refs.end() ) { return ""; }
            return ref0 -> first;
        }

        static void write_reference_file( const std::string& filename, const t_reference_map& refs,
                                          t_errors& errors ) {
            if ( ! filename . empty() ) {
                if ( refs . empty() ) {
                    errors . msg( "no references - cannot write reference-file: ", filename );
                } else {
                    std::ofstream out( filename );
                    auto iter = refs . begin();
                    while ( iter != refs . end() ) {
                        iter -> second -> write_fasta( out );
                        iter++;
                    }
                    out.close();
                }
            }
        }

        static void write_config_file( const std::string& filename, const t_reference_map& refs,
                                       t_errors& errors ) {
            if ( ! filename . empty() ) {
                if ( refs.empty() ) {
                    errors . msg( "no references - cannot write config-file: ", filename );
                } else {
                    std::ofstream out( filename );
                    auto iter = refs . begin();
                    while ( iter != refs . end() ) {
                        iter -> second ->  write_config( out );
                        iter++;
                    }
                    out.close();
                }
            }
        }
        
        static const t_reference_ptr lookup( const std::string &alias, const t_reference_map& refs ) {
            auto found = refs . find( alias );
            if ( found == refs . end() ) { return nullptr; }
            return found -> second;
        }

};

/* -----------------------------------------------------------------
 * the alignment
 * ----------------------------------------------------------------- */

class t_alignment;
typedef std::shared_ptr< t_alignment > t_alignment_ptr;
typedef std::map< std::string, std::vector< t_alignment_ptr > > t_alignment_bins;
typedef std::vector< t_alignment_ptr > t_alignment_vec;

class t_alignment {
    private :
        std::string name;
        int flags;
        const t_reference_ptr ref;
        int ref_pos;
        int mapq;
        std::string special_cigar;
        std::string pure_cigar;
        t_alignment_ptr mate;
        int tlen;
        std::string seq;
        std::string qual;
        std::string opts;
        std::string ins_bases;

        // ctor for PRIM/SEC alignments
        t_alignment( const std::string &a_name, int a_flags, const t_reference_ptr a_ref, int a_pos,
                   int a_mapq, const std::string &a_cigar, int a_tlen, const std::string &a_qual,
                   const std::string &a_opts, const std::string &a_ins_bases, int qual_div )
            : name( a_name ), flags( a_flags ), ref( a_ref ), ref_pos( a_pos ), mapq( a_mapq ),
              special_cigar( a_cigar ), pure_cigar( purify_cigar( a_cigar ) ),
              mate( nullptr ), tlen( a_tlen ),
              seq( "" ), qual( a_qual ), opts( a_opts ), ins_bases( a_ins_bases ){
            if ( ref_pos == 0 ) { 
                ref_pos = random_int( 1, ref -> length() );
                size_t spec_cig_len = len_of_special_cigar( special_cigar );
                size_t rlen = ref -> length();
                if ( ref_pos + spec_cig_len >= rlen ) {
                    ref_pos = ref -> length() - ( spec_cig_len + 1 );
                }
            }
            seq = apply_cigar_to_ref_and_generate_read( special_cigar, ref_pos, ref -> get_bases(), ins_bases );
            set_quality( qual_div );
        }

        // ctor for UNALIGNED alignments
        t_alignment( const std::string &a_name, int a_flags, const std::string &a_seq,
                   const std::string &a_qual, const std::string &a_opts, int qual_div )
            : name( a_name ), flags( a_flags ), ref( nullptr ), ref_pos( 0 ), mapq( 0 ),
              special_cigar( "" ), pure_cigar( "*" ), mate( nullptr ),
              tlen( 0 ), seq( a_seq ), qual( a_qual ), opts( a_opts ), ins_bases( "" ) {
            set_quality( qual_div );
        }

        void set_quality( int qual_div ) {
            size_t seq_len = seq.length();
            size_t qual_len = qual.length();
            if ( 0 == qual_len ) {
                // if no quality given: generate random quality the same length as seq
                if ( 0 == qual_div ) {
                    qual = random_quality( seq_len );
                } else {
                    qual = random_div_quality( seq_len, 10 );
                }
            } else if ( "*" == qual || qual_len == seq_len ) {
                // if qual is "*" or the same length as seq : do nothing - keep it
            } else {
                // generate qual by repeating the pattern given until seq_len is reached
                qual = pattern_quality( qual, seq_len );
            }
        }

        void print_opts( std::ostream &out ) const {
            std::stringstream ss( opts );
            std::string elem;
            while ( std::getline( ss, elem, ' ' ) ) {
                // trim white-space
                elem . erase( std::remove( elem . begin(), elem.end(), ' ' ), elem.end() );
                if ( !elem.empty() ) {
                    out << "\t" << elem;
                }
            }
        }
        
    public :
        static t_alignment_ptr make( const std::string &name, int flags, const t_reference_ptr ref, int pos,
                    int mapq, const std::string &cigar, int tlen, const std::string &qual,
                    const std::string &opts, const std::string &ins_bases, int qual_div ) {
            return t_alignment_ptr( new t_alignment( name, flags, ref, pos, mapq, cigar,
                                                     tlen, qual, opts, ins_bases, qual_div ) );
        }
        
        static t_alignment_ptr make( const std::string &name, int flags, const std::string &seq,
                                   const std::string &qual, const std::string &opts, int qual_div ) {
            return t_alignment_ptr( new t_alignment( name, flags, seq, qual, opts, qual_div ) );
        }
        
        bool operator<( const t_alignment& other ) const { return ref_pos < other . ref_pos; }

        std::string refname( void ) const {
            if ( ref != nullptr ) { return ref -> get_alias(); } else { return "*"; }
        }
        
        void print_SAM( std::ostream &out ) const {
            out << name << "\t" << flags << "\t" << refname() << "\t";
            out << ref_pos << "\t" << mapq << "\t" << pure_cigar << "\t";
            if ( mate != nullptr ) {
                out << mate -> refname() << "\t" << mate -> ref_pos << "\t";
            } else {
                out << "*\t0\t";
            }
            out << tlen << "\t";
            out << ( ( seq.length() > 0 ) ? seq : "*" );
            out << "\t";
            out << ( ( qual.length() > 0 ) ? qual : "*" );
            if ( opts.length() > 0 ) { print_opts( out ); }
            out << std::endl;
        }
        
        bool has_flag( int mask ) const { return ( flags & mask ) == mask; }
        bool has_ref( void ) const { return ref != nullptr; }
        const std::string& get_name( void ) const { return name; }
        
        void set_mate( t_alignment_ptr a_mate ) {
            mate = a_mate;
            set_flag( FLAG_NEXT_UNMAPPED, mate -> has_flag( FLAG_UNMAPPED ) );
            set_flag( FLAG_NEXT_REVERSED, mate -> has_flag( FLAG_REVERSED ) );
            set_flag( FLAG_MULTI, true );
        }
        
        void set_flag( int flagbit, bool state ) {
            if ( state ) {
                flags |= flagbit;
            } else {
                flags &= ~flagbit;
            }
        }
};

//we need that because we have a vector of smart-pointers - not objects, to sort...
class t_alignment_comparer {
    public :
        bool operator()( const t_alignment_ptr& a, const t_alignment_ptr& b ) {
            return *a < *b && !(*b < *a);    
        }    
};

/* -----------------------------------------------------------------
 * the alignment-group ( binned by name )
 * ----------------------------------------------------------------- */

class t_alignment_group;
typedef std::shared_ptr< t_alignment_group > t_alignment_group_ptr;
typedef std::map< std::string, t_alignment_group_ptr > t_alignment_group_map;
        
class t_alignment_group {
    private :
        std::string name;
        t_alignment_vec prim_alignments;
        t_alignment_vec sec_alignments;
        t_alignment_vec unaligned;
        
        t_alignment_group( std::string a_name ) : name( a_name ) {}

    public :
        static t_alignment_group_ptr make( t_alignment_ptr a ) { 
            t_alignment_group_ptr ag( new t_alignment_group( a -> get_name() ) );
            ag -> add( a );
            return ag;
        }

        void add( t_alignment_ptr a ) {
            if ( a -> has_flag( FLAG_UNMAPPED ) ) {
                unaligned . push_back( a );
            } else if ( a -> has_flag( FLAG_SECONDARY ) ) {
                sec_alignments . push_back( a );
            } else {
                prim_alignments . push_back( a );
            }
        }

        void print_SAM( std::ostream &out ) const {
            for ( const t_alignment_ptr a : prim_alignments ) { a -> print_SAM( out ); }
            for ( const t_alignment_ptr a : sec_alignments ) { a -> print_SAM( out ); }
            for ( const t_alignment_ptr a : unaligned ) { a -> print_SAM( out ); }            
        }

        static void finish_alignmnet_vector( t_alignment_vec& v ) {
            int cnt = v.size();
            if ( cnt > 1 ) {
                int idx = 0;
                for( t_alignment_ptr a : v ) { 
                    if ( idx == 0 ) {
                        // first ...
                        a -> set_mate( v[ 1 ] );
                        a -> set_flag( FLAG_FIRST, true );
                        a -> set_flag( FLAG_LAST, false );
                    } else if ( idx == cnt -1 ) {
                        // last ...
                        a -> set_mate( v[ 0 ] );
                        a -> set_flag( FLAG_FIRST, false );
                        a -> set_flag( FLAG_LAST, true );
                    } else {
                        // in between
                        a -> set_mate( v[ idx + 1 ] );
                        a -> set_flag( FLAG_FIRST, false );
                        a -> set_flag( FLAG_LAST, false );
                    }
                    idx++;
                }
            } else if ( cnt == 1 ) {
                v[ 0 ] -> set_flag( FLAG_MULTI, false );
            }
        }

        void finish_alignments( void ) {
            t_alignment_group::finish_alignmnet_vector( prim_alignments );
            t_alignment_group::finish_alignmnet_vector( sec_alignments );            
        }

        void bin_alignment_by_ref( t_alignment_ptr a, t_alignment_bins &by_ref, t_alignment_vec &without_ref ) {
            if ( a -> has_ref() ) {
                std::string name = a -> refname();
                auto bin = by_ref . find( name );
                if ( bin == by_ref . end() ) {
                    // no : we have to make one ...
                    t_alignment_vec v{ a };
                    by_ref . insert( std::pair< std::string, std::vector< t_alignment_ptr > >( name, v ) );                                
                } else {
                    // yes : just add it
                    bin -> second . push_back( a );
                }
                
            } else {
                // has no reference, comes at the end...
                without_ref . push_back( a );
            }
        }
        
        void bin_by_refname( t_alignment_bins &by_ref, t_alignment_vec &without_ref ) {
            for ( const auto a : prim_alignments ) { bin_alignment_by_ref( a, by_ref, without_ref ); }
            for ( const auto a : sec_alignments ) { bin_alignment_by_ref( a, by_ref, without_ref ); }
            for ( const auto a : unaligned ) { without_ref . push_back( a ); }
        }
        
        static void insert_alignment( t_alignment_ptr a, t_alignment_group_map& alignment_groups ) {
            const std::string& name = a -> get_name();
            auto found = alignment_groups . find( name );
            if ( found == alignment_groups . end() ) {
                alignment_groups . insert( 
                    std::pair< std::string, t_alignment_group_ptr >( name, t_alignment_group::make( a ) ) );
            } else {
                ( found -> second ) -> add( a );
            }
        }

};

class t_settings {
    private :
        std::string ref_out;
        std::string sam_out;
        std::string config;
        std::string dflt_alias;
        std::string dflt_cigar;
        int dflt_mapq;
        int dflt_qdiv;
        bool sort_alignments;
        
        void set_string( const t_progline_ptr pl, const char * msg, std::string *out,
                         t_errors & errors ) {
            const std::string& fn = pl -> get_string( "name" );
            if ( fn.empty() ) {
                errors . msg( msg, pl -> get_org() );
            } else {
                *out = fn;
            }
        }
        
    public :
        t_settings( const t_proglines& proglines, t_errors & errors ) 
            : dflt_cigar( "30M" ), dflt_mapq( 20 ), dflt_qdiv( 0 ), sort_alignments( true ) {
            for ( const t_progline_ptr pl : proglines ) {
                if ( pl -> is_ref_out() ) {
                    set_string( pl, "missing ref-file-name in: ", &ref_out, errors );
                } else if ( pl -> is_sam_out() ) {
                    set_string( pl, "missing sam-file-name in: ", &sam_out, errors );
                } else if ( pl -> is_config() ) {
                    set_string( pl, "missing config-file-name in: ", &config, errors );
                } else if ( pl -> is_dflt_mapq() ) {
                    dflt_mapq = pl -> get_int( dflt_mapq );
                } else if ( pl -> is_dflt_qdiv() ) {
                    dflt_qdiv = pl -> get_int( dflt_qdiv );                    
                } else if ( pl -> is_dflt_cigar() ) {
                    set_string( pl, "missing value in: ", &dflt_cigar, errors );
                } else if ( pl -> is_sort_alignments() ) {
                    sort_alignments = pl -> get_bool( sort_alignments );
                }
            }
        }

        const std::string& get_ref_out( void ) const { return ref_out; }
        const std::string& get_sam_out( void ) const { return sam_out; }
        const std::string& get_config( void ) const { return config; }
        const std::string& get_dflt_alias( void ) const { return dflt_alias; }
        void set_dflt_alias( std::string alias ) { dflt_alias = alias; }
        const std::string& get_dflt_cigar( void ) const { return dflt_cigar; }
        int get_dflt_mapq( void ) { return dflt_mapq; }
        int get_dflt_qdiv( void ) { return dflt_qdiv; }
        bool get_sort_alignments( void ) { return sort_alignments; }
};

class t_factory {
    private :
        const t_proglines& proglines;
        t_reference_map refs;
        t_errors& errors;
        t_settings settings;
        t_alignment_group_map alignment_groups;
        
        bool phase1( void ) {
            // populate references...
            for ( const t_progline_ptr pl : proglines ) {
                if ( pl -> is_ref() ) {
                    const std::string& t = pl -> get_string_key( "type", "" );
                    if ( t == "random" ) { 
                        t_reference::random_ref( pl, refs, errors );
                    } else if ( t == "fasta" ) {
                        t_reference::fasta_ref( pl, refs, errors );
                    } else if ( t == "refseq" ) {
                        t_reference::refseq_ref( pl, refs, errors );
                    } else {
                        errors . msg( "unknown type in: ", pl -> get_org() );
                    }
                }
            }

            // generate error if we do not have any references at this point
            if ( refs . empty() ) { errors . msg( "no references found!", "" ); }
            
            // write a reference-fasta-file ( if requested )
            if ( errors . empty() ) {
                settings . set_dflt_alias( t_reference::get_first_ref_alias( refs ) );
                t_reference::write_reference_file( settings.get_ref_out(), refs, errors );
                t_reference::write_config_file( settings.get_config(), refs, errors );
            }

            // write reference-file ( in case of random-type )
            return errors . empty();
        }

        int get_flags( const t_progline_ptr pl, int type_flags ) const {
            int flags = pl -> get_int_key( "flags", 0 ) | type_flags;
            if ( pl -> get_bool_key( "reverse", false ) ) { flags |= FLAG_REVERSED; }
            if ( pl -> get_bool_key( "bad", false ) ) { flags |= FLAG_BAD; }
            if ( pl -> get_bool_key( "pcr", false ) ) { flags |= FLAG_PCR; }
            return flags;
        }

        void generate_single_align( const t_progline_ptr pl, const std::string &name,
                                 const t_reference_ptr ref, int flags, int qual_div ) {
            t_alignment_group::insert_alignment(
                t_alignment::make( name,
                                flags,
                                ref, 
                                pl -> get_int_key( "pos", 0 ),
                                pl -> get_int_key( "mapq", settings.get_dflt_mapq() ),
                                pl -> get_string_key( "cigar", settings.get_dflt_cigar() ),
                                pl -> get_int_key( "tlen", 0 ),
                                pl -> get_string_key( "qual", "" ),
                                pl -> get_string_key( "opts", "" ),
                                pl -> get_string_key( "ins", "" ),
                                qual_div ),
                alignment_groups );
        }

        void generate_multiple_align( const t_progline_ptr pl, const std::string &base_name,
                                      const t_reference_ptr ref, int flags, int repeat, int qual_div ) {
            int pos = pl -> get_int_key( "pos", 0 );
            int mapq = pl -> get_int_key( "mapq", settings.get_dflt_mapq() );
            const std::string& cigar = pl -> get_string_key( "cigar", settings.get_dflt_cigar() );
            int tlen = pl -> get_int_key( "tlen", 0 );
            const std::string& qual = pl -> get_string_key( "qual", "" );
            const std::string& opts = pl -> get_string_key( "opts", "" );
            const std::string& ins_bases = pl -> get_string_key( "ins", "" );
            for ( int i = 0; i < repeat; i++ ) {
                std::ostringstream os;
                os << base_name << "_" << i;
                t_alignment_group::insert_alignment(
                    t_alignment::make( os.str(), flags, ref, pos, mapq, cigar, tlen, qual,
                                       opts, ins_bases, qual_div ),
                    alignment_groups );
            }
        }

        void generate_align( const t_progline_ptr pl, int type_flags ) {
            const std::string& name = pl -> get_string_key( "name", "" );
            if ( name.empty() ) {
                errors . msg( "missing name in: ", pl -> get_org() );
            } else {
                const std::string& ref_alias = pl -> get_string_key( "ref", settings.get_dflt_alias() );
                const t_reference_ptr ref = t_reference::lookup( ref_alias, refs );
                if ( ref == nullptr ) {
                    errors . msg( "cannot find reference: ", pl -> get_org() );                    
                } else {
                    int repeat = pl -> get_int_key( "repeat", 0 );
                    int qual_div = pl -> get_int_key( "qdiv", settings.get_dflt_qdiv() );
                    if ( repeat == 0 ) {
                        generate_single_align( pl, name, ref, get_flags( pl, type_flags ), qual_div );
                    } else {
                        generate_multiple_align( pl, name, ref, get_flags( pl, type_flags ), repeat, qual_div );
                    }
                }
            }
        }

        void generate_unaligned( const t_progline_ptr pl ) {
            const std::string& name = pl -> get_string_key( "name", "" );
            if ( name . empty() ) {
                errors . msg( "missing name in: ", pl -> get_org() );
            } else {
                int flags = pl -> get_int_key( "flags", 0 ) | FLAG_UNMAPPED;
                int qual_div = pl -> get_int_key( "qdiv", settings.get_dflt_qdiv() );
                std::string seq = pl -> get_string_key( "seq", "" ); // not const ref, because we may overwrite it
                if ( seq.empty() ) {
                    int len = pl -> get_int_key( "len", 0 );
                    if ( len == 0 ) {
                        errors . msg( "missing seq or len in: ", pl -> get_org() );
                    } else {
                        seq = random_bases( len );
                    }
                }
                t_alignment_group::insert_alignment(
                    t_alignment::make( name, flags, seq, pl -> get_string_key( "qual", "" ),
                                        pl -> get_string_key( "opts", "" ), qual_div ),
                    alignment_groups );
            }
        }

        bool phase2( void ) {
            // handle the different line-types prim|sec||unaligned
            for ( const t_progline_ptr pl : proglines ) {
                if ( pl -> is_prim() ) {
                    generate_align( pl, FLAG_PROPPER );
                } else if ( pl -> is_sec() ) {
                    generate_align( pl, FLAG_SECONDARY );
                } else if ( pl -> is_unaligned() ) {
                    generate_unaligned( pl );
                }
            }
            return errors . empty();
        }

        void sort_and_print( std::ostream &out ) {
            t_alignment_bins by_ref;
            t_alignment_vec without_ref;

            // bin the alignments by reference
            for( const auto &ag : alignment_groups ) {
                ag . second -> bin_by_refname( by_ref, without_ref );
            }
            
            // sort each reference...
            for ( auto &rb : by_ref ) {
                sort( rb . second . begin(), rb . second . end(), t_alignment_comparer() );
            }
            
            // produce the SAM-output...
            for ( const auto &rb : by_ref ) {
                for ( const auto &a : rb . second ) { a -> print_SAM( out ); }
            }
            for ( const auto &a : without_ref ) { a -> print_SAM( out ); }
        }

        void print_all( std::ostream &out ) {
            out << "@HD" << "\t" <<	"VN:1.0" << "\t" <<	"SO:coordinate" << std::endl;
            for ( const auto &ref : refs ) {
                ref . second -> print_HDR( out );
            }
            out << "@RG\tID:default" << std::endl;

            if ( settings.get_sort_alignments() ) {
                sort_and_print( out );
            } else {
                for( const auto &ag : alignment_groups ) {
                    ag . second -> print_SAM( out );
                }
            }
        }

        bool phase3( void ) {
            // set flags / next_ref / next_pos
            for( auto ag : alignment_groups ) { ag . second -> finish_alignments(); }
            
            // finally produce SAM-output
            if ( errors.empty() ) {
                // if sam-out-filename is given, create that file and write SAM into it,
                // otherwise write SAM to stdout
                const std::string& sam_out = settings.get_sam_out();
                if ( sam_out . empty() ) {
                    print_all( std::cout );
                } else {
                    std::ofstream out( sam_out );
                    print_all( out );
                }
            }
            return errors . empty();
        }

    public :
        t_factory( const t_proglines& lines, t_errors& error_list )
            : proglines( lines ), errors( error_list ), settings( lines, error_list ) {}
        
        int produce( void ) {
            if ( phase1() ) {
                if ( phase2() ) {
                    if ( phase3() ) {
                        return 0;
                    }
                }
            }
            return 1;
        }
};

int main( int argc, char *argv[] ) {
    int res = 3;
    t_proglines proglines;

    t_progline::consume_lines( argc, argv, proglines );
    if ( !proglines.empty() ) {
        t_errors errors;
        t_factory factory( proglines, errors );
        res = factory . produce();
        errors . print();
    }
    return res;
}
