#pragma once

#include <sstream>

#include "VdbObj.hpp"

namespace vdb {

extern "C" {

rc_t KProcMgrInit( void );
rc_t KProcMgrWhack( void );
rc_t KProcMgrMakeSingleton( KProcMgr ** mgr );
rc_t KProcMgrAddRef( const KProcMgr *self );
rc_t KProcMgrRelease( const KProcMgr *self );
rc_t KProcMgrAddCleanupTask( KProcMgr *self, KTaskTicket *ticket, KTask *task );
rc_t ProcMgrRemoveCleanupTask( KProcMgr *self, KTaskTicket const *ticket );
bool KProcMgrOnMainThread( void );
rc_t KProcMgrGetPID( const KProcMgr * self, uint32_t * pid );
rc_t KProcMgrGetHostName( const KProcMgr * self, char * buffer, std::size_t buffer_size );

};  // end of extern "C"

class ProcMgr;
typedef std::shared_ptr< ProcMgr > ProcMgrPtr;
class ProcMgr : public VDBObj {
    private :
        KProcMgr * mgr;

        ProcMgr( void ) : mgr( nullptr ) {
            if ( set_rc( KProcMgrInit() ) ) {
                set_rc( KProcMgrMakeSingleton( &mgr ) );
            }
        }

    public :
        static ProcMgrPtr make( void ) { return ProcMgrPtr( new ProcMgr ); }

        ~ProcMgr() { if ( nullptr != mgr ) { KProcMgrRelease( mgr ); } }

        bool on_main_thread( void ) const { return KProcMgrOnMainThread(); }

        uint32_t get_pid( void ) const {
            uint32_t res = 0;
            rc_t rc1 = KProcMgrGetPID( mgr, &res );
            return 0 == rc1 ? res : 0;
        }

        std::string get_host_name( void ) const {
            char buffer[ 512 ];
            rc_t rc1 = KProcMgrGetHostName( mgr, buffer, sizeof buffer );
            if ( 0 != rc1 ) { buffer[ 0 ] = 0; }
            return std::string( buffer );
        }

        std::string unique_id( void ) const {
            std::stringstream ss;
            ss << get_host_name() << "_" << get_pid();
            return ss . str();
        }
};

}; // end of namespace vdb
