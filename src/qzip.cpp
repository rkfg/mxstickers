#include "qzip.h"

QZip::QZip()
{
}

void QZip::open(const QString& name, int level, char mode)
{
    m_zip = zip_open(name.toUtf8(), level, mode);
    if (!m_zip) {
        throw QObject::tr("ошибка открытия архива %1 в режиме '%2'").arg(name).arg(QChar(mode));
    }
}

QZipEntry QZip::openEntry(const QString& name)
{
    auto r = zip_entry_open(m_zip, name.toUtf8());
    if (r) {
        throw QObject::tr("ошибка открытия записи %1: %2").arg(name).arg(r);
    }
    return QZipEntry(m_zip);
}

QZipEntry QZip::openEntry(int idx)
{
    auto r = zip_entry_openbyindex(m_zip, idx);
    if (r) {
        throw QObject::tr("ошибка открытия записи %1: %2").arg(idx).arg(r);
    }
    return QZipEntry(m_zip);
}

int QZip::count()
{
    return zip_total_entries(m_zip);
}

QZip::~QZip()
{
    zip_close(m_zip);
}

QByteArray QZipEntry::read()
{
    QByteArray result;
    result.resize(zip_entry_size(m_zip));
    if (zip_entry_noallocread(m_zip, result.data(), result.size()) < 0) {
        throw QObject::tr("ошибка чтения записи в архиве.");
    }
    return result;
}

void QZipEntry::fread(const QString& filename)
{
    if (zip_entry_fread(m_zip, filename.toUtf8())) {
        throw QObject::tr("ошибка извлечения файла из архива в %1").arg(filename);
    }
}

void QZipEntry::write(const QByteArray& data)
{
    if (zip_entry_write(m_zip, data, data.length())) {
        throw QObject::tr("ошибка записи файла в архив.");
    }
}

void QZipEntry::fwrite(const QString& filename)
{
    if (zip_entry_fwrite(m_zip, filename.toUtf8())) {
        throw QObject::tr("ошибка записи файла %1 в архив.").arg(filename);
    }
}

QString QZipEntry::name()
{
    return zip_entry_name(m_zip);
}

QZipEntry::QZipEntry(zip_t* zip)
    : m_zip(zip)
{
}

QZipEntry::~QZipEntry()
{
    zip_entry_close(m_zip);
}
