#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "preferences.h"
#include <QMainWindow>
#include <QMenu>
#include <QNetworkAccessManager>
#include <QSettings>
#include <QTableWidget>

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    bool eventFilter(QObject* watched, QEvent* event);
    ~MainWindow();
private slots:
    void send();
    void sendFinished();
    void uploadFinished();
    void packChanged(const QString& text);
    void stickerRenamed(QTableWidgetItem* item);

private:
    Ui::MainWindow* ui;
    QNetworkAccessManager* m_network;
    QSettings* m_settings;
    Preferences* m_preferences_dialog;
    QMenu* m_sticker_context_menu;
    void insertRow(const QString& image_path, const QString& description, const QString& server, const QString& code);
    void listPacks();
    void listRooms();
    void filterStickers();
    void addSticker();
    void removeSticker();
    QString buildRequest(const QString& method, const QString& type = "client");
    void createPack();
    void removePack();
    QStringList getServerCode(int row);
};

#endif // MAINWINDOW_H
