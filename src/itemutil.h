#ifndef ITEMUTIL_H
#define ITEMUTIL_H

#include <QTableWidgetItem>

inline QString getItemText(QTableWidgetItem* i) {
    return i->data(Qt::DisplayRole).toString();
}

#endif // ITEMUTIL_H
