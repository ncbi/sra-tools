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

#ifndef DIAGNOSTICSTREEITEM_H
#define DIAGNOSTICSTREEITEM_H

#include <QTreeWidgetItem>

#include <stdint.h>

typedef enum
{
    DTS_NotStarted,  /* test not started yet */
    DTS_Started,   /* test started */
    DTS_Succeed,   /* test finished successfully */
    DTS_Failed,    /* test finished with failure */
    DTS_Skipped,   /* test execution was skipped */
    DTS_Warning,   /* test finished successfully but has a warning for user */
    DTS_Paused,    /* KDiagnosePause was called */
    DTS_Resumed,   /* KDiagnoseResume was called */
    DTS_Canceled,  /* KDiagnoseCancel was called */
} DiagnosticsTestState;

class DiagnosticsTest
{
public:
    DiagnosticsTest ( QString p_name );
    ~DiagnosticsTest();

    void appendChild ( DiagnosticsTest *p_child );
    int childCount () const;
    DiagnosticsTest *getChild ( int p_row );
    QString getDescription ();
    uint32_t getLevel ();
    QString getName();
    DiagnosticsTest *getParent ();
    uint32_t getState ();
    int row () const;
    void setDescription ( QString p_desc );
    void setLevel ( uint32_t p_level );
    void setParent ( DiagnosticsTest *p_parent );
    void setState ( uint32_t p_state );

private:
    QList < DiagnosticsTest * > childItems;
    QString desc;
    uint32_t level;
    QString name;
    DiagnosticsTest *parent;
    uint32_t state;
};

#endif // DIAGNOSTICSTREEITEM_H
