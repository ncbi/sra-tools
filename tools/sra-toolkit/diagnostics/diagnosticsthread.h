#ifndef DIAGNOSTICSTHREAD_H
#define DIAGNOSTICSTHREAD_H

#include <QObject>


#include <diagnose/diagnose.h>

struct KDiagnose;
struct KDiagnoseTest;
class DiagnosticsTest;

class DiagnosticsThread : public QObject
{
    Q_OBJECT

public:
    DiagnosticsThread ( QObject *parent = 0 );
    ~DiagnosticsThread ();

public slots:
    void begin ();

signals:
    void finished ();
    void handle ( DiagnosticsTest *test );

private:

    KDiagnose *diagnose;

};

#endif // DIAGNOSTICSTHREAD_H
