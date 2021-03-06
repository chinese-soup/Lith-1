#include "colortheme.h"


QString ColorTheme::getIcon(const QString &name) {
    switch (m_group) {
    case DARK:
        return "qrc:/navigation/dark/"+name+".png";
    case LIGHT:
    default:
        return "qrc:/navigation/light/"+name+".png";
    }
}

QColor ColorTheme::dim(const QColor &color) {
    return color.lighter();
}

QPalette ColorTheme::palette() const {
    auto mix = [](const QColor &a, const QColor &b, float ratio = 0.5) {
        return QColor {
            static_cast<int>(a.red() * ratio + b.red() * (1 - ratio)),
            static_cast<int>(a.green() * ratio + b.green() * (1 - ratio)),
            static_cast<int>(a.blue() * ratio + b.blue() * (1 - ratio)),
            static_cast<int>(a.alpha() * ratio + b.alpha() * (1 - ratio)),
        };
    };

    // TODO very likely needs a bit of tweaking to differentiate button, window and base
    QColor windowText { m_weechatColors[0] };
    QColor button { m_weechatColors[1] };
    QColor light = mix(windowText, button, 0.4f);
    QColor dark = mix(button, windowText, 0.4f);
    QColor mid = mix(light, dark);
    QColor text = windowText;
    QColor bright_text = text;
    QColor base = button;
    QColor window = button;
    return QPalette {
        windowText,
        button,
        light,
        dark,
        mid,
        text,
        bright_text,
        base,
        window
    };
}
