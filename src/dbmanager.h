#ifndef DBMANAGER_H
#define DBMANAGER_H

#include <QObject>
#include <QtSql>

struct Sticker {
    qlonglong id;
    QString description;
    QString server;
    QString code;
    QString pack;
    QString type;
    QString path() const;
};

class DBException : public QException {
public:
    DBException(const QString& what);
    const QString& qwhat() const noexcept;

private:
    QString m_what;
};

class DBManager : public QObject {
    Q_OBJECT
public:
    DBManager(QObject* parent = nullptr);
    void init();
    QList<Sticker> getStickers(const QString& pack, const QString &filter, bool global);
    void addSticker(Sticker s);
    void renameSticker(const QString& code, const QString& description);
    void removeSticker(const QString& code);
    void renamePack(const QString& oldname, const QString& newname);
    void moveStickerToPack(const QString& code, const QString& pack);
private:
    QSqlDatabase m_db;
    int isExisting(const QString& code);
};

#endif // DBMANAGER_H
