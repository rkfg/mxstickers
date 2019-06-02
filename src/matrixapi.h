#ifndef MATRIXAPI_H
#define MATRIXAPI_H

#include "roomseditorpage.h"

#include <QNetworkAccessManager>
#include <QObject>

struct MXSync {
    QList<Room> rooms;
    QString since;
};

class MatrixAPI : public QObject {
    Q_OBJECT
public:
    QString m_since;
    QString m_access_token;
    QString m_server;
    explicit MatrixAPI(QNetworkAccessManager& network, QObject* parent = nullptr);
    void setSince(const QString& since);
    void sync();

private:
    QNetworkAccessManager& m_network;
    QString buildRequest(const QString& method, const QString& type);
signals:
    void syncComplete(MXSync sync);
public slots:
private slots:
    void syncRequestFinished(QNetworkReply* rep);
};

#endif // MATRIXAPI_H
