#include "roomseditorpage.h"
#include "itemutil.h"
#include "ui_roomseditorpage.h"
#include <QMessageBox>

RoomsEditorPage::RoomsEditorPage(QWidget* parent)
    : QWidget(parent)
    , ui(new Ui::RoomsEditorPage)
{
    ui->setupUi(this);
    ui->tw_rooms->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    ui->tw_rooms->setHorizontalHeaderLabels({ tr("Name"), tr("Address") });
    connect(ui->b_add, &QPushButton::clicked, [=] {
        ui->tw_rooms->insertRow(ui->tw_rooms->rowCount());
    });
    connect(ui->b_del, &QPushButton::clicked, [=] {
        if (ui->tw_rooms->selectedItems().empty()) {
            QMessageBox::critical(this, tr("Error"), tr("Choose room to delete."));
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
            result.append({ getItemText(ui->tw_rooms->item(i, 1)), getItemText(ui->tw_rooms->item(i, 0)), "" });
        }
    }
    return result;
}

RoomsEditorPage::~RoomsEditorPage()
{
    delete ui;
}
