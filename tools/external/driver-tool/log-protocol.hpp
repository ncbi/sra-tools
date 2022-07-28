/*

  tools.common.log-protocol

  copyright: (C) 2015-2018 Rodarmer, Inc. All Rights Reserved

 */

#ifndef _hpp_tools_common_log_protocol_
#define _hpp_tools_common_log_protocol_

#ifndef _hpp_tools_common_defs_
#include <common/defs.hpp>
#endif

namespace cmn
{
    enum LogEvent
    {
        logInvalid,
        logOpen,
        logPID,
        logMsg
    };

    struct LogOpenEvent
    {
        inline size_t size ( size_t bytes ) const
        { return sizeof * this - sizeof this -> str + bytes; }

        U8 event_code;
        U8 queue;
        U8 hostname;
        U8 procname;
        U32 pid;
        char str [ 1 /* bytes */ ];

    private:

        void * operator new ( std :: size_t );
        void operator delete ( void * );
    };

    struct LogPIDEvent
    {
        U8 event_code;
        U8 pid [ 4 ];
    };

    struct LogMsgEvent
    {
        inline size_t size ( size_t bytes ) const
        { return sizeof * this - sizeof this -> msg + bytes; }

        U8 event_code;
        U8 priority;
        U16 bytes;
        U32 ts_sec;
        U32 ts_nsec;
        char msg [ 1 /* bytes */ ];

    private:

        void * operator new ( std :: size_t );
        void operator delete ( void * );
    };

}

#endif // _hpp_tools_common_log_protocol_
