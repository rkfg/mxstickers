#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "preferences.h"
#include <QMainWindow>
#include <QMenu>
#include <QNetworkAccessManager>
#include <QSettings>
#include <QTableWidget>
#include "dbmanager.h"

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    bool eventFilter(QObject* watched, QEvent* event);
    bool init();
    ~MainWindow();
private slots:
    void send();
    void sendFinished();
    void uploadFinished();
    void reloadStickers();
    void packChanged(const QString& text);
    void stickerRenamed(QTableWidgetItem* item);
    void rescanPacks();
    void moveStickerToPack(const QString& pack);
private:
    Ui::MainWindow* ui;
    QNetworkAccessManager* m_network;
    QSettings* m_settings;
    Preferences* m_preferences_dialog;
    QMenu* m_sticker_context_menu;
    QMenu* m_move_to_menu;
    DBManager* m_dbmanager;
    void insertRow(const Sticker &s);
    void listPacks();
    void listRooms();
    void filterStickers();
    void addSticker();
    void removeSticker();
    void validatePack(const QString &packname);
    QString buildRequest(const QString& method, const QString& type = "client");
    void createPack();
    void removePack();
    QStringList getServerCode(int row);
    QString getServer(int row);
    QString getCode(int row);
    void renamePack();
    QString getStickerPath(int row, const QString &pack = "");
};

#endif // MAINWINDOW_H
