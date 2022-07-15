#pragma once

#include <vector>
#include <fstream>
#include "tools.hpp"

class RR_Ofs_result {
    public:
        RR_Ofs_result( void ) : _valid( false ), _ofs( 0 ) {}
        RR_Ofs_result( uint64_t ofs ) : _valid( true ), _ofs( ofs ) {}
        bool is_valid() const { return _valid; }
        uint64_t ofs() const { return _valid ? _ofs : 0; }
    private:
        bool _valid;
        uint64_t _ofs;
};

class RR_Name_result {
    public:
        RR_Name_result() : _valid( false ), _name( empty ) {}
        RR_Name_result( const std::string& name ) : _valid( true ), _name( name ) {}
        bool is_valid() const { return _valid; }
        const std::string& name() const { return _name; }

    private:
        const bool _valid;
        const std::string& _name;
        static std::string const empty;
};

class RR_Idx_result {
    public:
        RR_Idx_result( void ) : f_valid( false ), f_idx( 0 ) {}
        RR_Idx_result( uint32_t idx ) : f_valid( true ), f_idx( idx ) {}
        bool is_valid() const { return f_valid; }
        uint32_t idx() const { return f_valid ? f_idx : 0; }
    private:
        bool f_valid;
        uint32_t f_idx;
};

class RR_Entry {
    public:
        RR_Entry( const std::string &line ) {
            std::istringstream fields( line );
            try {
                fields >> f_name >> f_start >> f_end;
                f_valid = true;
            } catch ( ... ) {
                f_valid = false;
            }
        }
        
        std::string to_string() const {
            if ( f_valid ) {
                std::ostringstream line;
                line << f_name << " : " << f_start << " ... "<< f_end;
                return line.str();
            } else {
                return std::string();
            }
        }

        bool contains( uint64_t pos ) const {
            if ( f_valid ) {
                return pos >= f_start && pos <= f_end;
            } else {
                return false;
            }
        }

        RR_Ofs_result ofs( uint64_t pos ) const { 
            if ( f_valid && contains( pos ) ) {
                return RR_Ofs_result( pos - f_start );
            } else {
                return RR_Ofs_result();
            }
        }

        RR_Name_result name( void ) const {
            if ( f_valid ) {
                return RR_Name_result( f_name );
            } else {
                return RR_Name_result();
            }
        }

        bool valid( void ) { return f_valid; }

    private:
        bool f_valid;
        uint64_t f_start, f_end;
        std::string f_name;        
};

typedef std::vector< RR_Entry > entries_t;

class RR_Lookup {

    class line_event : public tools::iter_func {
        public:
            line_event( entries_t& entries ) : f_entries( entries ), f_invalid( 0 ) {}

            virtual void operator()( uint64_t id, const std::string& s ) {
                RR_Entry e( s );
                if ( e.valid() ) {
                    f_entries.push_back( e );
                } else {
                    f_invalid++;
                }
            }

            bool valid( void ) { return f_invalid == 0; }

        private:
            entries_t& f_entries;
            uint32_t f_invalid;
    };

    public:
        RR_Lookup( const char* filename ) {
            std::ifstream is( filename );
            line_event e( f_entries );
            tools::for_each_line( is, e );
            f_valid = e.valid();
        }

        std::string to_string() const {
            std::ostringstream line;
            for ( auto &entry : f_entries ) {
                line << entry.to_string() << std::endl;
            }
            return line.str();
        }

        RR_Idx_result find( uint64_t pos ) const {
            uint32_t idx = 0;
            for ( auto &entry : f_entries ) {
                if ( entry.contains( pos ) ) return RR_Idx_result( idx );
                idx++;
            }
            return RR_Idx_result();
        }

        RR_Name_result name_at_idx( uint32_t idx ) {
            if ( idx >= f_entries.size() ) {
                RR_Name_result res;
                return res;
            } else {
                RR_Name_result res( f_entries[ idx ].name() );
                return res;
            }
        }
        
        // idx is more of a hint to avoid looping every time...
        RR_Ofs_result ofs_at_idx( uint32_t idx, uint64_t pos ) const {
            if ( idx >= f_entries.size() ) {
                return RR_Ofs_result();
            } else {
                RR_Ofs_result res = f_entries[ idx ].ofs( pos );
                if ( !res.is_valid() ) {
                    RR_Idx_result idx_res = find( pos );
                    if ( idx_res.is_valid() ) {
                        res = f_entries[ idx_res.idx() ].ofs( pos );                        
                    } else {
                        res = RR_Ofs_result();
                    }
                }
                return res;
            }
        }
        
        bool valid( void ) { return f_valid; }

    private :
        entries_t f_entries;
        bool f_valid;

};
