#include "matrixapi.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMessageBox>
#include <QNetworkReply>

MatrixAPI::MatrixAPI(QNetworkAccessManager& network, QObject* parent)
    : QObject(parent)
    , m_network(network)
{
}

void MatrixAPI::setSince(const QString& since)
{
    m_since = since;
}

void MatrixAPI::sync()
{
    QNetworkRequest req;
    auto reqs = buildRequest("sync", "client");
    if (!m_since.isEmpty()) {
        reqs += "&since=" + m_since;
    }
    req.setUrl(reqs);
    auto rep = m_network.get(req);
    connect(rep, &QNetworkReply::finished, [=] { syncRequestFinished(rep); });
}

void MatrixAPI::syncRequestFinished(QNetworkReply* rep)
{
    if (rep->error() != QNetworkReply::NoError) {
        qWarning() << tr("Error quering Matrix: %1").arg(QString::fromUtf8(rep->readAll()));
        return;
    }
    auto root = QJsonDocument::fromJson(rep->readAll()).object();
    auto rooms = root["rooms"].toObject()["join"].toObject();
    MXSync result;
    for (auto i = rooms.constBegin(); i != rooms.constEnd(); ++i) {
        Room room;
        room.address = i.key();
        auto events = rooms[room.address].toObject()["state"].toObject()["events"].toArray();
        for (int e = 0; e < events.size(); ++e) {
            auto event = events[e].toObject();
            if (event["type"] == "m.room.name") {
                room.name = event["content"].toObject()["name"].toString();
            }
            if (event["type"] == "m.room.create") {
                room.creator = event["content"].toObject()["creator"].toString();
            }
        }
        result.rooms.append(room);
    }
    m_since = root["next_batch"].toString();
    result.since = m_since;
    emit syncComplete(result);
}

QString MatrixAPI::buildRequest(const QString& method, const QString& type)
{
    if (m_access_token.isEmpty()) {
        QMessageBox::critical(nullptr, tr("Error"), tr("Set access token in preferences"));
        return QString();
    }
    if (m_server.isEmpty()) {
        QMessageBox::critical(nullptr, tr("Error"), tr("Set Matrix server in preferences"));
        return QString();
    }
    return QString("https://%1/_matrix/%2/r0/%3?access_token=%4").arg(m_server).arg(type).arg(method).arg(m_access_token);
}
