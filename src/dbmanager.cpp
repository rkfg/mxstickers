#include "dbmanager.h"
#include <QDebug>
#include <QDir>
#include <QMessageBox>
#include <QStandardPaths>

DBManager::DBManager(QObject* parent)
    : QObject(parent)
{
}

void DBManager::init()
{
    auto path = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    if (!QDir(path).mkpath(".")) {
        throw DBException("Не удалось создать директорию данных для приложения.");
    }
    m_db = QSqlDatabase::addDatabase("QSQLITE");
    m_db.setDatabaseName(path + "/mxs.db");
    if (!m_db.open()) {
        throw DBException("Не удалось открыть БД.");
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
    QString sql = "SELECT * FROM sticker WHERE 1=1";
    if (filter.isEmpty() || !global) {
        sql += " AND pack = :pack";
    }
    if (!filter.isEmpty()) {
        sql += " AND desc_index LIKE :filter";
    }
    q.prepare(sql);
    q.bindValue(":pack", pack);
    q.bindValue(":filter", "%" + filter.toLower() + "%");
    if (!q.exec()) {
        throw DBException("Не удалось получить список стикеров: " + q.lastError().text());
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
        throw DBException("Не удалось найти существующий стикер: " + q.lastError().text());
    }
    return q.record().value(0).toInt();
}

void DBManager::upgradeSchema()
{
    QSqlQuery q;
    if (!q.exec("SELECT COUNT(value) FROM schema_version")) {
        qWarning() << "Can't query schema version!";
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
    qDebug() << "Current DB version is" << version;
    try {
        while (version < 2) {
            qInfo() << "Upgrading from" << version << "to" << version + 1;
            switch (version) {
            case 1:
                if (!q.exec("ALTER TABLE sticker ADD COLUMN last_used_ts INTEGER")) {
                    throw version;
                }
                break;
            default:
                qWarning() << "Skipping upgrade from" << version << "to" << version + 1;
            }
            version++;
            q.prepare("UPDATE schema_version SET value = :v");
            q.bindValue(":v", version);
            if (!q.exec()) {
                throw version - 1;
            }
        }
    } catch (int v) {
        QMessageBox::critical(0, "Ошибка", QString("Ошибка обновления БД с версии %1 до %2").arg(v).arg(v + 1));
    }
}

void DBManager::addSticker(Sticker s)
{
    auto found = isExisting(s.code);
    if (found > 0) {
        qDebug() << "Стикер с кодом" << s.code << "уже есть в БД, не добавляем";
        return;
    }
    QSqlQuery q;
    q.prepare("INSERT INTO sticker (pack, server, code, type, description, desc_index) VALUES (:pack, :server, :code, :type, :description, :desc_index)");
    q.bindValue(":pack", s.pack);
    q.bindValue(":server", s.server);
    q.bindValue(":code", s.code);
    q.bindValue(":type", s.type);
    q.bindValue(":description", s.description);
    q.bindValue(":desc_index", s.description.toLower());
    if (!q.exec()) {
        throw DBException("Ошибка добавления стикера в БД: " + q.lastError().text());
    }
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
        throw DBException("Ошибка переименования стикера с кодом " + code + ": " + q.lastError().text());
    }
    qDebug() << "Renamed" << q.numRowsAffected() << "stickers";
}

void DBManager::removeSticker(const QString& code)
{
    isExisting(code);
    QSqlQuery q;
    q.prepare("DELETE FROM sticker WHERE code = :code");
    q.bindValue(":code", code);
    if (!q.exec()) {
        throw DBException("Ошибка удаления стикера с кодом " + code + ": " + q.lastError().text());
    }
    qDebug() << "Removed" << q.numRowsAffected() << "stickers";
}

void DBManager::renamePack(const QString& oldname, const QString& newname)
{
    QSqlQuery q;
    q.prepare("UPDATE sticker SET pack = :newname WHERE pack = :oldname");
    q.bindValue(":oldname", oldname);
    q.bindValue(":newname", newname);
    q.exec();
    qDebug() << q.numRowsAffected() << "stickers changed pack name";
}

void DBManager::moveStickerToPack(const QString& code, const QString& pack)
{
    QSqlQuery q;
    q.prepare("UPDATE sticker SET pack = :pack WHERE code = :code");
    q.bindValue(":pack", pack);
    q.bindValue(":code", code);
    q.exec();
    qDebug() << q.numRowsAffected() << "stickers moved";
}

void DBManager::updateRecentSticker(const QString& code)
{
    QSqlQuery q;
    q.prepare("UPDATE sticker SET last_used_ts = :ts WHERE code = :code");
    q.bindValue(":code", code);
    q.bindValue(":ts", QDateTime::currentSecsSinceEpoch());
    if (!q.exec()) {
        qWarning() << "Can't update sticker TS:" << q.lastError().text();
    }
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
