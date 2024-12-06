#pragma once

#include "VdbObj.hpp"
#include "RowRange.hpp"
#include "VdbString.hpp"
#include "VSchema.hpp"

namespace vdb {

extern "C" {

rc_t VTableCreateCursorRead( VTable const *self, const VCursor **curs );
rc_t VTableCreateCursorWrite ( VTable *self, VCursor **curs, KCreateMode mode );
rc_t VTableCreateCachedCursorRead( VTable const *self, const VCursor **curs, std::size_t capacity );

rc_t VCursorAddRef( const VCursor *self );
rc_t VCursorRelease( const VCursor *self );

rc_t VViewCreateCursor( VView const *self, const VCursor **curs );

rc_t VCursorAddColumn( const VCursor *self, uint32_t *idx, const char *name, ... );
rc_t VCursorVAddColumn( const VCursor *self, uint32_t *idx, const char *name, va_list args );

rc_t VCursorGetColumnIdx( const VCursor *self, uint32_t *idx, const char *name, ... );
rc_t VCursorVGetColumnIdx( const VCursor *self, uint32_t *idx, const char *name, va_list args );

rc_t VCursorDatatype( const VCursor *self, uint32_t idx, VTypedecl *type, VTypedesc *desc );

rc_t VCursorIdRange( const VCursor *self, uint32_t idx, int64_t *first, uint64_t *count );

rc_t VCursorOpen( const VCursor *self );
rc_t VCursorRowId( const VCursor *self, int64_t *row_id );
rc_t VCursorSetRowId( const VCursor *self, int64_t row_id );
rc_t VCursorFindNextRowId( const VCursor *self, uint32_t idx, int64_t * next );
rc_t VCursorFindNextRowIdDirect( const VCursor *self, uint32_t idx, int64_t start_id, int64_t * next );
rc_t VCursorOpenRow( const VCursor *self );
rc_t VCursorCloseRow( const VCursor *self );
rc_t VCursorCommitRow( VCursor *self );
rc_t VCursorCommit( VCursor *self );

rc_t VCursorGetBlob( const VCursor *self,  VBlob const **blob, uint32_t col_idx );
rc_t VCursorGetBlobDirect( const VCursor *self, VBlob const **blob, int64_t row_id, uint32_t col_idx );

rc_t VCursorRead( const VCursor *self, uint32_t col_idx, uint32_t elem_bits,
        void *buffer, uint32_t blen, uint32_t *row_len );
rc_t VCursorReadDirect( const VCursor *self, int64_t row_id, uint32_t col_idx, uint32_t elem_bits,
        void *buffer, uint32_t blen, uint32_t *row_len );

rc_t VCursorReadBits( const VCursor *self, uint32_t col_idx, uint32_t elem_bits, uint32_t start,
        void *buffer, uint32_t boff, uint32_t blen, uint32_t *num_read, uint32_t *remaining );
rc_t VCursorReadBitsDirect( const VCursor *self, int64_t row_id, uint32_t col_idx, uint32_t elem_bits,
        uint32_t start, void *buffer, uint32_t boff, uint32_t blen, uint32_t *num_read, uint32_t *remaining );

rc_t VCursorCellData( const VCursor *self, uint32_t col_idx, uint32_t *elem_bits, const void **base,
        uint32_t *boff, uint32_t *row_len );
rc_t VCursorCellDataDirect( const VCursor *self, int64_t row_id, uint32_t col_idx, uint32_t *elem_bits,
        const void **base, uint32_t *boff, uint32_t *row_len );

rc_t VCursorDefault( VCursor *self, uint32_t col_idx, bitsz_t elem_bits,
                     const void *buffer, bitsz_t boff, uint64_t row_len );

rc_t VCursorWrite( VCursor *self, uint32_t col_idx, bitsz_t elem_bits,
                   const void *buffer, bitsz_t boff, uint64_t count );

rc_t VCursorDataPrefetch( const VCursor * self, const int64_t * row_ids, uint32_t col_idx,
        uint32_t num_rows, int64_t min_valid_row_id, int64_t max_valid_row_id, bool continue_on_error );

rc_t VBlobAddRef( const VBlob *self );
rc_t VBlobRelease( const VBlob *self );
rc_t VBlobIdRange( const VBlob *self, int64_t *first, uint64_t *count );
rc_t VBlobSize( const VBlob * self, std::size_t * bytes );

rc_t VBlobRead( const VBlob *self, int64_t row_id, uint32_t elem_bits, void *buffer,
        uint32_t blen, uint32_t *row_len );

rc_t VBlobReadBits( const VBlob *self, int64_t row_id, uint32_t elem_bits, uint32_t start,
        void *buffer, uint32_t boff, uint32_t blen, uint32_t *num_read, uint32_t *remaining );

rc_t VBlobCellData( const VBlob *self, int64_t row_id, uint32_t *elem_bits,
        const void **base, uint32_t *boff, uint32_t *row_len );

rc_t VViewAddRef( const VView *self );
rc_t VViewRelease( const VView *self );

rc_t VViewCreateCursor( VView const *self, const VCursor **curs );
uint32_t VViewParameterCount( VView const * self );
rc_t VViewGetParameter( VView const * self, uint32_t idx, const String ** name, bool * is_table );
rc_t VViewBindParameterTable( const VView * self, const String * param_name, const VTable * table );
rc_t VViewBindParameterView( const VView * self, const String * param_name, const VView * view );
rc_t VViewListCol( const VView *self, KNamelist **names );
rc_t VViewOpenSchema( const VView *self, VSchema const **schema );

}; // end of extern "C"

#define COL_ARRAY_COUNT 32
#define INVALID_COL 0xFFFFFFFF

class VCur;
typedef std::shared_ptr<VCur> VCurPtr;
class VCur : public VDBObj {
    private :
#ifdef VDB_WRITE
        VCursor * f_cur;
#else
        const VCursor * f_cur;
#endif
        uint32_t columns[ COL_ARRAY_COUNT ];
        uint32_t elem_bits, boff, row_len;
        const void *base;
        uint32_t f_next_col_id;

#ifdef VDB_WRITE
        VCur( VTable * tbl ) {
            set_rc( VTableCreateCursorWrite( tbl, &f_cur, kcmCreate ) );
            for ( int i = 0; i < COL_ARRAY_COUNT; ++i ) { columns[ i ] = INVALID_COL; }
            f_next_col_id = 0;
        }
#endif

