/*===========================================================================
*
*                            PUBLIC DOMAIN NOTICE
*               National Center for Biotechnology Information
*
*  This software/database is a "United States Government Work" under the
*  terms of the United States Copyright Act.  It was written as part of
*  the author's official duties as a United States Government employee and
*  thus cannot be copyrighted.  This software/database is freely available
*  to the public for use. The National Library of Medicine and the U.S.
*  Government have not placed any restriction on its use or reproduction.
*
*  Although all reasonable efforts have been taken to ensure the accuracy
*  and reliability of the software and data, the NLM and the U.S.
*  Government do not and cannot warrant the performance or results that
*  may be obtained by using this software or data. The NLM and the U.S.
*  Government disclaim all warranties, express or implied, including
*  warranties of performance, merchantability or fitness for any particular
*  purpose.
*
*  Please cite the author in any work or product based on this material.
*
* ===========================================================================
*
*/

#include "diagnosticsthread.h"
#include "diagnosticstest.h"

#include <diagnose/diagnose.h>
#include <klib/rc.h>
#include <kapp/main.h>

#include <QApplication>
#include <QTime>

#include <QDebug>

static
void delay ()
{
    int delay = qrand () % 250;
    QTime dieTime= QTime :: currentTime ( ) . addMSecs ( delay );
    while ( QTime :: currentTime() < dieTime )
        QCoreApplication::processEvents ( QEventLoop :: AllEvents );
}

static
void CC callback ( EKDiagTestState state, const KDiagnoseTest *diagnose_test, void *data )
{
    const char *name;

    DiagnosticsThread *self = static_cast <DiagnosticsThread *> ( data );

    if ( state == DTS_Canceled )
    {
        qDebug () << "canceled";
        emit self -> finished ();
        return;
    }

    delay ();
    rc_t rc = KDiagnoseTestName ( diagnose_test, &name );
    if ( rc ==  0 )
    {
        uint32_t test_level = 0;

        if ( state == DTS_Failed )
        {
            //KDiagnoseError *error  = KDiagnoseGetError ( self -> diagnose,
            qDebug () << "callback " << QString ( name ) << "- State: " << state;
        }

        rc = KDiagnoseTestLevel ( diagnose_test, &test_level );
        if ( rc == 0 )
        {
            DiagnosticsTest *test = new DiagnosticsTest ( QString ( name ) );
            test -> setLevel ( test_level );
            test -> setState ( state );

            self -> handle ( test );
        }
    }
}

DiagnosticsThread :: DiagnosticsThread ( QObject *parent )
    : QObject ( parent )
    , diagnose ( 0 )
{
    rc_t rc = 0;

    rc = KDiagnoseMakeExt ( &diagnose, nullptr, nullptr, nullptr, Quitting );
    if ( rc != 0 )
        qDebug () << rc;
    else
        KDiagnoseTestHandlerSet ( diagnose, callback, this );
}

DiagnosticsThread::~DiagnosticsThread ()
{
}

void DiagnosticsThread :: begin ()
{
    rc_t rc = KDiagnoseAll ( diagnose, 0 );
    if ( rc != 0 )
        qDebug () << "KDiagnoseAll failed";

    emit finished ();
}

void DiagnosticsThread :: cancel ()
{
    qDebug () << "about to cancel";
    rc_t rc = KDiagnoseCancel ( diagnose );
    if ( rc != 0 )
        qDebug () << rc;
    else
        qDebug () << "cancel request success";
}
