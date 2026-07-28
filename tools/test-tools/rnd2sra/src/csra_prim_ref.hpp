#pragma once

#include <cstdint>
#include <memory>
#include <vector>
#include <iostream>

using namespace std;

namespace sra_convert {

class Prim_Ref_Recorder;
typedef std::shared_ptr< Prim_Ref_Recorder > Prim_Ref_Recorder_ptr;
class Prim_Ref_Recorder {
    private:

        class Prim_Ref_Entry;
        typedef shared_ptr< Prim_Ref_Entry > Prim_Ref_Entry_ptr;
        class Prim_Ref_Entry {
            private :
                vector< int64_t > f_prim_row_ids;
            public :
                static Prim_Ref_Entry_ptr make( void ) {
                    return Prim_Ref_Entry_ptr( new Prim_Ref_Entry() );
                }
                void add( int64_t id ) { f_prim_row_ids.push_back( id ); }
                size_t count( void ) const { return f_prim_row_ids . size(); }
                const int64_t* arr( void ) const { return &f_prim_row_ids[ 0 ]; }
        };

        vector< Prim_Ref_Entry_ptr > f_entries;

        Prim_Ref_Recorder( size_t ref_row_count ) {
            for( uint32_t i = 0; i < ref_row_count; ++i ) {
                f_entries . push_back( Prim_Ref_Entry::make() );
            }
        }

        bool erase_entry( int64_t ref_id_1 ) {
            bool res = ( ref_id_1 > 0 && ref_id_1 <= ( int64_t )f_entries . size() );
            if ( res ) { f_entries . erase( f_entries . begin() + ( ref_id_1 - 1 ) ); }
            return res;
        }

    public :
        static Prim_Ref_Recorder_ptr make( size_t ref_row_count ) {
            return Prim_Ref_Recorder_ptr( new Prim_Ref_Recorder( ref_row_count ) );
        }

        size_t ref_row_count( void ) const { return f_entries . size(); }

        bool remove_ref_id( int64_t ref_id_1 ) {
            bool done = false;
            while ( !done ) {
                if ( prim_al_id_count( ref_id_1 ) > 0 ) {
                    done = erase_entry( ref_id_1 );
                } else {
                    if ( ref_id_1 < ( int64_t )f_entries . size() ) {
                        ref_id_1 ++;
                    } else {
                        break;
                    }
                }
            }
            return done;
        }

        void add( int64_t prim_al_id, int64_t ref_id_1 ) {
            if ( ref_id_1 > 0 && ref_id_1 <= ( int64_t )f_entries . size() ) {
                f_entries[ --ref_id_1 ] -> add( prim_al_id );
            }
        }

        size_t prim_al_id_count( int64_t ref_id_1 ) const {
            if ( ref_id_1 > 0 && ref_id_1 <= ( int64_t )f_entries . size() ) {
                return f_entries[ --ref_id_1 ] -> count();
            }
            return 0;
        }

        const int64_t* prim_al_ids( int64_t ref_id_1 ) const {
            if ( ref_id_1 > 0 && ref_id_1 <= ( int64_t )f_entries . size() ) {
                return f_entries[ --ref_id_1 ] -> arr();
            }
            return nullptr;
        }

};

} // end of namespace sra_convert
