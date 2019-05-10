#include "mainwindow.h"
#include "itemutil.h"
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
    , m_dbmanager(new DBManager(this))
{
    ui->setupUi(this);
    setGeometry(QStyle::alignedRect(Qt::LeftToRight, Qt::AlignCenter, size(), QApplication::screens().first()->availableGeometry()));
    ui->tableWidget->setHorizontalHeaderLabels({ "Изображение", "Название", "Сервер", "Код" });
    ui->tableWidget->hideColumn(2);
    ui->tableWidget->hideColumn(3);
    ui->tableWidget->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    auto action = new QAction;
    action->setShortcuts({ { "Ctrl+Shift+Return" }, { "Ctrl+Return" } });
    connect(action, &QAction::triggered, this, &MainWindow::send);
    ui->b_send->addAction(action);
    connect(ui->b_send, &QPushButton::clicked, this, &MainWindow::send);
    connect(ui->cb_stickerpack, &QComboBox::currentTextChanged, this, &MainWindow::packChanged);
    connect(ui->tableWidget, &QTableWidget::itemChanged, this, &MainWindow::stickerRenamed);
    connect(ui->b_preferences, &QPushButton::clicked, m_preferences_dialog, &QDialog::open);
    connect(m_preferences_dialog, &Preferences::settingsUpdated, this, &MainWindow::listRooms);
    connect(ui->le_filter, &QLineEdit::textChanged, this, &MainWindow::filterStickers);
    connect(ui->b_create_pack, &QPushButton::clicked, this, &MainWindow::createPack);
    connect(ui->b_remove_pack, &QPushButton::clicked, this, &MainWindow::removePack);
    connect(ui->cb_global_search, &QCheckBox::clicked, this, &MainWindow::reloadStickers);
    connect(ui->b_rename_pack, &QPushButton::clicked, this, &MainWindow::renamePack);
    ui->le_filter->setClearButtonEnabled(true);
    auto clear_action = new QAction();
    clear_action->setShortcut(QKeySequence(Qt::Key_Escape));
    connect(clear_action, &QAction::triggered, [=] {
        ui->le_filter->clear();
        ui->le_filter->setFocus();
    });
    ui->le_filter->addAction(clear_action);
    m_sticker_context_menu->addAction(QIcon(":/res/icons/list-add.png"), "Добавить стикер", this, &MainWindow::addSticker);
    m_sticker_context_menu->addAction(QIcon(":/res/icons/list-remove.png"), "Удалить стикер", this, &MainWindow::removeSticker);
    m_move_to_menu = m_sticker_context_menu->addMenu("Переместить");
    ui->tableWidget->installEventFilter(this);
    ui->le_filter->installEventFilter(this);
    init();
    listPacks();
    listRooms();
}

