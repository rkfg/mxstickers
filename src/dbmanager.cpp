#include "dbmanager.h"
#include <QDebug>
#include <QDir>
#include <QMessageBox>
#include <QStandardPaths>

#define CURRENT_SCHEMA_VERSION 3

DBManager::DBManager(QObject* parent)
    : QObject(parent)
{
}

void DBManager::init()
{
    auto path = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    if (!QDir(path).mkpath(".")) {
        throw DBException(tr("Can't create the data directory."));
    }
    m_db = QSqlDatabase::addDatabase("QSQLITE");
    m_db.setDatabaseName(path + "/mxs.db");
    if (!m_db.open()) {
        throw DBException(tr("Can't open the database."));
    }
    QSqlQuery q;
    q.exec("CREATE TABLE `sticker` ( `id` INTEGER NOT NULL PRIMARY KEY AUTOINCREMENT, `pack` TEXT NOT NULL, `server` TEXT NOT NULL, `code` TEXT NOT NULL, "
           "`type` TEXT NOT NULL, `description` TEXT, `desc_index` TEXT )");
    q.exec("CREATE TABLE `schema_version` (`value` INTEGER NOT NULL)");
    upgradeSchema();
}

QList<Sticker> DBManager::getStickers(const QString& pack, const QString& filter, bool global)
{
    QSqlQuery q;
    QString sql = "SELECT * FROM sticker LEFT JOIN tag ON tag.code = sticker.code WHERE 1=1";
    if (!pack.isEmpty() && (filter.isEmpty() || !global)) {
        sql += " AND pack = :pack";
    }
    if (!filter.isEmpty()) {
        sql += " AND (desc_index LIKE :filter OR tag LIKE :tag)";
    }
    sql += " GROUP BY sticker.code";
    if (pack.isEmpty()) { // show recents
        sql += " ORDER BY last_used_ts DESC LIMIT 5";
    }
    q.prepare(sql);
    q.bindValue(":pack", pack);
    q.bindValue(":filter", "%" + filter.toLower() + "%");
    q.bindValue(":tag", filter.toLower() + "%");
    if (!q.exec()) {
        throw DBException(tr("Can't get the sticker list: %1").arg(q.lastError().text()));
    }
    QList<Sticker> result;
    while (q.next()) {
        result << Sticker { q.value("id").toLongLong(), q.value("description").toString(), q.value("server").toString(), q.value("code").toString(), q.value("pack").toString(), q.value("type").toString() };
    }
    return result;
}

int DBManager::isExisting(const QString& code)
{
    QSqlQuery q;
    q.prepare("SELECT COUNT(id) FROM sticker WHERE code = :code");
    q.bindValue(":code", code);
    if (!q.exec() || !q.next()) {
        throw DBException(tr("Can't find existing sticker with code %1: %2").arg(code).arg(q.lastError().text()));
    }
    return q.record().value(0).toInt();
}

void DBManager::upgradeSchema()
{
    QSqlQuery q;
    if (!q.exec("SELECT COUNT(value) FROM schema_version")) {
        qWarning() << tr("Can't query schema version!");
        return;
    }
    q.next();
    if (q.record().value(0).toInt() == 0) {
        q.exec("INSERT INTO schema_version(value) VALUES(1)");
    }
    if (!q.exec("SELECT value FROM schema_version")) {
        qWarning() << "Can't query schema version!";
        return;
    }
    q.next();
    int version = q.record().value(0).toInt();
    qDebug() << tr("Current DB version is %1").arg(version);
    try {
        auto t = startTransaction();
        while (version < CURRENT_SCHEMA_VERSION) {
            qInfo() << tr("Upgrading from %1 to %2").arg(version).arg(version + 1);
            bool r = true;
            switch (version) {
            case 1:
                r &= q.exec("ALTER TABLE sticker ADD COLUMN last_used_ts INTEGER");
                if (!r) {
                    throw version;
                }
                break;
            case 2:
                r &= q.exec("CREATE TABLE `tag` ( `code` TEXT NOT NULL, `tag` TEXT NOT NULL )");
                r &= q.exec("CREATE INDEX `code_idx` ON `tag` ( `code` )");
                r &= q.exec("CREATE INDEX `desc_idx` ON `sticker` ( `desc_index` )");
                r &= q.exec("CREATE INDEX `last_used_idx` ON `sticker` ( `last_used_ts` )");
                r &= q.exec("CREATE INDEX `pack_idx` ON `sticker` ( `pack` )");
                r &= q.exec("CREATE INDEX `scode_idx` ON `sticker` ( `code` )");
                r &= q.exec("CREATE INDEX `tag_idx` ON `tag` ( `tag` )");
                if (!r) {
                    throw version;
                }
                break;
            default:
                qWarning() << tr("Skipping upgrade from %1 to %2").arg(version).arg(version + 1);
            }
            version++;
            q.prepare("UPDATE schema_version SET value = :v");
            q.bindValue(":v", version);
            if (!q.exec()) {
                throw version - 1;
            }
        }
    } catch (int v) {
        QMessageBox::critical(0, tr("Error"), tr("Error updating database version from %1 to %2").arg(v).arg(v + 1));
    }
}

bool DBManager::addSticker(Sticker s, bool update)
{
    QSqlQuery q;
    auto found = isExisting(s.code);
    if (found > 0) {
        if (!update) {
            qDebug() << tr("Sticker with code %1 already present in the database, not adding.").arg(s.code);
            return false;
        } else {
            q.prepare("UPDATE sticker SET pack = :pack, description = :description, desc_index = :desc_index WHERE code = :code");
        }
    } else {
        q.prepare("INSERT INTO sticker (pack, server, code, type, description, desc_index) VALUES (:pack, :server, :code, :type, :description, :desc_index)");
    }
    q.bindValue(":pack", s.pack);
    q.bindValue(":server", s.server);
    q.bindValue(":code", s.code);
    q.bindValue(":type", s.type);
    q.bindValue(":description", s.description);
    q.bindValue(":desc_index", s.description.toLower());
    if (!q.exec()) {
        throw DBException(tr("Error adding sticker with code %1 to the database: %2").arg(s.code).arg(q.lastError().text()));
    }
    return true;
}

