#include "mainwindow.h"
#include "itemutil.h"
#include "previewfiledialog.h"
#include "ui_mainwindow.h"
#include <QAction>
#include <QDesktopWidget>
#include <QDirIterator>
#include <QFileDialog>
#include <QInputDialog>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeyEvent>
#include <QLabel>
#include <QMessageBox>
#include <QNetworkReply>
#include <QScreen>
#include <time.h>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_network(new QNetworkAccessManager(this))
    , m_settings(new QSettings("rkfg", "mxstickers", this))
    , m_preferences_dialog(new Preferences(m_settings, this))
    , m_sticker_context_menu(new QMenu(this))
    , m_pack_menu(new QMenu(this))
    , m_dbmanager(new DBManager(this))
    , m_tag_editor(new TagEditor(this))
    , m_archive_manager(new ArchiveManager(m_dbmanager, this))
    , m_matrix(new MatrixAPI(*m_network, this))
{
    auto pack = m_settings->value("current_pack", "").toString();
    ui->setupUi(this);
    setGeometry(QStyle::alignedRect(Qt::LeftToRight, Qt::AlignCenter, size(), QApplication::screens().first()->availableGeometry()));
    ui->tableWidget->setHorizontalHeaderLabels({ tr("Image"), tr("Name"), tr("Server"), tr("Code"), tr("Type") });
    ui->tableWidget->hideColumn(2);
    ui->tableWidget->hideColumn(3);
    ui->tableWidget->hideColumn(4);
    ui->tableWidget->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    auto action = new QAction;
    action->setShortcuts({ { "Ctrl+Shift+Return" }, { "Ctrl+Return" } });
    connect(action, &QAction::triggered, this, &MainWindow::send);
    ui->b_send->addAction(action);
    connect(ui->b_send, &QPushButton::clicked, this, &MainWindow::send);
    connect(ui->cb_stickerpack, &QComboBox::currentTextChanged, this, &MainWindow::packChanged);
    connect(ui->tableWidget, &QTableWidget::itemChanged, this, &MainWindow::stickerRenamed);
    connect(ui->le_filter, &QLineEdit::textChanged, this, &MainWindow::filterStickers);
    connect(ui->cb_global_search, &QCheckBox::clicked, this, &MainWindow::reloadStickers);
    connect(ui->cb_rooms, qOverload<int>(&QComboBox::currentIndexChanged), [=](int idx) {
        m_settings->setValue("current_room", ui->cb_rooms->currentData().toString());
    });
    m_pack_menu->addAction(QIcon(":/res/icons/list-add.png"), tr("Create stickerpack"), this, &MainWindow::createPack);
    m_pack_menu->addAction(QIcon(":/res/icons/list-remove.png"), tr("Remove stickerpack"), this, &MainWindow::removePack);
    m_pack_menu->addAction(QIcon(":/res/icons/document-edit.png"), tr("Rename stickerpack"), this, &MainWindow::renamePack);
    m_pack_menu->addAction(QIcon(":/res/icons/document-import.png"), tr("Import stickerpack"), this, &MainWindow::importPack);
    m_pack_menu->addAction(QIcon(":/res/icons/document-export.png"), tr("Export stickerpack"), this, &MainWindow::exportPack);
    m_pack_menu->addAction(QIcon(":/res/icons/applications-system.png"), tr("Preferences"), m_preferences_dialog, &QDialog::open);
    connect(ui->b_menu, &QPushButton::clicked, [=] {
        m_pack_menu->exec(mapToGlobal(ui->b_menu->pos() + ui->b_menu->rect().bottomLeft()));
    });
    ui->le_filter->setClearButtonEnabled(true);
    auto clear_action = new QAction();
    clear_action->setShortcut(QKeySequence(Qt::Key_Escape));
    connect(clear_action, &QAction::triggered, [=] {
        ui->le_filter->clear();
        ui->le_filter->setFocus();
    });
    ui->le_filter->addAction(clear_action);
    m_sticker_context_menu->addAction(QIcon(":/res/icons/list-add.png"), tr("Add sticker"), this, &MainWindow::addSticker);
    m_sticker_context_menu->addAction(QIcon(":/res/icons/list-remove.png"), tr("Remove sticker"), this, &MainWindow::removeSticker);
    auto edit_tag_action = new QAction(QIcon(":/res/icons/document-edit.png"), tr("Tags"));
    edit_tag_action->setShortcut(QKeySequence("Shift+Return"));
#if QT_VERSION >= QT_VERSION_CHECK(5, 10, 0)
    edit_tag_action->setShortcutVisibleInContextMenu(true);
#endif
    connect(edit_tag_action, &QAction::triggered, this, &MainWindow::editTags);
    m_sticker_context_menu->addAction(edit_tag_action);
    ui->tableWidget->addAction(edit_tag_action);
    m_move_to_menu = m_sticker_context_menu->addMenu(tr("Move"));
    ui->tableWidget->installEventFilter(this);
    ui->le_filter->installEventFilter(this);
    ui->le_filter->setFocus();
    init();
    listPacks();
    if (!pack.isEmpty()) {
        ui->cb_stickerpack->setCurrentText(pack);
    }
    updateMatrixParams();
    connect(m_preferences_dialog, &Preferences::settingsUpdated, this, &MainWindow::updateMatrixParams);
    connect(m_matrix, &MatrixAPI::syncComplete, this, &MainWindow::sync);
    m_matrix->m_since = m_settings->value("matrix/since").toString();
    m_matrix->sync();
}

