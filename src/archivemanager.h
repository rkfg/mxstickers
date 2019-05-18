#ifndef ARCHIVEMANAGER_H
#define ARCHIVEMANAGER_H

#include "dbmanager.h"
#include "zip.h"

#include <QObject>
#include <QString>

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
};

#endif // ARCHIVEMANAGER_H