void DBManager::renameSticker(const QString& code, const QString& description)
{
    isExisting(code);
    QSqlQuery q;
    q.prepare("UPDATE sticker SET description = :desc, desc_index = :desc_index WHERE code = :code");
    q.bindValue(":desc", description);
    q.bindValue(":desc_index", description.toLower());
    q.bindValue(":code", code);
    if (!q.exec()) {
        throw DBException(tr("Error renaming sticker with code %1: %2").arg(code).arg(q.lastError().text()));
    }
    qDebug() << tr("Renamed %1 stickers").arg(q.numRowsAffected());
}

void DBManager::removeSticker(const QString& code)
{
    isExisting(code);
    QSqlQuery q;
    q.prepare("DELETE FROM sticker WHERE code = :code");
    q.bindValue(":code", code);
    if (!q.exec()) {
        throw DBException(tr("Error deleting sticker with code %1: %2").arg(code).arg(q.lastError().text()));
    }
    qDebug() << "Removed" << q.numRowsAffected() << "stickers";
}

void DBManager::removePack(const QString& pack)
{
    auto t = startTransaction();
    QSqlQuery q;
    q.prepare("DELETE FROM tag WHERE code IN (SELECT code FROM sticker WHERE pack = :pack)");
    q.bindValue(":pack", pack);
    if (!q.exec()) {
        throw DBException(tr("Error removing tags from stickerpack %1: %2").arg(pack).arg(q.lastError().text()));
    }
    qDebug() << tr("Removed %1 tags").arg(q.numRowsAffected());
    q.prepare("DELETE FROM sticker WHERE pack = :pack");
    q.bindValue(":pack", pack);
    if (!q.exec()) {
        throw DBException(tr("Error removing stickerpack %1: %2").arg(pack).arg(q.lastError().text()));
    }
    qDebug() << tr("Removed %1 stickerpacks").arg(q.numRowsAffected());
}

void DBManager::renamePack(const QString& oldname, const QString& newname)
{
    QSqlQuery q;
    q.prepare("UPDATE sticker SET pack = :newname WHERE pack = :oldname");
    q.bindValue(":oldname", oldname);
    q.bindValue(":newname", newname);
    q.exec();
    qDebug() << tr("%1 stickers changed pack name").arg(q.numRowsAffected());
}

void DBManager::moveStickerToPack(const QString& code, const QString& pack)
{
    QSqlQuery q;
    q.prepare("UPDATE sticker SET pack = :pack WHERE code = :code");
    q.bindValue(":pack", pack);
    q.bindValue(":code", code);
    q.exec();
    qDebug() << tr("%1 stickers moved").arg(q.numRowsAffected());
}

void DBManager::updateRecentSticker(const QString& code)
{
    QSqlQuery q;
    q.prepare("UPDATE sticker SET last_used_ts = :ts WHERE code = :code");
    q.bindValue(":code", code);
    q.bindValue(":ts", (qlonglong)time(NULL));
    if (!q.exec()) {
        qWarning() << tr("Can't update sticker TS: %1").arg(q.lastError().text());
    }
}

QStringList DBManager::getTags(const QString& code)
{
    QStringList result;
    QSqlQuery q;
    QString sql = "SELECT tag FROM tag";
    if (!code.isEmpty()) {
        sql += " WHERE code = :code";
    }
    sql += " GROUP BY tag";
    q.prepare(sql);
    q.bindValue(":code", code);
    if (!q.exec()) {
        qWarning() << tr("Can't get tags: %1").arg(q.lastError().text());
        return result;
    }
    while (q.next()) {
        result << q.record().value(0).toString();
    }
    return result;
}

void DBManager::setTags(const QString& code, const QStringList& tags)
{
    QSqlQuery q;
    q.prepare("DELETE FROM tag WHERE code = :code");
    q.bindValue(":code", code);
    if (!q.exec()) {
        qWarning() << tr("Can't remove existing tags: %1").arg(q.lastError().text());
    }
    q.prepare("INSERT INTO tag(code, tag) VALUES(:code, :tag)");
    q.bindValue(":code", code);
    for (auto& t : tags) {
        q.bindValue(":tag", t);
        q.exec();
    }
}

Transaction DBManager::startTransaction()
{
    return Transaction();
}

bool DBManager::packExists(const QString& pack)
{
    QSqlQuery q;
    q.prepare("SELECT COUNT(id) FROM sticker WHERE pack = :pack");
    q.bindValue(":pack", pack);
    q.exec();
    q.next();
    return q.value(0).toInt() > 0;
}

DBException::DBException(const QString& what)
    : m_what(what)
{
}

const QString& DBException::qwhat() const noexcept
{
    return m_what;
}

QString Sticker::path() const
{
    return QString("packs/%1/%2_%3.%4").arg(pack).arg(server).arg(code).arg(type);
}

Transaction::Transaction()
{
    QSqlQuery q;
    q.exec("BEGIN TRANSACTION");
    qDebug() << QObject::tr("Transaction started");
}

Transaction::~Transaction()
{
    if (std::uncaught_exception()) {
        QSqlQuery q;
        q.exec("ROLLBACK");
        qDebug() << QObject::tr("Transaction rolled back due to exception");
    } else {
        QSqlQuery q;
        q.exec("COMMIT");
        qDebug() << QObject::tr("Transaction committed");
    }
}
