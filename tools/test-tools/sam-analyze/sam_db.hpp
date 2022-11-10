#ifndef SAM_DB_H
#define SAM_DB_H

#include <memory>
#include <algorithm>
#include <vector>
#include "database.hpp"

class SAM_DB : public mt_database::DB {
    private :
        int status;
        mt_database::PREP_STM_PTR ins_ref_stm;
        mt_database::PREP_STM_PTR ins_hdr_stm;
        mt_database::PREP_STM_PTR ins_alig_stm;
        
        SAM_DB( const SAM_DB& ) = delete;
        
        int16_t make_ref_hdr_tbl( void ) {
            const char * stm = "CREATE TABLE IF NOT EXISTS REF( " \
                "ID INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL," \
                "NAME TEXT NOT NULL UNIQUE," \
                "LENGTH INT NOT NULL, " \
                "OTHER TEXT NOT NULL );";
            return exec( stm );
        }

        int16_t make_hdr_tbl( void ) {
            const char * stm = "CREATE TABLE IF NOT EXISTS HDR( " \
                "ID INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL," \
                "VALUE TEXT NOT NULL );";
            return exec( stm );
        }

        int16_t make_alig_tbl( void ) {
            const char * stm = "CREATE TABLE IF NOT EXISTS ALIG( " \
                "ID INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL," \
                "NAME TEXT NOT NULL," \
                "FLAG INT NOT NULL," \
                "RNAME TEXT NOT NULL," \
                "RPOS INT NOT NULL," \
                "MAPQ INT NOT NULL," \
                "CIGAR TEXT NOT NULL," \
                "MRNAME TEXT NOT NULL," \
                "MRPOS INT NOT NULL," \
                "TLEN INT NOT NULL," \
                "SEQ TEXT NOT NULL," \
                "QUAL TEXT NOT NULL," \
                "TAGS TEXT NOT NULL );";
            return exec( stm );
        }
        
        int16_t make_refcnt_tbl( void ) {
            const char * stm = "CREATE TABLE IF NOT EXISTS REF_CNT( " \
                "ID INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL," \
                "RNAME TEXT NOT NULL UNIQUE," \
                "CNT INT NOT NULL );";
            return exec( stm );
        }

        int16_t make_spots_tbl( void ) {
            const char * stm = "CREATE TABLE IF NOT EXISTS SPOTS( " \
                "ID INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL," \
                "NAME TEXT NOT NULL UNIQUE," \
                "CNT INT NOT NULL );";
            return exec( stm );
        }

        int16_t make_spot_mode_tbl( void ) {
            const char * stm = "CREATE TABLE IF NOT EXISTS SPOT_MODE( " \
                "ID INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL," \
                "NAME TEXT NOT NULL UNIQUE," \
                "CNT INT NOT NULL );";
            int16_t res = exec( stm );
            if ( ok_or_done( res ) ) {
                const char * stm = "CREATE TRIGGER IF NOT EXISTS after_spot_insert " \
                    "AFTER INSERT ON SPOTS " \
                    "BEGIN" \
                    " INSERT INTO SPOT_MODE ( NAME, CNT )" \
                    " VALUES ( " \
                    "     NEW.NAME, " \
                    "     ( SELECT COUNT( NAME ) FROM ALIG WHERE NAME = NEW.NAME AND RNAME = '*' ) " \
                    " );" \
                    "END;";
                res = exec( stm );
            }
            return res;
        }

        uint64_t select_number( const char * sql ) {
            uint64_t res = 0;
            if ( ok_or_done( status ) ) {
                auto stm( make_prep_stm( sql ) );
                uint64_t tmp;
                status = stm -> read_long( tmp );
                if ( ok_or_done( status ) ) { res = tmp; }
            }
            return res;
        }

