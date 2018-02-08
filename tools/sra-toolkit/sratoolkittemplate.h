#ifndef SRATOOLKITTEMPLATE_H
#define SRATOOLKITTEMPLATE_H

#include <QColor>
#include <QLinearGradient>

enum TemplateType { Official, Modern, DarkGlass };

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
        case DarkGlass:
            initDarkGlassTemplate ();
            break;
        }
    }

    QColor getBaseColor () { return baseColor; }
    QLinearGradient getBaseGradient () { return baseGradient; }

private:

    void initOfficialTemplate ()
    {
        baseColor = official_blue;

        baseGradient = QLinearGradient ();
        baseGradient . setColorAt ( 0, baseColor );
        baseGradient . setColorAt ( 1, baseColor );
    }

    void initModernTemplate ()
    {
        baseColor = modern_blue;

        baseGradient = QLinearGradient ();

        baseGradient . setColorAt ( 0, baseColor );
        baseGradient . setColorAt ( 0.35, modern_light_blue );
        baseGradient . setColorAt ( 0.65, modern_light_blue );
        baseGradient . setColorAt ( 1, baseColor );
    }

    void initDarkGlassTemplate ()
    {
        baseColor = darkglass_transparent;

        baseGradient = QLinearGradient ();
        baseGradient . setColorAt ( 0, baseColor );
        baseGradient . setColorAt ( 1, baseColor );
    }

    QColor baseColor;
    QLinearGradient baseGradient;


    const QColor official_blue = QColor ( 54, 103, 151, 255 );

    const QColor modern_blue = QColor ( 0, 82, 155, 255 );
    const QColor modern_light_blue = QColor ( 125, 185, 255 );

    const QColor darkglass_transparent = QColor ( 0, 0, 0, 50);
};

#endif // SRATOOLKITTEMPLATE_H
