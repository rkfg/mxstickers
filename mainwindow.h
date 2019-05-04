#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "preferences.h"
#include <QMainWindow>
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
    ~MainWindow();
private slots:
    void send();
    void finished(QNetworkReply* reply);
    void packChanged(const QString& text);
    void stickerRenamed(QTableWidgetItem* item);

private:
    Ui::MainWindow* ui;
    QNetworkAccessManager* m_network;
    QSettings* m_settings;
    Preferences* m_preferences_dialog;
    void insertRow(const QString& image_path, const QString& description, const QString& server, const QString& code);
    void listPacks();
    void listRooms();
    void filterStickers();
};

#endif // MAINWINDOW_H
