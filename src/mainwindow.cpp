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
#include <time.h>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_network(new QNetworkAccessManager(this))
    , m_settings(new QSettings("rkfg", "mxstickers", this))
    , m_preferences_dialog(new Preferences(m_settings, this))
    , m_sticker_context_menu(new QMenu(this))
{
    ui->setupUi(this);
    setGeometry(QStyle::alignedRect(Qt::LeftToRight, Qt::AlignCenter, size(), QApplication::desktop()->availableGeometry()));
    ui->tableWidget->setHorizontalHeaderLabels({ "Изображение", "Название", "Сервер", "Код" });
    ui->tableWidget->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    connect(ui->b_send, &QPushButton::clicked, this, &MainWindow::send);
    connect(ui->cb_stickerpack, &QComboBox::currentTextChanged, this, &MainWindow::packChanged);
    connect(ui->tableWidget, &QTableWidget::itemChanged, this, &MainWindow::stickerRenamed);
    connect(ui->b_preferences, &QPushButton::clicked, m_preferences_dialog, &QDialog::open);
    connect(m_preferences_dialog, &Preferences::settingsUpdated, this, &MainWindow::listRooms);
    connect(ui->le_filter, &QLineEdit::textChanged, this, &MainWindow::filterStickers);
    connect(ui->b_create_pack, &QPushButton::clicked, this, &MainWindow::createPack);
    connect(ui->b_remove_pack, &QPushButton::clicked, this, &MainWindow::removePack);
    ui->le_filter->setClearButtonEnabled(true);
    auto clear_action = new QAction();
    clear_action->setShortcut(QKeySequence(Qt::Key_Escape));
    connect(clear_action, &QAction::triggered, [=] {
        ui->le_filter->clear();
        ui->le_filter->setFocus();
    });
    ui->le_filter->addAction(clear_action);
    listPacks();
    listRooms();
    m_sticker_context_menu->addAction(QIcon(":/res/icons/list-add.png"), "Добавить стикер", this, &MainWindow::addSticker);
    m_sticker_context_menu->addAction(QIcon(":/res/icons/list-remove.png"), "Удалить стикер", this, &MainWindow::removeSticker);
    ui->tableWidget->installEventFilter(this);
    ui->le_filter->installEventFilter(this);
}

bool MainWindow::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == ui->tableWidget && event->type() == QEvent::ContextMenu) {
        m_sticker_context_menu->exec(static_cast<QContextMenuEvent*>(event)->globalPos());
        return true;
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
    sticker_text = QInputDialog::getText(this, "Текст стикера", "Введите текст стикера", QLineEdit::Normal, sticker_text);
    if (sticker_text.isNull()) {
        return;
    }
    int w = 256;
    int h = 256;
    auto pic = static_cast<QLabel*>(ui->tableWidget->cellWidget(sel[0]->row(), 0))->pixmap();
    if (pic->width() > pic->height()) {
        h = h * pic->height() / pic->width();
    } else {
        w = w * pic->width() / pic->height();
    }
    QJsonObject info { { "mimetype", "image/png" }, { "w", w }, { "h", h } };
    QJsonObject content({ { "body", sticker_text }, { "url", QString("mxc://%1/%2").arg(getItemText(sel[1])).arg(getItemText(sel[2])) }, { "info", info } });
    auto encodedJson = QJsonDocument(content).toJson(QJsonDocument::Compact);
    connect(m_network->put(req, encodedJson), &QNetworkReply::finished, this, &MainWindow::sendFinished);
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
        QFile::copy(filename, QString("packs/%1/%2_%3.png").arg(pack_name).arg(mxc_parts[0]).arg(mxc_parts[1]));
        if (ui->cb_stickerpack->currentText() == pack_name) {
            emit ui->cb_stickerpack->currentTextChanged(pack_name);
        }
    }
}

void MainWindow::packChanged(const QString& text)
{
    ui->tableWidget->setRowCount(0);
    for (auto& d : QDir("packs/" + text).entryInfoList(QDir::NoDotAndDotDot | QDir::Files)) {
        auto mxc = d.completeBaseName();
        auto s = mxc.split('_');
        if (s.length() == 2) {
            insertRow(d.filePath(), m_settings->value("names/" + s[1]).toString(), s[0], s[1]);
        }
    }
    ui->tableWidget->resizeRowsToContents();
}

void MainWindow::stickerRenamed(QTableWidgetItem* item)
{
    auto text = getItemText(item);
    m_settings->setValue("names/" + getItemText(ui->tableWidget->item(item->row(), 3)), text);
}

void MainWindow::insertRow(const QString& image_path, const QString& description, const QString& server, const QString& code)
{
    ui->tableWidget->blockSignals(true); // don't want to trigger editing signal
    int idx = ui->tableWidget->rowCount();
    ui->tableWidget->insertRow(idx);
    auto icon = new QLabel();
    icon->setPixmap(QPixmap(image_path).scaled(128, 128, Qt::AspectRatioMode::KeepAspectRatio, Qt::TransformationMode::SmoothTransformation));
    icon->setAlignment(Qt::AlignCenter);
    ui->tableWidget->setCellWidget(idx, 0, icon);
    ui->tableWidget->setItem(idx, 1, new QTableWidgetItem(description));
    auto mxc_server = new QTableWidgetItem(server);
    mxc_server->setFlags(mxc_server->flags() & ~Qt::ItemIsEditable);
    ui->tableWidget->setItem(idx, 2, mxc_server);
    auto mxc_code = new QTableWidgetItem(code);
    mxc_code->setFlags(mxc_code->flags() & ~Qt::ItemIsEditable);
    ui->tableWidget->setItem(idx, 3, mxc_code);
    ui->tableWidget->blockSignals(false);
}

void MainWindow::listPacks()
{
    ui->cb_stickerpack->clear();
    for (auto& d : QDir("packs").entryList(QDir::NoDotAndDotDot | QDir::Dirs)) {
        ui->cb_stickerpack->addItem(d);
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
    auto filter = ui->le_filter->text().toLower();
    for (int i = 0; i < ui->tableWidget->rowCount(); ++i) {
        ui->tableWidget->setRowHidden(i, !filter.isEmpty() && !getItemText(ui->tableWidget->item(i, 1)).toLower().contains(filter));
    }
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
    QFile::remove(QString("packs/%1/%2_%3.png").arg(ui->cb_stickerpack->currentText()).arg(getItemText(sel[1])).arg(getItemText(sel[2])));
    emit ui->cb_stickerpack->currentTextChanged(ui->cb_stickerpack->currentText());
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
    if (newpack.contains('/') || newpack.contains('\\')) {
        QMessageBox::critical(this, "Ошибка", "Неверное имя стикерпака.");
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