bool MainWindow::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == ui->tableWidget && event->type() == QEvent::ContextMenu) {
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
        if (!m_mini) {
            m_mini = ui->tableWidget->width() < 256;
        } else {
            m_mini = ui->tableWidget->width() < 300;
        }
        ui->tableWidget->setColumnHidden(1, m_mini);
        ui->l_pack->setVisible(!m_mini);
        ui->l_room->setVisible(!m_mini);
        ui->l_filter->setVisible(!m_mini);
        ui->b_create_pack->setVisible(!m_mini);
        ui->b_preferences->setVisible(!m_mini);
        ui->b_remove_pack->setVisible(!m_mini);
        ui->b_rename_pack->setVisible(!m_mini);
        ui->cb_global_search->setText(m_mini ? "" : "По всем пакам");
    }
    if (watched == ui->le_filter && event->type() == QEvent::KeyRelease) {
        auto key = static_cast<QKeyEvent*>(event)->key();
        if (key == Qt::Key_Down) {
            ui->tableWidget->setFocus();
            for (int i = 0; i < ui->tableWidget->rowCount(); ++i) {
                if (!ui->tableWidget->isRowHidden(i)) {
                    ui->tableWidget->selectRow(i);
                    break;
                }
            }
        }
        if (key == Qt::Key_Up) {
            ui->tableWidget->setFocus();
            for (int i = ui->tableWidget->rowCount() - 1; i >= 0; --i) {
                if (!ui->tableWidget->isRowHidden(i)) {
                    ui->tableWidget->selectRow(i);
                    break;
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
        QMessageBox::critical(this, "Ошибка", e.qwhat());
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
        QMessageBox::critical(this, "Ошибка", "Выберите комнату для отправки");
        return;
    }
    auto sel = ui->tableWidget->selectedItems();
    if (sel.empty()) {
        QMessageBox::critical(this, "Ошибка", "Выберите стикер для отправки");
        return;
    }
    QString event_id = QString("$%1%2:matrix.org").arg(time(NULL)).arg(rand());
    auto url = buildRequest(QString("rooms/%1/send/m.sticker/%2").arg(room.toString()).arg(event_id));
    if (url.isEmpty()) {
        return;
    }
    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    auto sticker_text = getItemText(sel[0]);
    if (QApplication::queryKeyboardModifiers().testFlag(Qt::ShiftModifier)) {
        bool ok;
        sticker_text = QInputDialog::getText(this, "Текст стикера", "Введите текст стикера", QLineEdit::Normal, sticker_text, &ok);
        if (!ok) {
            return;
        }
    }
    int w = 256;
    int h = 256;
    auto pic = static_cast<QLabel*>(ui->tableWidget->cellWidget(sel[0]->row(), 0))->pixmap();
    if (pic->width() > pic->height()) {
        h = h * pic->height() / pic->width();
    } else {
        w = w * pic->width() / pic->height();
    }
    QString mimetype = "image/png";
    auto server_code = getServerCode(sel[0]->row());
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
        QMessageBox::critical(this, "Ошибка", QString("Сетевая ошибка при отправке стикера %1").arg(reply->errorString()));
        return;
    }
    auto replyData = reply->readAll();
    QJsonObject result = QJsonDocument::fromJson(replyData).object();
    if (result.contains("event_id")) {
        ui->statusBar->showMessage("Стикер отправлен, id сообщения " + result["event_id"].toString(), 5000);
    } else {
        QMessageBox::critical(this, "Ошибка", "Неверный ответ сервера: " + replyData);
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
        qDebug() << "Content uri" << mxc;
        auto mxc_parts = mxc.mid(6).split("/");
        auto filename = reply->request().attribute(QNetworkRequest::User).toString();
        auto pack_name = reply->request().attribute(QNetworkRequest::Attribute(QNetworkRequest::User + 1)).toString();
        QFile::copy(filename, QString("packs/%1/%2_%3.png").arg(pack_name).arg(mxc_parts[0]).arg(mxc_parts[1])); // TODO: detect image type properly
        m_dbmanager->addSticker({ 0, "Новый стикер", mxc_parts[0], mxc_parts[1], pack_name, "png" }); // TODO: detect image type properly
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
        if (ui->cb_stickerpack->currentIndex() == 0) {
            pack = "";
        }
        auto stickers = m_dbmanager->getStickers(pack, ui->le_filter->text(), ui->cb_global_search->isChecked());
        for (auto& s : stickers) {
            insertRow(s);
        }
    } catch (const DBException& e) {
        qWarning() << "Не удалось загрузить стикеры: " << e.what();
    }
    ui->tableWidget->resizeRowsToContents();
}

void MainWindow::stickerRenamed(QTableWidgetItem* item)
{
    auto text = getItemText(item);
    m_dbmanager->renameSticker(getCode(item->row()), text);
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
                    m_dbmanager->addSticker({ 0, desc, s[0], s[1], p.completeBaseName(), "png" });
                } catch (const DBException& e) {
                    qWarning() << e.qwhat();
                }
            }
        }
    }
}

void MainWindow::moveStickerToPack(const QString& pack)
{
    auto sel = ui->tableWidget->selectedItems();
    if (sel.isEmpty()) {
        QMessageBox::critical(this, "Ошибка", "Выберите стикер для перемещения.");
        return;
    }
    int row = sel.first()->row();
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
    ui->tableWidget->blockSignals(false);
}

