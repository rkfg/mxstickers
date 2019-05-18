#ifndef QZIP_H
#define QZIP_H

#include "zip.h"
#include <QObject>
#include <QString>

class QZipEntry {
public:
    QByteArray read();
    void fread(const QString& filename);
    void write(const QByteArray& data);
    void fwrite(const QString& filename);
    QString name();
    ~QZipEntry();

private:
    friend class QZip;
    zip_t* m_zip;
    QZipEntry(zip_t* zip);
};

class QZip {
public:
    QZip();
    void open(const QString& name, int level, char mode);
    QZipEntry openEntry(const QString& name);
    QZipEntry openEntry(int idx);
    int count();
    ~QZip();

private:
    zip_t* m_zip;
};

#endif // QZIP_H
