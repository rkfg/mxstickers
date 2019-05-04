#ifndef PREFERENCES_H
#define PREFERENCES_H

#include "accesstokenpage.h"
#include "roomseditorpage.h"
#include <QDialog>
#include <QSettings>

namespace Ui {
class Preferences;
}

class Preferences : public QDialog {
    Q_OBJECT

public:
    explicit Preferences(QSettings* settings, QWidget* parent = nullptr);
    QList<Room> loadRooms();
    void saveRooms(const QList<Room>& rooms);
    ~Preferences();

private:
    Ui::Preferences* ui;
    QSettings* m_settings;
    AccessTokenPage* m_access_token_page;
    RoomsEditorPage* m_rooms_editor_page;

    // QDialog interface
public slots:
    void open();
    void accept();
signals:
    void settingsUpdated();
};

#endif // PREFERENCES_H
