#ifndef ROOMSEDITORPAGE_H
#define ROOMSEDITORPAGE_H

#include <QSettings>
#include <QWidget>

namespace Ui {
class RoomsEditorPage;
}

struct Room {
    QString address;
    QString name;
};

class RoomsEditorPage : public QWidget {
    Q_OBJECT

public:
    explicit RoomsEditorPage(QWidget* parent = nullptr);
    void setRooms(const QList<Room>& rooms);
    QList<Room> rooms() const;
    ~RoomsEditorPage();

private:
    Ui::RoomsEditorPage* ui;
};

#endif // ROOMSEDITORPAGE_H
