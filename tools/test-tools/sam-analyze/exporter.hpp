#ifndef EXPORTER_H
#define EXPORTER_H

#include <algorithm>

#include "line_split.hpp"
#include "params.hpp"
#include "result.hpp"
#include "sam_db.hpp"

class EXPORTER {
    private :
        const EXPORT_PARAMS& params;
        EXPORT_RESULT& result;
        SAM_DB &db;

        EXPORTER( const EXPORTER& ) = delete;
        
        bool export_HD_headers( std::ostream& stream, bool negate ) {
            auto stm = mt_database::PREP_STM(
                db . get_db_handle(),
                negate ? "SELECT VALUE FROM HDR WHERE NOT VALUE LIKE '@HD%';"
                       : "SELECT VALUE FROM HDR WHERE VALUE LIKE '@HD%';" );
            if ( SQLITE_OK == stm . get_status() ) {
                mt_database::DB::str_vec data;
                int st;
                do {
                    st = stm . read_row( data, 1 );
                    if ( SQLITE_ROW == st ) {
                        stream << data[ 0 ] << std::endl;
                        result . header_lines += 1;
                    }
                } while ( SQLITE_ROW == st );
            }
            return ( SQLITE_DONE == stm . get_status() );
        }

        bool export_ref_headers( std::ostream& stream ) {
            std::string sstm( "SELECT NAME,LENGTH,OTHER FROM REF" );
            if ( params . only_used_refs ) {
                sstm +=" WHERE ( SELECT CNT FROM REF_CNT WHERE RNAME = REF.NAME ) > 0;";
            } else {
                sstm += ";";
            }
            auto stm = mt_database::PREP_STM( db . get_db_handle(), sstm );
            if ( SQLITE_OK == stm . get_status() ) {
                mt_database::DB::str_vec data;
                int st;
                do {
                    st = stm . read_row( data, 3 );
                    if ( SQLITE_ROW == st ) {
                        stream << "@SQ\tSN:" << data[ 0 ] << "\tLN:" << data[ 1 ] << std::endl;
                        result . header_lines += 1;
                    }
                } while ( SQLITE_ROW == st );
            }
            return ( SQLITE_DONE == stm . get_status() );
        }

        bool export_headers( std::ostream& stream ) {
            bool res = export_HD_headers( stream, false );
            if ( res ) { res = export_ref_headers( stream ); }
            if ( res ) { res = export_HD_headers( stream, true ); }
            return res;
        }

        std::string fix_name( const std::string& s ){
            std::string str( s );
            std::replace( str . begin(), str . end(), ' ', '_' );
            return str;
        }
        
        bool export_alignments( std::ostream& stream ) {
            std::string cols( "NAME,FLAG,RNAME,RPOS,MAPQ,CIGAR,MRNAME,MRPOS,TLEN,SEQ,QUAL,TAGS" );
            std::string sstmu( "SELECT " + cols + " FROM ALIG;" );
            std::string sstms( "SELECT " + cols + " FROM ALIG order by RNAME,RPOS;" );
            std::string sstmn( "SELECT " + cols + " FROM ALIG order by NAME;" );
            std::string sst;
            if ( params . sort_by_ref_pos ) {
                sst = sstms;
            } else if ( params . sort_by_name ) {
                sst = sstmn;
            } else {
                sst = sstmu;
            }
            auto stm = mt_database::PREP_STM( db . get_db_handle(), sst );
            if ( SQLITE_OK == stm . get_status() ) {
                bool show_progress = params . cmn . progress;
                mt_database::DB::str_vec data;
                int st;
                do {
                    st = stm . read_row( data, 12 );
                    if ( SQLITE_ROW == st ) {
                        if ( params . fix_names ) {
                            stream << fix_name( data[ 0 ] ) << "\t";                            
                        } else {
                            stream << data[ 0 ] << "\t";
                        }
                        for ( int i = 1; i < 11; ++i ) { stream << data[ i ] << "\t"; }
                        stream << data[ 11 ] << std::endl;
                        result . alignment_lines += 1;
                        if ( show_progress ) {
                            if ( ( result . alignment_lines % params . cmn . transaction_size ) == 0 ) {
                                 std::cerr << '.';
                            }
                        }
                    }
                } while ( SQLITE_ROW == st );
                if ( show_progress ) { std::cerr << std::endl; }
            }
            return ( SQLITE_DONE == stm . get_status() );
        }

    public :
        EXPORTER( SAM_DB& a_db, const EXPORT_PARAMS& a_params, EXPORT_RESULT& a_result )
            : params( a_params ),
              result( a_result ),
              db( a_db ) { }

        void run( void ) {
            FILE_WRITER writer( params . export_filename );
            std::ostream *stream = writer . get();
            bool ok = export_headers( *stream );
            if ( ok ) { ok = export_alignments( *stream ); }
            result . success = ok;
        }

};


#endif