bool MainWindow::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == ui->tableWidget && event->type() == QEvent::ContextMenu) {
        if (ui->cb_stickerpack->currentIndex() == 0) {
            return true;
        }
        m_move_to_menu->clear();
        for (int i = 2; i < ui->cb_stickerpack->count(); ++i) {
            if (ui->cb_stickerpack->currentIndex() != i) {
                m_move_to_menu->addAction(ui->cb_stickerpack->itemText(i), std::bind(&MainWindow::moveStickerToPack, this, ui->cb_stickerpack->itemText(i)));
            }
        }
        m_sticker_context_menu->exec(static_cast<QContextMenuEvent*>(event)->globalPos());
        return true;
    }
    if (watched == ui->tableWidget && event->type() == QEvent::Resize) {
        bool mini = m_mini;
        if (!m_mini) {
            m_mini = ui->tableWidget->width() < 256;
        } else {
            m_mini = ui->tableWidget->width() < 300;
        }
        ui->tableWidget->setColumnHidden(1, m_mini);
        ui->l_pack->setVisible(!m_mini);
        ui->l_room->setVisible(!m_mini);
        ui->l_filter->setVisible(!m_mini);
        ui->b_menu->setVisible(!m_mini);
        ui->cb_global_search->setText(m_mini ? "" : tr("All packs"));
        if (mini ^ m_mini) {
            ui->gridLayout->addWidget(ui->cb_stickerpack, 0, m_mini ? 0 : 1, 1, m_mini ? 4 : 2);
            ui->gridLayout->addWidget(ui->le_filter, 1, m_mini ? 0 : 1, 1, m_mini ? 3 : 1);
            ui->gridLayout->addWidget(ui->cb_global_search, 1, m_mini ? 3 : 2, 1, m_mini ? 1 : 2);
            ui->gridLayout->addWidget(ui->cb_rooms, 3, m_mini ? 0 : 1, 1, m_mini ? 3 : 2);
        }
        return false;
    }
    if (watched == ui->le_filter && event->type() == QEvent::KeyRelease) {
        auto key = static_cast<QKeyEvent*>(event)->key();
        if (key == Qt::Key_Down) {
            ui->tableWidget->setFocus();
            for (int i = 0; i < ui->tableWidget->rowCount(); ++i) {
                if (!ui->tableWidget->isRowHidden(i)) {
                    ui->tableWidget->selectRow(i);
                    return true;
                }
            }
        }
        if (key == Qt::Key_Up) {
            ui->tableWidget->setFocus();
            for (int i = ui->tableWidget->rowCount() - 1; i >= 0; --i) {
                if (!ui->tableWidget->isRowHidden(i)) {
                    ui->tableWidget->selectRow(i);
                    return true;
                }
            }
        }
    }
    return false;
}

