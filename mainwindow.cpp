#include "mainwindow.h"
#include "itemutil.h"
#include "ui_mainwindow.h"
#include <QAction>
#include <QDesktopWidget>
#include <QDirIterator>
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
{
    ui->setupUi(this);
    ui->tableWidget->setHorizontalHeaderLabels({ "Изображение", "Название", "Сервер", "Код" });
    ui->tableWidget->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    connect(ui->b_send, &QPushButton::clicked, this, &MainWindow::send);
    connect(m_network, &QNetworkAccessManager::finished, this, &MainWindow::finished);
    connect(ui->cb_stickerpack, &QComboBox::currentTextChanged, this, &MainWindow::packChanged);
    connect(ui->tableWidget, &QTableWidget::itemChanged, this, &MainWindow::stickerRenamed);
    connect(ui->b_preferences, &QPushButton::clicked, m_preferences_dialog, &QDialog::open);
    connect(m_preferences_dialog, &Preferences::settingsUpdated, this, &MainWindow::listRooms);
    connect(ui->le_filter, &QLineEdit::textChanged, this, &MainWindow::filterStickers);
    setGeometry(QStyle::alignedRect(Qt::LeftToRight, Qt::AlignCenter, size(), QApplication::desktop()->availableGeometry()));
    ui->le_filter->setClearButtonEnabled(true);
    listPacks();
    listRooms();
    ui->le_filter->installEventFilter(this);
}

bool MainWindow::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == ui->le_filter && event->type() == QEvent::KeyRelease) {
        auto key = static_cast<QKeyEvent*>(event)->key();
        switch (key) {
        case Qt::Key_Escape:
            ui->le_filter->clear();
            return true;
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
    auto access_token = m_settings->value("matrix/access_token").toString();
    if (access_token.isEmpty()) {
        QMessageBox::critical(this, "Ошибка", "Задайте токен доступа в настройках");
        return;
    }
    auto server = m_settings->value("matrix/server").toString();
    if (server.isEmpty()) {
        QMessageBox::critical(this, "Ошибка", "Задайте сервер Matrix в настройках");
        return;
    }
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
    QString event_id = QString("$%1%2:matrix.org").arg(time(NULL)).arg(rand());
    auto encodedJson = QJsonDocument(content).toJson(QJsonDocument::Compact);
    QNetworkRequest req(QString("https://%1/_matrix/client/r0/rooms/%2/send/m.sticker/%3?access_token=%4").arg(server).arg(room.toString()).arg(event_id).arg(access_token));
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    m_network->put(req, encodedJson);
}

void MainWindow::finished(QNetworkReply* reply)
{
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
