/* Homepage http://facetracknoir.sourceforge.net/home/default.htm
 *
 * ISC License (ISC)                                                             *
 *                                                                               *
 * Copyright (c) 2015, Wim Vriend                                                *
 *                                                                               *
 * Permission to use, copy, modify, and/or distribute this software for any      *
 * purpose with or without fee is hereby granted, provided that the above        *
 * copyright notice and this permission notice appear in all copies.             *
 */

#include "csv.h"
#include "opentrack/library-path.hpp"
#include <QTextDecoder>
#include <QFile>
#include <QCoreApplication>
#include <QString>

#include <QDebug>

#include <utility>
#include <algorithm>

using std::move;

const QTextCodec* CSV::m_codec = QTextCodec::codecForName("System");
const QRegExp CSV::m_rx = QRegExp(QStringLiteral("((?:(?:[^;\\n]*;?)|(?:\"[^\"]*\";?))*)?\\n?"));
const QRegExp CSV::m_rx2 = QRegExp(QStringLiteral("(?:\"([^\"]*)\";?)|(?:([^;]*);?)?"));

CSV::CSV(QIODevice* device) :
    m_device(device),
    m_pos(0)
{
    if (m_device && m_device->isReadable())
    {
        QTextDecoder dec(m_codec);
        m_string = dec.toUnicode(m_device->readAll());
    }
}

QString CSV::readLine()
{
    QString line;

    if ((m_pos = m_rx.indexIn(m_string,m_pos)) != -1)
    {
        line = m_rx.cap(1);
        m_pos += m_rx.matchedLength();
    }
    else
    {
        static const QChar lf(QChar::LineFeed);

        while (m_pos < m_string.length())
            if (m_string[m_pos++] == lf)
                break;
    }
    return line;
}

bool CSV::parseLine(QStringList& ret)
{
    QString line(move(readLine()));

    QStringList list;
    int pos2 = 0;

    if (line.size() == 0)
    {
        ret = move(QStringList());
        return m_device->size() > m_pos;
    }
    else
    {
        while (line.size() > pos2 && (pos2 = m_rx2.indexIn(line, pos2)) != -1)
        {
            QString col;
            if (m_rx2.cap(1).size() > 0)
                col = move(m_rx2.cap(1));
            else if (m_rx2.cap(2).size() > 0)
                col = move(m_rx2.cap(2));

            list << move(col);

            if (col.size())
                pos2 += m_rx2.matchedLength();
            else
                pos2++;
        }
    }
    ret = move(list);
    return true;
}

bool CSV::getGameData(int id, unsigned char* table, QString& gamename)
{
    for (int i = 0; i < 8; i++)
        table[i] = 0;

    QString id_str(move(QString::number(id)));

    static const QString libexec_path(QStringLiteral("./") +
                                      QString(opentrack_library_path) +
                                      QStringLiteral("/settings/facetracknoir supported games.csv"));

    QFile file(QCoreApplication::applicationDirPath() + libexec_path);

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return false;

    CSV csv(&file);

    int lineno = 0;
    unsigned tmp[8];
    unsigned fuzz[3];

    QStringList gameLine;

    while (lineno++, csv.parseLine(gameLine))
    {
        //qDebug() << "Column 0: " << gameLine.at(0);		// No.
        //qDebug() << "Column 1: " << gameLine.at(1);		// Game Name
        //qDebug() << "Column 2: " << gameLine.at(2);		// Game Protocol
        //qDebug() << "Column 3: " << gameLine.at(3);		// Supported since version
        //qDebug() << "Column 4: " << gameLine.at(4);		// Verified
        //qDebug() << "Column 5: " << gameLine.at(5);		// By
        //qDebug() << "Column 6: " << gameLine.at(6);		// International ID
        //qDebug() << "Column 7: " << gameLine.at(7);		// FaceTrackNoIR ID

        if (gameLine.count() == 8)
        {
            if (gameLine.at(6).compare(id_str, Qt::CaseInsensitive) == 0)
            {
                const QString proto(move(gameLine.at(3)));
                const QString name(move(gameLine.at(1)));

                const QByteArray id_cstr = move(gameLine.at(7).toLatin1());

                if (proto == QStringLiteral("V160"))
                {
                    /* nothing */
                }
                else if (id_cstr.length() != 22 ||
                         sscanf(id_cstr.constData(),
                                "%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x",
                                fuzz + 2,
                                fuzz + 0,
                                tmp + 3,
                                tmp + 2,
                                tmp + 1,
                                tmp + 0,
                                tmp + 7,
                                tmp + 6,
                                tmp + 5,
                                tmp + 4,
                                fuzz + 1) != 11)
                {
                    qDebug() << "scanf failed" << lineno;
                }
                else
                {
                    for (int i = 0; i < 8; i++)
                    {
                        using t = unsigned char;
                        table[i] = t(tmp[i]);
                    }
                }
                qDebug() << "game-id" << id_str << "proto" << proto;
                gamename = move(name);
                return true;
            }
        }
        else
        {
            qDebug() << "malformed csv line" << lineno;
        }
    }

    qDebug() << "unknown game connected" << id;

    return false;
}
