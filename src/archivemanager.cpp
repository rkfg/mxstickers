#include "archivemanager.h"
#include "mainwindow.h"
#include "qzip.h"

#include <QInputDialog>
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
    auto packinfo = "name=" + pack + "\nversion=1\n";
    QByteArray b;
    for (auto& s : m_dbmanager->getStickers(pack, "", false)) {
        auto p = s.path();
        {
            auto e = zip.openEntry(QFileInfo(p).fileName());
            e.fwrite(p);
        }
        auto tags = m_dbmanager->getTags(s.code);
        packinfo += QString("%1_%2_description=%3\n%1_%2_tags=%4\n").arg(s.server).arg(s.code).arg(s.description).arg(tags.join("|"));
    }
    auto e = zip.openEntry("packinfo.txt");
    e.write(packinfo.toUtf8());
}

QString ArchiveManager::importPack(const QString& packname)
{
    QZip zip;
    zip.open(packname, ZIP_DEFAULT_COMPRESSION_LEVEL, 'r');
    auto e = zip.openEntry("packinfo.txt");
    auto lines = QString(e.read()).split('\n');
    if (lines.size() < 2) {
        throw tr("неверный формат файла packinfo.txt");
    }
    auto kv = takeLine(lines);
    if (kv.first != "name") {
        throw tr("неверный формат файла packinfo.txt");
    }
    auto pack = kv.second;
    pack = QInputDialog::getText(nullptr, "Название стикерпака", "Введите название нового стикерпака", QLineEdit::Normal, pack);
    if (pack.isEmpty()) {
        return pack;
    }
    if (m_dbmanager->packExists(pack)) {
        throw tr("стикерпак с названием %1 уже существует.").arg(pack);
    }
    MainWindow::validatePack(pack);
    if (!QDir("packs").mkpath(pack)) {
        throw tr("ошибка создания директории стикерпака %1").arg(pack);
    }
    kv = takeLine(lines);
    if (kv.first != "version" || kv.second.toInt() != 1) {
        throw tr("неверный формат файла packinfo.txt");
    }
    auto t = m_dbmanager->startTransaction();
    QMap<QString, Sticker> stickers;
    QMap<QString, QStringList> tags;
    while (!lines.isEmpty()) {
        kv = takeLine(lines);
        if (kv.first.endsWith("_description")) {
            auto server_code = serverCode(kv.first);
            stickers[server_code.second] = { 0, kv.second, server_code.first, server_code.second, pack, "" };
        }
        if (kv.first.endsWith("_tags")) {
            auto server_code = serverCode(kv.first);
            tags[server_code.second] = kv.second.split('|');
        }
    }
    bool result = true;
    bool update = false;
    bool asked = false;
    for (int i = 0; i < zip.count(); ++i) {
        auto e = zip.openEntry(i);
        auto sname = e.name();
        if (sname != "packinfo.txt") {
            QFileInfo fi(sname);
            auto code = fi.completeBaseName().split('_').last();
            auto type = fi.suffix();
            if (stickers.contains(code)) {
                e.fread(QString("packs/%1/%2").arg(pack).arg(sname));
                stickers[code].type = type;
                auto add_result = m_dbmanager->addSticker(stickers[code], update);
                if (!add_result && !update && !asked) {
                    update = QMessageBox::question(nullptr, tr("Обновлять стикеры?"), tr("В импортируемом стикерпаке обнаружен дубль стикера (в импортируемом паке '%1'). Заменять уже существующие в вашей коллекции стикеры с таким кодом? ВНИМАНИЕ: ваши описания стикеров и теги будут заменены таковыми из импортируемого стикерпака.").arg(stickers[code].description)) == QMessageBox::Yes;
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
        QMessageBox::warning(nullptr, tr("Внимание"), tr("Некоторые стикеры не были добавлены, потому что они уже присутствуют в других стикерпаках."));
    }
    return pack;
}

QPair<QString, QString> ArchiveManager::takeLine(QStringList& lines)
{
    auto s = lines.first().split('=');
    auto first = s.first();
    s.removeFirst();
    auto second = s.join('=');
    lines.removeFirst();
    return { first, second };
}

QPair<QString, QString> ArchiveManager::serverCode(const QString& key)
{
    auto server_code = key.split('_');
    if (server_code.size() != 3) {
        throw tr("ошибка записи в packinfo.txt: %1").arg(key);
    }
    auto server = server_code[0];
    auto code = server_code[1];
    return { server, code };
}
