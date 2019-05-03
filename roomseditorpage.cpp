#include "roomseditorpage.h"
#include "ui_roomseditorpage.h"
#include <QMessageBox>

RoomsEditorPage::RoomsEditorPage(QWidget* parent)
    : QWidget(parent)
    , ui(new Ui::RoomsEditorPage)
{
    ui->setupUi(this);
    ui->tw_rooms->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    ui->tw_rooms->setHorizontalHeaderLabels({ "Название", "Адрес" });
    connect(ui->b_add, &QPushButton::clicked, [=] {
        ui->tw_rooms->insertRow(ui->tw_rooms->rowCount());
    });
    connect(ui->b_del, &QPushButton::clicked, [=] {
        if (ui->tw_rooms->selectedItems().empty()) {
            QMessageBox::critical(this, "Ошибка", "Выберите комнату для удаления.");
            return;
        }
        ui->tw_rooms->removeRow(ui->tw_rooms->selectedItems()[0]->row());
    });
}

void RoomsEditorPage::setRooms(const QList<Room>& rooms)
{
    ui->tw_rooms->setRowCount(0);
    for (auto& r : rooms) {
        int idx = ui->tw_rooms->rowCount();
        ui->tw_rooms->insertRow(idx);
        ui->tw_rooms->setItem(idx, 0, new QTableWidgetItem(r.name));
        ui->tw_rooms->setItem(idx, 1, new QTableWidgetItem(r.address));
    }
}

QList<Room> RoomsEditorPage::rooms() const
{
    QList<Room> result;
    for (int i = 0; i < ui->tw_rooms->rowCount(); ++i) {
        if (ui->tw_rooms->item(i, 0) && ui->tw_rooms->item(i, 1)) {
            result.append({ ui->tw_rooms
                                ->item(i, 1)
                                ->data(Qt::DisplayRole)
                                .toString(),
                ui->tw_rooms
                    ->item(i, 0)
                    ->data(Qt::DisplayRole)
                    .toString() });
        }
    }
    return result;
}

RoomsEditorPage::~RoomsEditorPage()
{
    delete ui;
}
