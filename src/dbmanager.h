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

class Transaction {
public:
    ~Transaction();

private:
    friend class DBManager;
    Transaction();
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
    QList<Sticker> getStickers(const QString& pack, const QString& filter, bool global);
    bool addSticker(Sticker s);
    void renameSticker(const QString& code, const QString& description);
    void removeSticker(const QString& code);
    void removePack(const QString& pack);
    void renamePack(const QString& oldname, const QString& newname);
    void moveStickerToPack(const QString& code, const QString& pack);
    void updateRecentSticker(const QString& code);
    QStringList getTags(const QString& code);
    void setTags(const QString& code, const QStringList& tags);
    Transaction startTransaction();
    bool packExists(const QString& pack);

private:
    QSqlDatabase m_db;
    int isExisting(const QString& code);
    void upgradeSchema();
};

#endif // DBMANAGER_H
