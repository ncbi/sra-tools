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

#ifndef SRATOOLKITTEMPLATE_H
#define SRATOOLKITTEMPLATE_H

#include <QColor>
#include <QLinearGradient>


#define ALPHA_90 229
#define ALPHA_80 204
#define ALPHA_70 178
#define ALPHA_60 153
#define ALPHA_50 127
#define ALPHA_40 102
#define ALPHA_30 76
#define ALPHA_20 51
#define ALPHA_10 25

enum TemplateType { Official, Modern, DarkGlass };

class SRAToolkitTemplate
{
public:
    SRAToolkitTemplate ( TemplateType type = Official )
        : baseGradient ( QLinearGradient () )
        , standardBackground ( QLinearGradient () )
    {
        switch ( type )
        {
        case Official:
            initOfficialTemplate ();
            break;
        case Modern:
            initModernTemplate ();
            break;
        case DarkGlass:
            initDarkGlassTemplate ();
            break;
        }
    }

    QLinearGradient getBaseGradient () { return baseGradient; }
    QLinearGradient getStandardBackground () { return standardBackground; }

private:

    static
    QColor getAlphaColor ( QColor p_color, int alpa )
    {
        QColor color = p_color;
        color.setAlpha ( alpa );

        return color;
    }

    void initOfficialTemplate ()
    {
        baseGradient . setColorAt ( 0, official_blue );
        baseGradient . setColorAt ( 1, official_blue );

        standardBackground . setColorAt ( 0, official_gray );
        standardBackground . setColorAt ( 1, official_gray );
    }

    void initModernTemplate ()
    {
        baseGradient . setColorAt ( 0, modern_blue );
        baseGradient . setColorAt ( 0.35, modern_light_blue );
        baseGradient . setColorAt ( 0.65, modern_light_blue );
        baseGradient . setColorAt ( 1, modern_blue );
    }

    void initDarkGlassTemplate ()
    {
        baseGradient . setColorAt ( 0, darkglass_transparent );
        baseGradient . setColorAt ( 1, darkglass_transparent );
    }

    QColor baseColor;
    QLinearGradient baseGradient;
    QLinearGradient standardBackground;

    const QColor black =  QColor ( 0, 0, 0, 255 );
    const QColor white =  QColor ( 255, 255, 255, 255 );

    const QColor official_blue = QColor ( 54, 103, 151, 255 );
    const QColor official_gray = QColor ( 245, 245, 245, 255 );

    const QColor modern_blue = QColor ( 0, 82, 155, 255 );
    const QColor modern_light_blue = QColor ( 125, 185, 255 );

    const QColor darkglass_transparent = QColor ( 0, 0, 0, ALPHA_30 );
};

#endif // SRATOOLKITTEMPLATE_H
