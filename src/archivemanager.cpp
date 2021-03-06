#include "archivemanager.h"
#include "mainwindow.h"
#include "qzip.h"

#include <QInputDialog>
#include <QJsonDocument>
#include <QMessageBox>

ArchiveManager::ArchiveManager(DBManager* db_manager, QObject* parent)
    : QObject(parent)
    , m_dbmanager(db_manager)
{
}

void ArchiveManager::exportPack(const QString& pack, const QString& packname)
{
    QZip zip;
    zip.open(packname, ZIP_DEFAULT_COMPRESSION_LEVEL, 'w');
    QJsonObject root;
    root["name"] = pack;
    root["version"] = 2;
    QJsonArray stickers;
    for (auto& s : m_dbmanager->getStickers(pack, "", false)) {
        auto p = s.path();
        {
            auto e = zip.openEntry(QFileInfo(p).fileName());
            e.fwrite(p);
        }
        auto tags = m_dbmanager->getTags(s.code);
        QJsonObject sticker;
        sticker["description"] = s.description;
        sticker["code"] = s.code;
        sticker["server"] = s.server;
        sticker["tags"] = QJsonArray::fromStringList(tags);
        stickers.append(sticker);
    }
    auto e = zip.openEntry("packinfo.json");
    root["stickers"] = stickers;
    QJsonDocument packinfo;
    packinfo.setObject(root);
    e.write(packinfo.toJson());
}

QString ArchiveManager::importPack(const QString& packname)
{
    QZip zip;
    zip.open(packname, ZIP_DEFAULT_COMPRESSION_LEVEL, 'r');
    auto e = zip.openEntry("packinfo.json");
    auto packinfo = QJsonDocument::fromJson(e.read());
    if (!packinfo.isObject()) {
        throw tr("invalid packinfo.json file format");
    }
    if (packinfo["version"].toInt() != 2) {
        throw tr("invalid packinfo.json file format");
    }
    auto pack = packinfo["name"].toString();
    qDebug() << tr("Import pack '%1'").arg(pack);
    pack = QInputDialog::getText(nullptr, tr("Stickerpack name"), tr("Enter new stickerpack name"), QLineEdit::Normal, pack);
    if (pack.isEmpty()) {
        return pack;
    }
    if (m_dbmanager->packExists(pack)) {
        throw tr("stickerpack named '%1' already exists.").arg(pack);
    }
    MainWindow::validatePack(pack);
    if (!QDir("packs").mkpath(pack)) {
        throw tr("error creating stickerpack directory '%1'").arg(pack);
    }
    auto jstickers = packinfo["stickers"].toArray();
    if (jstickers.isEmpty()) {
        throw tr("stickers not found in stickerpack '%1'").arg(pack);
    }
    auto t = m_dbmanager->startTransaction();
    QMap<QString, Sticker> stickers;
    QMap<QString, QStringList> tags;
    for (int i = 0; i < jstickers.size(); ++i) {
        auto sticker = jstickers[i].toObject();
        auto description = sticker["description"].toString();
        auto server = sticker["server"].toString();
        auto code = sticker["code"].toString();
        stickers[code] = { 0, description, server, code, pack, "" };
        QStringList taglist;
        for (auto tag : sticker["tags"].toArray()) {
            taglist << tag.toString();
        }
        tags[code] = taglist;
    }
    bool result = true;
    bool update = false;
    bool asked = false;
    for (int i = 0; i < zip.count(); ++i) {
        auto e = zip.openEntry(i);
        auto sname = e.name();
        if (sname != "packinfo.json") {
            QFileInfo fi(sname);
            auto code = fi.completeBaseName().split('_').last();
            auto type = fi.suffix();
            if (stickers.contains(code)) {
                e.fread(QString("packs/%1/%2").arg(pack).arg(sname));
                stickers[code].type = type;
                auto add_result = m_dbmanager->addSticker(stickers[code], update);
                if (!add_result && !update && !asked) {
                    update = QMessageBox::question(nullptr, tr("Update stickers?"), tr("Found a duplicate sticker in the stickerpack '%1' that's being imported. Replace all existing stickers with the imported ones? WARNING: your stickers descriptions and tags will be replaced with those from the stickerpack you're importing.").arg(stickers[code].description)) == QMessageBox::Yes;
                    if (update) {
                        add_result = m_dbmanager->addSticker(stickers[code], update);
                    }
                    asked = true;
                }
                result &= add_result;
                m_dbmanager->setTags(code, tags[code]);
            }
        }
    }
    if (!result) {
        QMessageBox::warning(nullptr, tr("Warning"), tr("Some stickers were skipped because they already present in other stickerpacks."));
    }
    return pack;
}
