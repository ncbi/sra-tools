#ifndef SRATOOLKITTEMPLATE_H
#define SRATOOLKITTEMPLATE_H

#include <QColor>
#include <QLinearGradient>

enum TemplateType { Official, Modern };

class SRAToolkitTemplate
{
public:
    SRAToolkitTemplate ( TemplateType type = Official )
    {
        switch ( type )
        {
        case Official:
            initOfficialTemplate ();
            break;
        case Modern:
            initModernTemplate ();
            break;
        }
    }

    QColor getBaseColor () { return baseColor; }
    QLinearGradient getBaseGradient () { return baseGradient; }

private:

    void initOfficialTemplate ()
    {
        baseColor = QColor ( 0, 82, 155, 255 );
        baseGradient = QLinearGradient ();
    }

    void initModernTemplate ()
    {
        baseColor = QColor ( 0, 82, 155 );

        baseGradient = QLinearGradient ();
        baseGradient . setColorAt ( 0, baseColor );
        baseGradient . setColorAt ( 0.5, QColor ( 41, 137, 216 ) );
        baseGradient . setColorAt ( 1, QColor ( 125, 185, 232 ) );
    }

    QColor baseColor;
    QLinearGradient baseGradient;
};

#endif // SRATOOLKITTEMPLATE_H
