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

        standardBackground . setColorAt ( 0, white );
        standardBackground . setColorAt ( 1, white );
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
    const QColor official_gray = QColor ( 213, 213, 213, 255 );

    const QColor modern_blue = QColor ( 0, 82, 155, 255 );
    const QColor modern_light_blue = QColor ( 125, 185, 255 );

    const QColor darkglass_transparent = QColor ( 0, 0, 0, 70);
};

#endif // SRATOOLKITTEMPLATE_H