void MainWindow::listPacks()
{
    int idx = ui->cb_stickerpack->currentIndex();
    ui->cb_stickerpack->clear();
    ui->cb_stickerpack->addItem("<Недавние>");
    ui->cb_stickerpack->insertSeparator(1);
    for (auto& d : QDir("packs").entryList(QDir::NoDotAndDotDot | QDir::Dirs)) {
        ui->cb_stickerpack->addItem(d);
    }
    if (idx > 0) {
        ui->cb_stickerpack->setCurrentIndex(idx);
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
    auto stickers = QFileDialog::getOpenFileNames(this, "Выберите стикеры для добавления", QString(), "PNG (*.png)");
    if (stickers.isEmpty()) {
        return;
    }
    for (auto& f : stickers) {
        auto url = buildRequest("upload", "media");
        QNetworkRequest req(url);
        req.setHeader(QNetworkRequest::ContentTypeHeader, "image/png");
        req.setAttribute(QNetworkRequest::User, f);
        req.setAttribute(QNetworkRequest::Attribute(QNetworkRequest::User + 1), ui->cb_stickerpack->currentText());
        auto file = new QFile(f);
        file->open(QFile::ReadOnly);
        connect(m_network->post(req, file), &QNetworkReply::finished, this, &MainWindow::uploadFinished);
    }
}

void MainWindow::removeSticker()
{
    auto sel = ui->tableWidget->selectedItems();
    if (sel.empty()) {
        QMessageBox::critical(this, "Ошибка", "Выберите стикер для удаления");
        return;
    }
    if (QMessageBox::question(this, "Подтверждение", "Удалить этот стикер?") != QMessageBox::Yes) {
        return;
    }
    int row = sel.first()->row();
    QFile::remove(getStickerPath(row));
    m_dbmanager->removeSticker(getCode(row));
    reloadStickers();
}

void MainWindow::validatePack(const QString& packname)
{
    if (packname.contains('/') || packname.contains('\\') || packname == "." || packname == ".." || packname.startsWith("<")) {
        throw "Неверное имя стикерпака.";
    }
}

QString MainWindow::buildRequest(const QString& method, const QString& type)
{
    auto access_token = m_settings->value("matrix/access_token").toString();
    if (access_token.isEmpty()) {
        QMessageBox::critical(this, "Ошибка", "Задайте токен доступа в настройках");
        return QString();
    }
    auto server = m_settings->value("matrix/server").toString();
    if (server.isEmpty()) {
        QMessageBox::critical(this, "Ошибка", "Задайте сервер Matrix в настройках");
        return QString();
    }
    return QString("https://%1/_matrix/%2/r0/%3?access_token=%4").arg(server).arg(type).arg(method).arg(access_token);
}

void MainWindow::createPack()
{
    auto newpack = QInputDialog::getText(this, "Новый стикерпак", "Введите название нового стикерпака");
    if (newpack.isEmpty()) {
        return;
    }
    try {
        validatePack(newpack);
    } catch (const char* e) {
        QMessageBox::critical(this, "Ошибка", e);
        return;
    }
    if (!QDir("packs").mkpath(newpack)) {
        QMessageBox::critical(this, "Ошибка", "Не удалось создать стикерпак.");
        return;
    }
    listPacks();
    ui->cb_stickerpack->setCurrentText(newpack);
}

void MainWindow::removePack()
{
    auto pack = ui->cb_stickerpack->currentText();
    if (pack.isEmpty()) {
        QMessageBox::critical(this, "Ошибка", "Выберите стикерпак для удаления");
    }
    if (QMessageBox::question(this, "Удаление стикерпака", "ВНИМАНИЕ! Все стикеры в этом стикерпаке будут также удалены! Удалить этот стикерпак?") != QMessageBox::Yes) {
        return;
    }
    if (!QDir("packs/" + pack).removeRecursively()) {
        QMessageBox::critical(this, "Ошибка", "Не удалось удалить стикерпак.");
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

void MainWindow::renamePack()
{
    auto oldname = ui->cb_stickerpack->currentText();
    auto newname = QInputDialog::getText(this, "Переименование стикерпака", "Введите новое название стикерпака", QLineEdit::Normal, oldname);
    if (newname.isEmpty()) {
        return;
    }
    try {
        validatePack(newname);
    } catch (const char* e) {
        QMessageBox::critical(this, "Ошибка", e);
        return;
    }
    if (QDir("packs").rename(oldname, newname)) {
        m_dbmanager->renamePack(oldname, newname);
    } else {
        QMessageBox::critical(this, "Ошибка", "Ошибка переименования стикерпака. Попробуйте другое название.");
    }
    listPacks();
}

QString MainWindow::getStickerPath(int row, const QString& pack)
{
    QString curpack = pack;
    if (curpack.isEmpty()) {
        curpack = ui->cb_stickerpack->currentText();
    }
    return QString("packs/%1/%2_%3.png").arg(curpack).arg(getServer(row)).arg(getCode(row));
}
