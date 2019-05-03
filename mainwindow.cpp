#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QDebug>
#include <QDesktopWidget>
#include <QDirIterator>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QMessageBox>
#include <QNetworkReply>
#include <QInputDialog>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_network(new QNetworkAccessManager(this))
    , m_settings(new QSettings("rkfg", "mxstickers", this))
    , m_preferences_dialog(new Preferences(m_settings, this))
{
    ui->setupUi(this);
    ui->tableWidget->setHorizontalHeaderLabels({ "Изображение", "Название", "Код" });
    ui->tableWidget->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    connect(ui->tableWidget, &QTableWidget::itemSelectionChanged, this, &MainWindow::selectionChanged);
    connect(ui->b_send, &QPushButton::clicked, this, &MainWindow::send);
    connect(m_network, &QNetworkAccessManager::finished, this, &MainWindow::finished);
    connect(ui->cb_stickerpack, &QComboBox::currentTextChanged, this, &MainWindow::packChanged);
    connect(ui->tableWidget, &QTableWidget::itemChanged, this, &MainWindow::stickerRenamed);
    connect(ui->b_preferences, &QPushButton::clicked, m_preferences_dialog, &QDialog::open);
    connect(m_preferences_dialog, &Preferences::settingsUpdated, this, &MainWindow::listRooms);
    setGeometry(QStyle::alignedRect(Qt::LeftToRight, Qt::AlignCenter, size(), QApplication::desktop()->availableGeometry()));
    listPacks();
    listRooms();
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::selectionChanged()
{
    auto sel = ui->tableWidget->selectedItems();
    if (!sel.empty()) {
        qDebug() << sel[0]->data(Qt::DisplayRole).toString();
    }
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
    auto sticker_text = sel[0]->data(Qt::DisplayRole).toString();
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
    QJsonObject content({ { "body", sticker_text }, { "url", "mxc://matrix.org/" + sel[1]->data(Qt::DisplayRole).toString() }, { "info", info } });
    QString event_id = QString("$%1%2:matrix.org").arg(time(NULL)).arg(rand());
    auto encodedJson = QJsonDocument(content).toJson(QJsonDocument::Compact);
    qDebug().noquote() << encodedJson;
    QNetworkRequest req(QString("https://%1/_matrix/client/r0/rooms/%2/send/m.sticker/%3?access_token=%4").arg("behind.computer").arg(room.toString()).arg(event_id).arg(m_settings->value("matrix/access_token").toString()));
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
        insertRow({ m_settings->value("names/" + mxc).toString(), mxc, d.filePath() });
    }
    ui->tableWidget->resizeRowsToContents();
}

void MainWindow::stickerRenamed(QTableWidgetItem* item)
{
    auto text = item->data(Qt::DisplayRole);
    m_settings->setValue("names/" + ui->tableWidget->item(item->row(), 2)->data(Qt::DisplayRole).toString(), text);
}

void MainWindow::insertRow(QList<QString> items)
{
    ui->tableWidget->blockSignals(true); // don't want to trigger editing signal
    int idx = ui->tableWidget->rowCount();
    ui->tableWidget->insertRow(idx);
    ui->tableWidget->setItem(idx, 1, new QTableWidgetItem(items[0]));
    auto mxc = new QTableWidgetItem(items[1]);
    mxc->setFlags(mxc->flags() & ~Qt::ItemIsEditable);
    ui->tableWidget->setItem(idx, 2, mxc);
    auto icon = new QLabel();
    icon->setPixmap(QPixmap(items[2]).scaled(128, 128, Qt::AspectRatioMode::KeepAspectRatio, Qt::TransformationMode::SmoothTransformation));
    icon->setAlignment(Qt::AlignCenter);
    ui->tableWidget->setCellWidget(idx, 0, icon);
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