        VCur( const VTable * tbl, std::size_t capacity = 0 ) {
            if ( 0 == capacity ) {
                set_rc( VTableCreateCursorRead( tbl, ( const VCursor ** )&f_cur ) );
            } else {
                set_rc( VTableCreateCachedCursorRead( tbl, ( const VCursor ** )&f_cur, capacity ) );
            }
            for ( int i = 0; i < COL_ARRAY_COUNT; ++i ) { columns[ i ] = INVALID_COL; }
            f_next_col_id = 0;
        }

        bool valid_id( uint32_t id ) { return ( id < COL_ARRAY_COUNT && INVALID_COL != columns[ id ] ); }

    public :
        ~VCur() { VCursorRelease( f_cur ); }

#ifdef VDB_WRITE
        static VCurPtr make( VTable * tbl ) { return VCurPtr( new VCur( tbl ) ); }
#endif

        uint32_t next_col_id( void ) const { return f_next_col_id; }
        void inc_next_col_id( void ) { f_next_col_id++; }

        static VCurPtr make( const VTable * tbl, std::size_t capacity = 0 ) {
            return VCurPtr( new VCur( tbl, capacity ) );
        }

        bool open( void ) { return set_rc( VCursorOpen( f_cur ) ); }

        int64_t get_rowid( void ) {
            int64_t res = 0;
            set_rc( VCursorRowId( f_cur, &res ) );
            return res;
        }

        bool set_rowid( int64_t row ) { return set_rc( VCursorSetRowId( f_cur, row ) ); }

        int64_t find_next_rowid( uint32_t col_idx ) {
            int64_t res = 0;
            set_rc( VCursorFindNextRowId( f_cur, col_idx, &res ) );
            return res;
        }

        int64_t find_next_rowid_from( uint32_t col_idx, int64_t start ) {
            int64_t res = 0;
            set_rc( VCursorFindNextRowIdDirect( f_cur, col_idx, start, &res ) );
            return res;
        }

        bool open_row( void ) { return set_rc( VCursorOpenRow( f_cur ) ); }
        bool close_row( void ) { return set_rc( VCursorCloseRow( f_cur ) ); }