    public :
        SAM_DB( const std::string& filename ) 
            : DB( filename.c_str() ), status( SQLITE_OK ) {
            status = make_ref_hdr_tbl();
            if ( ok_or_done( status ) ) { status = make_hdr_tbl(); }
            if ( ok_or_done( status ) ) { status = make_alig_tbl(); }
            if ( ok_or_done( status ) ) { make_refcnt_tbl(); }
            if ( ok_or_done( status ) ) { make_spots_tbl(); }
            if ( ok_or_done( status ) ) { make_spot_mode_tbl(); }
            if ( ok_or_done( status ) ) {
                ins_ref_stm = make_prep_stm(
                    "INSERT INTO REF ( NAME, LENGTH, OTHER ) VALUES( ?, ?, ? );" );                    
                status = ins_ref_stm -> get_status();
            }
            if ( ok_or_done( status ) ) {
                ins_hdr_stm = make_prep_stm(
                    "INSERT INTO HDR ( VALUE ) VALUES( ? );" );
                status = ins_hdr_stm -> get_status();
            }
            if ( ok_or_done( status ) ) {
                ins_alig_stm = make_prep_stm(
                    "INSERT INTO ALIG "\
                    "( NAME, FLAG, RNAME, RPOS, MAPQ, CIGAR, MRNAME, MRPOS, TLEN, SEQ, QUAL, TAGS )"\
                    " VALUES( ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ? );" );
                status = ins_alig_stm -> get_status();
            }
        }
        
        int16_t drop_all( void ) {
            str_vec tables;
            tables . push_back( "REF" );
            tables . push_back( "HDR" );
            tables . push_back( "ALIG" );
            status = clear_tables( tables );
            return status; 
        }
        
        int16_t create_alig_tbl_idx( void ) {
            exec( "CREATE INDEX ALIG_RNAME_IDX on ALIG( RNAME );" );
            return exec( "CREATE INDEX ALIG_NAME_IDX on ALIG( NAME );" );            
        }

        int16_t create_refcnt_tbl_idx( void ) {
            return exec( "CREATE INDEX REF_CNT_RNAME_IDX on REF_CNT( RNAME );" );            
        }

        int16_t add_ref( const mt_database::PREP_STM::str_vec& data ) {
            if ( ok_or_done( status ) ) {
                status = ins_ref_stm -> bind_and_step( data );
            }
            return status;
        }
        
        int16_t add_hdr( const mt_database::PREP_STM::str_vec& data ) {
            if ( ok_or_done( status ) ) {
                status = ins_hdr_stm -> bind_and_step( data );
            }
            return status;
        }
        
        int16_t add_alig( const mt_database::PREP_STM::str_vec& data ) {
            if ( ok_or_done( status ) ) {
                status = ins_alig_stm -> bind_and_step( data );
            }
            return status;
        }
        
        int16_t populate_refcnt( void ) {
            if ( ok_or_done( status ) ) {
                const char * stm = "INSERT INTO REF_CNT( RNAME, CNT ) " \
                    "SELECT RNAME,COUNT(RNAME) AS RNCNT FROM ALIG " \
                    "GROUP BY RNAME ORDER BY RNCNT DESC;";
                status = exec( stm );
            }
            return status;
        }

        int16_t populate_spots( void ) {
            if ( ok_or_done( status ) ) {
                const char * stm = "INSERT INTO SPOTS( NAME, CNT ) " \
                    "SELECT NAME,COUNT(NAME) AS NCNT FROM ALIG " \
                    "GROUP BY NAME ORDER BY NCNT DESC;";
                status = exec( stm );
            }
            return status;
        }

        uint64_t spot_count( void ) {
            return select_number( "SELECT count(*) FROM SPOTS;" );
        }
        
        uint64_t refs_in_use_count( void ) {
            return select_number( "SELECT count(*) FROM REF_CNT WHERE RNAME != '*';" );
        }
        
        uint64_t alig_count( void ) {
            return select_number( "SELECT count(*) FROM ALIG;" );            
        }

        uint64_t count_unaligned( void ) {
            return select_number( "select count( NAME ) from SPOT_MODE where CNT > 1;" );
        }

        uint64_t count_fully_aligned( void ) {
            return select_number( "select count( NAME ) from SPOT_MODE where CNT = 0;" );
        }

        uint64_t count_half_aligned( void ) {
            return select_number( "select count( NAME ) from SPOT_MODE where CNT = 1;" );
        }
        
        int16_t exec_pragma( const std::string& value ) {
            if ( ok_or_done( status ) ) {
                std::string stm( "PRAGMA " );
                stm += value;
                stm += ";";
                status = exec( stm . c_str() );
            }
            return status;
        }
};

typedef enum ALIG_ORDER{ ALIG_ORDER_NONE, ALIG_ORDER_NAME, ALIG_ORDER_REFPOS } ALIG_ORDER;

struct SAM_ALIG {
    std::string NAME;
    uint16_t FLAGS;
    std::string RNAME;
    uint32_t RPOS;
    uint8_t MAPQ;
    std::string CIGAR;
    std::string MRNAME;
    uint32_t MRPOS;
    int32_t TLEN;
    std::string SEQ;
    std::string QUAL;
    std::string TAGS;
    bool empty;
    
