#include "dbmanager.h"
#include <QDebug>
#include <QDir>
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
    if (q.exec("CREATE TABLE `sticker` ( `id` INTEGER NOT NULL PRIMARY KEY AUTOINCREMENT, `pack` TEXT NOT NULL, `server` TEXT NOT NULL, `code` TEXT NOT NULL, "
               "`type` TEXT NOT NULL, `description` TEXT, `desc_index` TEXT )")) {
        qDebug() << "Таблица стикеров создана";
    } else {
        qDebug() << "Ошибка при создании таблицы стикеров:" << q.lastError();
    }
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
    qDebug() << "Renamed" << q.record().value(0) << "stickers";
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
    qDebug() << "Removed" << q.record().value(0) << "stickers";
}

void DBManager::renamePack(const QString& oldname, const QString& newname)
{
    QSqlQuery q;
    q.prepare("UPDATE sticker SET pack = :newname WHERE pack = :oldname");
    q.bindValue(":oldname", oldname);
    q.bindValue(":newname", newname);
    q.exec();
    qDebug() << q.record().value(0) << "stickers changed pack name";
}

void DBManager::moveStickerToPack(const QString& code, const QString& pack)
{
    QSqlQuery q;
    q.prepare("UPDATE sticker SET pack = :pack WHERE code = :code");
    q.bindValue(":pack", pack);
    q.bindValue(":code", code);
    q.exec();
    qDebug() << q.record().value(0) << "stickers moved";
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
