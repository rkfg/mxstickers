#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QNetworkAccessManager>
#include <QSettings>
#include <QTableWidget>
#include "preferences.h"

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
 Q_OBJECT

public:
 explicit MainWindow(QWidget *parent = nullptr);
 ~MainWindow();
private slots:
 void selectionChanged();
 void send();
 void finished(QNetworkReply *reply);
 void packChanged(const QString& text);
 void stickerRenamed(QTableWidgetItem *item);
private:
 Ui::MainWindow *ui;
 QNetworkAccessManager* m_network;
 QSettings* m_settings;
 Preferences* m_preferences_dialog;
 void insertRow(QList<QString> items);
 void listPacks();
 void listRooms();
};

#endif // MAINWINDOW_H