        bool add_col( uint32_t id, const std::string& name ) {
            if ( id >= COL_ARRAY_COUNT ) return false;
            if ( name . empty() ) return false;
            if ( INVALID_COL != columns[ id ] ) return false;
            return set_rc( VCursorAddColumn( f_cur, &( columns[ id ] ), "%s", name.c_str() ) );
        }

        RowRange row_range( uint32_t id ) {
            RowRange res{ 0, 0 };
            if ( valid_id( id  ) ) {
                set_rc( VCursorIdRange( f_cur, columns[ id ], &( res . first ), &( res . count ) ) );
            }
            return res;
        }

/* ----------------------------------------------------------------------------------------------------- */

        bool CellData( uint32_t id, int64_t row, uint32_t *elem_bits_p,
                       const void **base_p, uint32_t *boff_p, uint32_t *row_len_p ) {
            if ( !valid_id( id ) ) return false;
            return set_rc( VCursorCellDataDirect( f_cur, row, columns[ id ], elem_bits_p, base_p, boff_p, row_len_p ) );
        }

        template <typename T> bool read( uint32_t id, int64_t row, T* v ) {
            bool res = CellData( id, row, &elem_bits, &base, &boff, &row_len );
            if ( res ) { *v = *( ( T* )base ); }
            return res;
        }

        template <typename T, typename A> bool read_arr( uint32_t id, int64_t row, A* arr ) {
            bool res = CellData( id, row, &elem_bits, &base, &boff, &row_len );
            if ( res ) {
                arr -> data = ( T* )base;
                arr -> len = row_len;
            }
            return res;
        }

/* ----------------------------------------------------------------------------------------------------- */

#ifdef VDB_WRITE
        bool SetDefault( uint32_t id, bitsz_t elem_bits_p, const void * data_p,
                         bitsz_t boff_p, uint64_t row_len_p ) {
            if ( !valid_id( id ) ) return false;
            return set_rc( VCursorDefault( f_cur, columns[ id ], elem_bits_p, data_p, boff_p, row_len_p ) );
        }

        bool WriteCell( uint32_t id, bitsz_t elem_bits_p, const void * data_p,
                        bitsz_t boff_p, uint64_t row_len_p ) {
            if ( !valid_id( id ) ) return false;
            return set_rc( VCursorWrite( f_cur, columns[ id ], elem_bits_p, data_p, boff_p, row_len_p ) );
        }

        template <typename T> bool write( uint32_t id, const T* v ) {
            uint32_t bitcount = sizeof( T ) * 8;
            return WriteCell( id, bitcount, ( void * )v, 0, 1 );
        }

        template <typename T, typename A> bool write_arr( uint32_t id, const A* arr ) {
            uint32_t bitcount = sizeof( T ) * 8;
            return WriteCell( id, bitcount, ( void * )( arr -> data ), 0, arr -> len );
        }

        bool write_u8( uint32_t id, uint8_t v ) { return write< uint8_t >( id, &v ); }
        bool write_i8( uint32_t id, int8_t v )  { return write< int8_t >( id, &v ); }
        bool write_u16( uint32_t id, uint16_t v ) { return write< uint16_t >( id, &v ); }
        bool write_i16( uint32_t id, int16_t v )  { return write< int16_t >( id, &v ); }
        bool write_u32( uint32_t id, uint32_t v ) { return write< uint32_t >( id, &v ); }
        bool write_i32( uint32_t id, int32_t v )  { return write< int32_t >( id, &v ); }
        bool write_u64( uint32_t id, uint64_t v ) { return write< uint64_t >( id, &v ); }
        bool write_i64( uint32_t id, int64_t v )  { return write< int64_t >( id, &v ); }
        bool write_f32( uint32_t id, float v ) { return write< float >( id, &v ); }
        bool write_f64( uint32_t id, double v )  { return write< double >( id, &v ); }

        bool write_u8arr( uint32_t id, const uint8_t * v, size_t len ) {
            uint8_arr a{ v, len }; return write_arr< uint8_t, uint8_arr >( id, &a );
        }
        bool write_u8arr( uint32_t id, const uint8_arr_ptr v ) { return write_arr< uint8_t, uint8_arr >( id, v ); }

        bool write_i8arr( uint32_t id, const int8_t * v, size_t len ) {
            int8_arr a{ v, len }; return write_arr< int8_t, int8_arr >( id, &a );
        }
        bool write_i8arr( uint32_t id, const int8_arr_ptr v ) { return write_arr< int8_t, int8_arr >( id, v ); }

