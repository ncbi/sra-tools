#include "diagnosticsthread.h"
#include "diagnosticstest.h"

#include <diagnose/diagnose.h>
#include <klib/rc.h>

#include <QDebug>

static void CC callback ( EKDiagTestState state, const KDiagnoseTest *diagnose_test, void *data );

DiagnosticsThread::DiagnosticsThread ( QObject *parent )
    : QObject( parent )
    , diagnose ( 0 )
{
    rc_t rc = 0;

    rc = KDiagnoseMakeExt ( &diagnose, nullptr, nullptr, nullptr );
    if ( rc != 0 )
        qDebug () << rc;
    else
        KDiagnoseTestHandlerSet ( diagnose, callback, this );
}

DiagnosticsThread::~DiagnosticsThread ()
{
}

static
void CC callback ( EKDiagTestState state, const KDiagnoseTest *diagnose_test, void *data )
{
    const char *name;

    DiagnosticsThread *self = static_cast <DiagnosticsThread *> ( data );

    rc_t rc = KDiagnoseTestName ( diagnose_test, &name );
    if ( rc ==  0 )
    {
        uint32_t test_level = 0;

        rc = KDiagnoseTestLevel ( diagnose_test, &test_level );
        if ( rc == 0 )
        {
            DiagnosticsTest *test = new DiagnosticsTest ( QString ( name ) );
            test -> setLevel ( test_level );
            test -> setState ( state );

/*
            const KDiagnoseTestDesc *desc;
            rc = KDiagnoseGetDesc ( ds -> diagnose, &desc );
            if ( rc != 0 )
                qDebug () << "Failed to get test description object for test: " << QString ( name );
            else
            {
                const char *buf;
                rc = KDiagnoseTestDescDesc ( desc, &buf );
                if ( rc != 0 )
                    qDebug () << "failed to get description for test: " << QString ( name );
                else
                   // test -> setDescription ( QString ( buf ) );
                    qDebug () << QString ( buf );
            }
*/

            self -> handle ( test );
        }
    }
}


void DiagnosticsThread :: begin ()
{
    rc_t rc = KDiagnoseAll ( diagnose, 0 );
    if ( rc != 0 )
        qDebug () << rc;

    emit finished ();
}