bool MainWindow::init()
{
    try {
        m_dbmanager->init();
    } catch (const DBException& e) {
        QMessageBox::critical(this, tr("Error"), e.qwhat());
        return false;
    }
    rescanPacks();
    return true;
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::send()
{
    auto room = ui->cb_rooms->currentData();
    if (room.isNull()) {
        QMessageBox::critical(this, tr("Error"), tr("Choose room to send sticker to"));
        return;
    }
    auto sel = ui->tableWidget->selectedRanges();
    if (sel.empty()) {
        QMessageBox::critical(this, tr("Error"), tr("Choose sticker to send"));
        return;
    }
    int row = sel[0].topRow();
    QString event_id = QString("$%1%2:matrix.org").arg(time(NULL)).arg(rand());
    auto url = buildRequest(QString("rooms/%1/send/m.sticker/%2").arg(room.toString()).arg(event_id));
    if (url.isEmpty()) {
        return;
    }
    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    auto sticker_text = getDescription(row);
    if (QApplication::queryKeyboardModifiers().testFlag(Qt::ShiftModifier)) {
        bool ok;
        sticker_text = QInputDialog::getText(this, tr("Sticker text"), tr("Enter sticker text"), QLineEdit::Normal, sticker_text, &ok);
        if (!ok) {
            return;
        }
    }
    if (sticker_text.isEmpty()) {
        sticker_text = tr("Sticker");
    }
    int w = 160;
    int h = 160;
    auto pic = static_cast<QLabel*>(ui->tableWidget->cellWidget(row, 0))->pixmap();
    if (pic->width() > pic->height()) {
        h = h * pic->height() / pic->width();
    } else {
        w = w * pic->width() / pic->height();
    }
    QString mimetype = "image/" + getType(row);
    auto server_code = getServerCode(row);
    auto sticker_url = QString("mxc://%1/%2").arg(server_code[0]).arg(server_code[1]);
    QJsonObject info { { "mimetype", mimetype }, { "w", w }, { "h", h }, { "size", 1 } };
    QJsonObject content({ { "body", sticker_text }, { "url", sticker_url }, { "info", info } });
    auto encodedJson = QJsonDocument(content).toJson(QJsonDocument::Compact);
    connect(m_network->put(req, encodedJson), &QNetworkReply::finished, this, &MainWindow::sendFinished);
    m_dbmanager->updateRecentSticker(server_code[1]);
}

void MainWindow::sendFinished()
{
    auto reply = qobject_cast<QNetworkReply*>(QObject::sender());
    if (reply->error() != QNetworkReply::NoError) {
        QMessageBox::critical(this, tr("Error"), tr("Network error while sending sticker %1").arg(reply->errorString()));
        return;
    }
    auto replyData = reply->readAll();
    QJsonObject result = QJsonDocument::fromJson(replyData).object();
    if (result.contains("event_id")) {
        ui->statusBar->showMessage(tr("Sticker sent, event id is %1").arg(result["event_id"].toString()), 5000);
    } else {
        QMessageBox::critical(this, tr("Error"), tr("Invalid server response: %1").arg(QString::fromUtf8(replyData)));
    }
    reply->deleteLater();
}

void MainWindow::uploadFinished()
{
    auto reply = qobject_cast<QNetworkReply*>(QObject::sender());
    if (reply->error() != QNetworkReply::NoError) {
        QMessageBox::critical(this, "Ошибка", QString("Сетевая ошибка при загрузке стикера %1").arg(reply->errorString()));
        return;
    }
    QJsonObject result = QJsonDocument::fromJson(reply->readAll()).object();
    if (result.contains("content_uri")) {
        auto mxc = result["content_uri"].toString();
        qDebug() << tr("Content uri %1").arg(mxc);
        auto mxc_parts = mxc.mid(6).split("/");
        auto filename = reply->request().attribute(QNetworkRequest::User).toString();
        auto type = typeFromFilename(filename);
        auto pack_name = reply->request().attribute(QNetworkRequest::Attribute(QNetworkRequest::User + 1)).toString();
        QFile::copy(filename, QString("packs/%1/%2_%3.%4").arg(pack_name).arg(mxc_parts[0]).arg(mxc_parts[1]).arg(type));
        m_dbmanager->addSticker({ 0, tr("New sticker"), mxc_parts[0], mxc_parts[1], pack_name, type });
        if (ui->cb_stickerpack->currentText() == pack_name) {
            emit ui->cb_stickerpack->currentTextChanged(pack_name);
        }
    }
}

void MainWindow::reloadStickers()
{
    packChanged(ui->cb_stickerpack->currentText());
}

void MainWindow::packChanged(const QString& text)
{
    ui->tableWidget->setRowCount(0);
    try {
        auto pack = text;
        m_settings->setValue("current_pack", pack);
        if (ui->cb_stickerpack->currentIndex() == 0) {
            pack = "";
        }
        m_pack_menu->actions()[1]->setEnabled(ui->cb_stickerpack->currentIndex() != 0);
        m_pack_menu->actions()[2]->setEnabled(ui->cb_stickerpack->currentIndex() != 0);
        m_pack_menu->actions()[4]->setEnabled(ui->cb_stickerpack->currentIndex() != 0);
        auto stickers = m_dbmanager->getStickers(pack, ui->le_filter->text(), ui->cb_global_search->isChecked());
        for (auto& s : stickers) {
            insertRow(s);
        }
    } catch (const DBException& e) {
        qWarning() << tr("Can't load stickers: %1").arg(e.what());
    }
    ui->tableWidget->resizeRowsToContents();
}

void MainWindow::stickerRenamed(QTableWidgetItem* item)
{
    auto text = getItemText(item);
    try {
        m_dbmanager->renameSticker(getCode(item->row()), text);
    } catch (const DBException& e) {
        QMessageBox::critical(this, tr("Error"), e.qwhat());
        return;
    }
    qobject_cast<QLabel*>(ui->tableWidget->cellWidget(item->row(), 0))->setToolTip(text);
}

void MainWindow::rescanPacks()
{
    for (auto& p : QDir("packs").entryInfoList(QDir::NoDotAndDotDot | QDir::Dirs)) {
        for (auto& d : QDir(p.filePath()).entryInfoList(QDir::NoDotAndDotDot | QDir::Files)) {
            auto mxc = d.completeBaseName();
            auto s = mxc.split('_');
            if (s.length() == 2) {
                auto desc = m_settings->value("names/" + s[1]).toString();
                try {
                    m_dbmanager->addSticker({ 0, desc, s[0], s[1], p.completeBaseName(), typeFromFilename(mxc) });
                } catch (const DBException& e) {
                    qWarning() << e.qwhat();
                }
            }
        }
    }
}

void MainWindow::moveStickerToPack(const QString& pack)
{
    auto sel = ui->tableWidget->selectedRanges();
    if (sel.isEmpty()) {
        QMessageBox::critical(this, tr("Error"), tr("Choose sticker to move."));
        return;
    }
    int row = sel.first().topRow();
    m_dbmanager->moveStickerToPack(getCode(row), pack);
    QFile(getStickerPath(row)).rename(getStickerPath(row, pack));
    reloadStickers();
}

void MainWindow::insertRow(const Sticker& s)
{
    ui->tableWidget->blockSignals(true); // don't want to trigger editing signal
    int idx = ui->tableWidget->rowCount();
    ui->tableWidget->insertRow(idx);
    auto icon = new QLabel();
    icon->setPixmap(QPixmap(s.path()).scaled(128, 128, Qt::AspectRatioMode::KeepAspectRatio, Qt::TransformationMode::SmoothTransformation));
    icon->setAlignment(Qt::AlignCenter);
    icon->setToolTip(s.description);
    ui->tableWidget->setCellWidget(idx, 0, icon);
    ui->tableWidget->setItem(idx, 1, new QTableWidgetItem(s.description));
    auto mxc_server = new QTableWidgetItem(s.server);
    mxc_server->setFlags(mxc_server->flags() & ~Qt::ItemIsEditable);
    ui->tableWidget->setItem(idx, 2, mxc_server);
    auto mxc_code = new QTableWidgetItem(s.code);
    mxc_code->setFlags(mxc_code->flags() & ~Qt::ItemIsEditable);
    ui->tableWidget->setItem(idx, 3, mxc_code);
    ui->tableWidget->setItem(idx, 4, new QTableWidgetItem(s.type));
    ui->tableWidget->blockSignals(false);
}

void MainWindow::listPacks(QString newtext)
{
    if (newtext.isEmpty()) {
        newtext = ui->cb_stickerpack->currentText();
    }
    ui->cb_stickerpack->clear();
    ui->cb_stickerpack->addItem(tr("<Recent>"));
    ui->cb_stickerpack->insertSeparator(1);
    for (auto& d : QDir("packs").entryList(QDir::NoDotAndDotDot | QDir::Dirs)) {
        ui->cb_stickerpack->addItem(d);
    }
    if (!newtext.isEmpty()) {
        ui->cb_stickerpack->setCurrentText(newtext);
    }
}

void MainWindow::listRooms()
{
    ui->cb_rooms->clear();
    for (auto& r : m_preferences_dialog->loadRooms()) {
        ui->cb_rooms->addItem(r.name, r.address);
    }
}

void MainWindow::filterStickers()
{
    packChanged(ui->cb_stickerpack->currentText());
}

void MainWindow::addSticker()
{
    PreviewFileDialog dlg(this, tr("Choose stickers to add"), QString(), "Images (*.png *.jpg *.gif *.webp)", 256);
    dlg.setFileMode(QFileDialog::ExistingFiles);
    if (dlg.exec() != QDialog::Accepted) {
        return;
    }
    auto stickers = dlg.selectedFiles();
    if (stickers.isEmpty()) {
        return;
    }
    for (auto& f : stickers) {
        auto url = buildRequest("upload", "media");
        QNetworkRequest req(url);
        auto type = typeFromFilename(f);
        req.setHeader(QNetworkRequest::ContentTypeHeader, "image/" + type);
        req.setAttribute(QNetworkRequest::User, f);
        req.setAttribute(QNetworkRequest::Attribute(QNetworkRequest::User + 1), ui->cb_stickerpack->currentText());
        auto file = new QFile(f);
        file->open(QFile::ReadOnly);
        connect(m_network->post(req, file), &QNetworkReply::finished, this, &MainWindow::uploadFinished);
    }
}

void MainWindow::removeSticker()
{
    auto sel = ui->tableWidget->selectedRanges();
    if (sel.empty()) {
        QMessageBox::critical(this, tr("Error"), tr("Choose sticker to delete"));
        return;
    }
    if (QMessageBox::question(this, tr("Confirm"), tr("Remove this sticker?")) != QMessageBox::Yes) {
        return;
    }
    int row = sel.first().topRow();
    QFile::remove(getStickerPath(row));
    try {
        m_dbmanager->removeSticker(getCode(row));
    } catch (const DBException& e) {
        QMessageBox::critical(this, tr("Error"), e.qwhat());
        return;
    }
    reloadStickers();
}

void MainWindow::validatePack(const QString& packname)
{
    if (packname.contains('/') || packname.contains('\\') || packname == "." || packname == ".." || packname.startsWith("<")) {
        throw tr("Invalid stickerpack name.");
    }
}

QString MainWindow::buildRequest(const QString& method, const QString& type)
{
    auto access_token = m_settings->value("matrix/access_token").toString();
    if (access_token.isEmpty()) {
        QMessageBox::critical(this, tr("Error"), tr("Set access token in preferences"));
        return QString();
    }
    auto server = m_settings->value("matrix/server").toString();
    if (server.isEmpty()) {
        QMessageBox::critical(this, tr("Error"), tr("Set Matrix server in preferences"));
        return QString();
    }
    return QString("https://%1/_matrix/%2/r0/%3?access_token=%4").arg(server).arg(type).arg(method).arg(access_token);
}

void MainWindow::createPack()
{
    auto newpack = QInputDialog::getText(this, tr("New stickerpack"), tr("Enter new stickerpack name"));
    if (newpack.isEmpty()) {
        return;
    }
    try {
        validatePack(newpack);
    } catch (const QString& e) {
        QMessageBox::critical(this, tr("Error"), e);
        return;
    }
    if (!QDir("packs").mkpath(newpack)) {
        QMessageBox::critical(this, tr("Error"), tr("Can't create stickerpack."));
        return;
    }
    listPacks(newpack);
}

void MainWindow::removePack()
{
    auto pack = ui->cb_stickerpack->currentText();
    if (pack.isEmpty()) {
        QMessageBox::critical(this, tr("Error"), tr("Choose stickerpack to delete"));
    }
    if (QMessageBox::question(this, tr("Stickerpack removal"), tr("WARNING! All stickers in this stickerpack will be removed as well! Remove this stickerpack?")) != QMessageBox::Yes) {
        return;
    }
    try {
        m_dbmanager->removePack(pack);
    } catch (const DBException& e) {
        QMessageBox::critical(this, tr("Error"), e.qwhat());
        return;
    }
    if (!QDir("packs/" + pack).removeRecursively()) {
        QMessageBox::critical(this, tr("Error"), tr("Can't remove stickerpack."));
        return;
    }
    listPacks();
}

QStringList MainWindow::getServerCode(int row)
{
    return { getServer(row), getCode(row) };
}

QString MainWindow::getServer(int row)
{
    return getItemText(ui->tableWidget->item(row, 2));
}

QString MainWindow::getCode(int row)
{
    return getItemText(ui->tableWidget->item(row, 3));
}

QString MainWindow::getDescription(int row)
{
    return getItemText(ui->tableWidget->item(row, 1));
}

QString MainWindow::getType(int row)
{
    return getItemText(ui->tableWidget->item(row, 4));
}

void MainWindow::renamePack()
{
    auto oldname = ui->cb_stickerpack->currentText();
    auto newname = QInputDialog::getText(this, tr("Rename stickerpack"), tr("Enter new name for this stickerpack"), QLineEdit::Normal, oldname);
    if (newname.isEmpty()) {
        return;
    }
    try {
        validatePack(newname);
    } catch (const QString& e) {
        QMessageBox::critical(this, tr("Error"), e);
        return;
    }
    if (QDir("packs").rename(oldname, newname)) {
        m_dbmanager->renamePack(oldname, newname);
    } else {
        QMessageBox::critical(this, tr("Error"), tr("Error renaming stickerpack. Try another name."));
    }
    listPacks(newname);
}

QString MainWindow::getStickerPath(int row, const QString& pack)
{
    QString curpack = pack;
    if (curpack.isEmpty()) {
        curpack = ui->cb_stickerpack->currentText();
    }
    return QString("packs/%1/%2_%3.%4").arg(curpack).arg(getServer(row)).arg(getCode(row)).arg(getType(row));
}

QString MainWindow::typeFromFilename(const QString& filename) const
{
    return QFileInfo(filename).suffix().toLower();
}

void MainWindow::editTags()
{
    auto sel = ui->tableWidget->selectedRanges();
    if (sel.isEmpty()) {
        QMessageBox::critical(this, tr("Error"), tr("Choose sticker to edit tags."));
        return;
    }
    int row = sel.first().topRow();
    auto code = getCode(row);
    m_tag_editor->setAvailableTags(m_dbmanager->getTags(""));
    m_tag_editor->setTags(m_dbmanager->getTags(code));
    if (m_tag_editor->exec() != QDialog::Accepted) {
        return;
    }
    m_dbmanager->setTags(code, m_tag_editor->getTags());
}

void MainWindow::exportPack()
{
    auto pack = ui->cb_stickerpack->currentText();
    auto filename = QFileDialog::getSaveFileName(this, tr("Export stickerpack"), pack + ".zip", "Stickerpack (*.zip)");
    if (filename.isEmpty()) {
        return;
    }
    if (!filename.toLower().endsWith(".zip")) {
        filename += ".zip";
    }
    try {
        m_archive_manager->exportPack(pack, filename);
        ui->statusBar->showMessage(tr("Stickerpack '%1' has been exported successfully.").arg(pack), 5000);
    } catch (const QString& e) {
        QMessageBox::critical(this, tr("Error"), tr("Can't export stickerpack: %1").arg(e));
    }
}

void MainWindow::importPack()
{
    auto filename = QFileDialog::getOpenFileName(this, tr("Import stickerpack"), QString(), "Stickerpack (*.zip)");
    if (filename.isEmpty()) {
        return;
    }
    try {
        auto pack = m_archive_manager->importPack(filename);
        if (!pack.isEmpty()) {
            ui->statusBar->showMessage(tr("Stickerpack '%1' has been imported successfully.").arg(pack), 5000);
            listPacks(pack);
        }
    } catch (const QString& e) {
        QMessageBox::critical(this, tr("Error"), tr("Can't import stickerpack: %1").arg(e));
    }
}

void MainWindow::sync(MXSync sync)
{
    ui->cb_rooms->clear();
    auto room = m_settings->value("current_room", "").toString();
    for (auto& r : sync.rooms) {
        ui->cb_rooms->addItem(r.title(), r.address);
        if (room == r.address) {
            ui->cb_rooms->setCurrentIndex(ui->cb_rooms->count() - 1);
        }
    }
}

void MainWindow::updateMatrixParams()
{
    m_matrix->m_server = m_settings->value("matrix/server").toString();
    m_matrix->m_access_token = m_settings->value("matrix/access_token").toString();
}
