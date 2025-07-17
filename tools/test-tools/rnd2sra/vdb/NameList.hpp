#pragma once

#include <string>
#include <list>

namespace vdb {

extern "C" {

typedef uint32_t rc_t;

/* ========================================================================================= */

typedef struct KNamelist KNamelist;

rc_t KNamelistAddRef( const KNamelist *self );
rc_t KNamelistRelease( const KNamelist *self );
rc_t KNamelistCount( const KNamelist *self, uint32_t *count );
rc_t KNamelistGet( const KNamelist *self, uint32_t idx, const char **name );
bool KNamelistContains( const KNamelist * self, const char * to_find );

/* ========================================================================================= */

/*
typedef struct VNamelist VNamelist;

rc_t VNamelistMake( VNamelist **names, const uint32_t alloc_blocksize );
rc_t VNamelistRelease( const VNamelist *self );
rc_t VNamelistToNamelist( VNamelist *self, KNamelist **cast );
rc_t VNamelistToConstNamelist( const VNamelist *self, const KNamelist **cast );
rc_t VNamelistAppend ( VNamelist *self, const char* src );
rc_t VNamelistAppendString( VNamelist *self, const String * src );
rc_t VNamelistRemove( VNamelist *self, const char* s );
rc_t VNamelistRemoveAll( VNamelist *self );
rc_t VNamelistRemoveIdx( VNamelist *self, uint32_t idx );
rc_t VNamelistIndexOf( VNamelist *self, const char* s, uint32_t *found );
rc_t VNameListCount( const VNamelist *self, uint32_t *count );
rc_t VNameListGet( const VNamelist *self, uint32_t idx, const char **name );
void VNamelistReorder( VNamelist *self, bool case_insensitive );
rc_t foreach_String_part( const String * src, const uint32_t delim,
        rc_t ( * f ) ( const String * part, void *data ), void * data );
rc_t foreach_Str_part( const char * src, const uint32_t delim,
        rc_t ( * f ) ( const String * part, void *data ), void * data );
rc_t VNamelistSplitString( VNamelist * list, const String * str, const uint32_t delim );
rc_t VNamelistSplitStr( VNamelist * list, const char * str, const uint32_t delim );
rc_t VNamelistFromKNamelist( VNamelist ** list, const KNamelist * src );
rc_t CopyVNamelist( VNamelist ** list, const VNamelist * src );
rc_t VNamelistFromString( VNamelist ** list, const String * str, const uint32_t delim );
rc_t VNamelistFromStr( VNamelist ** list, const char * str, const uint32_t delim );
rc_t VNamelistJoin( const VNamelist * list, const uint32_t delim, const String ** rslt );
rc_t VNamelistContainsString( const VNamelist * list, const String * item, int32_t * idx );
rc_t VNamelistContainsStr( const VNamelist * list, const char * item, int32_t * idx );
*/

}; // end of extern "C"

typedef std::list< std::string > list_of_str;

class NameList {
    private :
        KNamelist * l;
    public :
        NameList( KNamelist * al ) : l( al ) {}
        ~NameList() { KNamelistRelease( l ); }

        void into( list_of_str& res ) {
            uint32_t count;
            rc_t rc = KNamelistCount( l, &count );
            for ( uint32_t i = 0; 0 == rc && i < count; i++ ) {
                const char * s = nullptr;
                rc = KNamelistGet( l, i, &s );
                if ( 0 == rc && nullptr != s ) {
                    res . push_back( std::string( s ) );
                }
            }
        }

        list_of_str to_list( void ) {
            list_of_str res;
            into( res );
            return res;
        }
};

}; // end of namespace vdb