        bool write_u16arr( uint32_t id, uint16_t * v, size_t len ) {
            uint16_arr a{ v, len }; return write_arr< uint16_t, uint16_arr >( id, &a );
        }
        bool write_u16arr( uint32_t id, uint16_arr_ptr v ) { return write_arr< uint16_t, uint16_arr >( id, v ); }

        bool write_i16arr( uint32_t id, const int16_t * v, size_t len ) {
            int16_arr a{ v, len }; return write_arr< int16_t, int16_arr >( id, &a );
        }
        bool write_i16arr( uint32_t id, const int16_arr_ptr v ) { return write_arr< int16_t, int16_arr >( id, v ); }

        bool write_u32arr( uint32_t id, const uint32_t * v, size_t len ) {
            uint32_arr a{ v, len };  return write_arr< uint32_t, uint32_arr >( id, &a );
        }
        bool write_u32arr( uint32_t id, const uint32_arr_ptr v ) { return write_arr< uint32_t, uint32_arr >( id, v ); }

        bool write_i32arr( uint32_t id, const int32_t * v, size_t len ) {
            int32_arr a{ v, len }; return write_arr< int32_t, int32_arr >( id, &a );
        }
        bool write_i32arr( uint32_t id, const int32_arr_ptr v ) { return write_arr< int32_t, int32_arr >( id, v ); }

        bool write_u64arr( uint32_t id, const uint64_t * v, size_t len ) {
            uint64_arr a{ v, len }; return write_arr< uint64_t, uint64_arr >( id, &a );
        }
        bool write_u64arr( uint32_t id, const uint64_arr_ptr v ) { return write_arr< uint64_t, uint64_arr >( id, v ); }

        bool write_i64arr( uint32_t id, const int64_t * v, size_t len ) {
            int64_arr a{ v, len }; return write_arr< int64_t, int64_arr >( id, &a ); }
        bool write_i64arr( uint32_t id, const int64_arr_ptr v ) { return write_arr< int32_t, int64_arr >( id, v ); }

        bool write_f32arr( uint32_t id, const float * v, size_t len ) {
            f32_arr a{ v, len }; return write_arr< float, f32_arr >( id, &a );
        }
        bool write_f32arr( uint32_t id, const f32_arr_ptr v ) { return write_arr< float, f32_arr >( id, v ); }

        bool write_f64arr( uint32_t id, const double * v, size_t len ) {
            f64_arr a{ v, len }; return write_arr< double, f64_arr >( id, &a );
        }
        bool write_f64arr( uint32_t id, const f64_arr_ptr v ) { return write_arr< double, f64_arr >( id, v ); }

        bool WriteString( uint32_t id, const std::string& v ) {
            char_arr a{ v . c_str(), v . length() }; return write_arr< char, char_arr >( id, &a );
        }

        template <typename T> bool write_dflt( uint32_t id, const T* v ) {
            uint32_t bitcount = sizeof( T ) * 8;
            return SetDefault( id, bitcount, ( void * )v, 0, 1 );
        }

        bool dflt_u8( uint32_t id, uint8_t v ) { return write_dflt< uint8_t >( id, &v ); }
        bool dflt_i8( uint32_t id, int8_t v )  { return write_dflt< int8_t >( id, &v ); }
        bool dflt_u16( uint32_t id, uint16_t v ) { return write_dflt< uint16_t >( id, &v ); }
        bool dlft_i16( uint32_t id, int16_t v )  { return write_dflt< int16_t >( id, &v ); }
        bool dflt_u32( uint32_t id, uint32_t v ) { return write_dflt< uint32_t >( id, &v ); }
        bool dlft_i32( uint32_t id, int32_t v )  { return write_dflt< int32_t >( id, &v ); }
        bool dflt_u64( uint32_t id, uint64_t v ) { return write_dflt< uint64_t >( id, &v ); }
        bool dflt_i64( uint32_t id, int64_t v )  { return write_dflt< int64_t >( id, &v ); }
        bool dlft_f32( uint32_t id, float v ) { return write_dflt< float >( id, &v ); }
        bool dlft_f64( uint32_t id, double v )  { return write_dflt< double >( id, &v ); }

        bool commit_row( void ) {
            return set_rc( VCursorCommitRow( f_cur ) );
        }

        bool commit( void ) {
            return set_rc( VCursorCommit( f_cur ) );
        }
#endif

};

}; // end of namespace vdb