    void read( mt_database::PREP_STM_PTR stm ) {
        NAME . assign( stm -> read_column_text( 0 ) );
        FLAGS = stm -> read_column_int( 1 );
        RNAME . assign( stm -> read_column_text( 2 ) );
        RPOS = stm -> read_column_int64( 3 );
        MAPQ = stm -> read_column_int( 4 );
        CIGAR . assign( stm -> read_column_text( 5 ) );
        MRNAME . assign( stm -> read_column_text( 6 ) );
        MRPOS = stm -> read_column_int64( 7 );
        TLEN = stm -> read_column_int( 8 );
        SEQ . assign( stm -> read_column_text( 9 ) );
        QUAL . assign( stm -> read_column_text( 10 ) );
        TAGS . assign( stm -> read_column_text( 11 ) );
        empty = false;
    }

    SAM_ALIG() : empty( true ) {}
    
    void print( std::ostream& stream ) const {
        stream << NAME << "\t" << FLAGS << "\t" << RNAME << "\t" << RPOS << "\t" << MAPQ << "\t"
            << CIGAR << "\t" << MRNAME << "\t" << MRPOS << "\t" << TLEN << "\t" << SEQ << "\t"
            << QUAL << "\t" << TAGS << std::endl;
    }
    
    void fix_name( void ) {
        std::replace( NAME . begin(), NAME . end(), ' ', '_' );
    }
    
    bool same_name( const SAM_ALIG& other ) const {
        return ( NAME == other . NAME );
    }
};

class ALIG_ITER {
    private :
        mt_database::PREP_STM_PTR stm;
        
        ALIG_ITER( const ALIG_ITER& ) = delete;

    public :
        ALIG_ITER( SAM_DB &db, ALIG_ORDER spot_order ) { 
            std::string cols( "NAME,FLAG,RNAME,RPOS,MAPQ,CIGAR,MRNAME,MRPOS,TLEN,SEQ,QUAL,TAGS" );
            std::string sstmu( "SELECT " + cols + " FROM ALIG;" );
            std::string sstms( "SELECT " + cols + " FROM ALIG order by RNAME,RPOS;" );
            std::string sstmn( "SELECT " + cols + " FROM ALIG order by NAME;" );
            std::string sst;
            switch( spot_order ) {
                case ALIG_ORDER_NAME   : sst = sstmn; break;
                case ALIG_ORDER_REFPOS : sst = sstms; break;
                default : sst = sstmu; break;
            };
            stm = db . make_prep_stm( sst );
        }

        bool next( SAM_ALIG& alig ) {
            bool res = stm -> step_if_ok_or_row();
            if ( res ) { alig . read( stm ); }
            return res;;
        }
};

struct SAM_SPOT {
    std::vector< SAM_ALIG > aligs;
    
    void clear( SAM_ALIG& alig ) {
        aligs . clear();
        if ( ! alig . empty ) { aligs . push_back( alig ); }
    }

    bool add( SAM_ALIG& alig ) {
        bool res = true;
        if ( aligs . empty() ) {
            aligs . push_back( alig );
        } else {
            auto last = aligs . back();
            res = ( alig . same_name( last ) );
            if ( res ) {
                aligs . push_back( alig );
            }
        }
        return res;
    }

    size_t count( void ) { return aligs . size(); }
    
    size_t aligned_count( void ) {
        size_t res = 0;
        for ( auto & entry : aligs ) {
            if ( entry . RNAME != "*" ) res++;
        }
        return res;
    }
};

class SPOT_ITER {
    private :
        ALIG_ITER alig_iter;
        SAM_ALIG alig;

        SPOT_ITER( const SPOT_ITER& ) = delete;

    public :
        SPOT_ITER( SAM_DB &db ) : alig_iter( db, ALIG_ORDER_NAME ) { }
        
        bool next( SAM_SPOT& spot ) {
            bool done = false;
            bool res = false;
            spot . clear( alig );
            while ( !done ) {
                bool res = alig_iter . next( alig );
                if ( res ) {
                    if ( spot . add( alig ) ) {
                        // belongs to the current spot, continue
                    } else {
                        done = true;
                        // no: different name, keep it in alig for the next call
                    }
                } else {
                    // no more alignments in the alig-iter
                    done = true;
                }
            }
            return res;
        }
};

#endif
