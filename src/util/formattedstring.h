// Lith
// Copyright (C) 2021 Martin Bříza
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; If not, see <http://www.gnu.org/licenses/>.

#ifndef FORMATTEDSTRING_H
#define FORMATTEDSTRING_H

#include <QObject>
#include <QString>
#include <QList>

#include "colortheme.h"

class FormattedString {
    Q_GADGET
public:
    struct Part {
        struct Color {
            int32_t index { 0 };
            bool extended { false };
        };

        Part(const QString &text) : text(text) {}
        bool containsHtml() { return foreground.index >= 0 || background.index >= 0 || hyperlink || bold || underline || italic; }
        QString toHtml(const ColorTheme &theme) const;

        QString text {};
        Color foreground;
        Color background;
        bool hyperlink { false };
        bool bold { false };
        bool underline { false };
        bool italic;
    };

    FormattedString();
    FormattedString(const char *d);
    FormattedString(const QString &o);
    FormattedString(QString &&o);

    FormattedString &operator=(const QString &o);
    FormattedString &operator=(QString &&o);
    FormattedString &operator=(const char *o);

    bool operator==(const FormattedString &o);
    bool operator!=(const FormattedString &o);
    bool operator==(const QString &o);
    bool operator!=(const QString &o);

    // these methods append to the last available segment
    FormattedString &operator+=(const char *s);
    FormattedString &operator+=(const QString &s);

    operator QString() const;

    Q_INVOKABLE QString toPlain() const;
    Q_INVOKABLE QString testHtml() const;
    Q_INVOKABLE QString toHtml(const ColorTheme &theme) const;
    Q_INVOKABLE QString toTrimmedHtml(int n) const;

    bool containsHtml();

    void clear();

    Part &addPart(const Part &p = {{}});
    Part &lastPart();
    // prune would potentially (not 100% done) remove all empty parts and merge the ones with the same formatting
    void prune();

    // QString compatibility wrappers
    QStringList split(const QString &sep) const;
    qlonglong toLongLong(bool *ok = nullptr, int base = 10) const;
    QString toLower() const;
    std::string toStdString() const;

private:
    QList<Part> m_parts {};
};

Q_DECLARE_METATYPE(FormattedString)

#endif // FORMATTEDSTRING_H
