#ifndef ARCHIVEMANAGER_H
#define ARCHIVEMANAGER_H

#include "dbmanager.h"
#include "zip.h"

#include <QObject>
#include <QString>

using uzip = std::unique_ptr<zip_t, decltype(&zip_close)>;

class ArchiveManager : public QObject {
    Q_OBJECT
public:
    ArchiveManager(DBManager* db_manager, QObject* parent = nullptr);
    void exportPack(const QString& pack, const QString& packname);
    QString importPack(const QString& packname);

private:
    QPair<QString, QString> takeLine(QStringList& lines);
    QPair<QString, QString> serverCode(const QString& key);
    DBManager* m_dbmanager;
    auto qzip_entry_open(const uzip& zip, const char* entryname);
    uzip qzip_open(const QString& name, int level, char mode);
};

#endif // ARCHIVEMANAGER_H
