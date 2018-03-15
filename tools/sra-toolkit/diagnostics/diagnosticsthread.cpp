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
