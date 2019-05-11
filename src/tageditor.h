#ifndef TAGEDITOR_H
#define TAGEDITOR_H

#include <QCompleter>
#include <QDialog>
#include <QListWidget>
#include <QStyledItemDelegate>

namespace Ui {
class TagEditor;
}

class TagDelegate : public QStyledItemDelegate {
public:
    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const;
};

class TagEditor : public QDialog {
    Q_OBJECT

public:
    explicit TagEditor(QWidget* parent = nullptr);
    void setAvailableTags(const QStringList& tags);
    QStringList getTags();
    void setTags(const QStringList& tags);
    ~TagEditor();
private slots:
    void tagAdded(const QString& tag);
    void removeTag(QListWidgetItem* item);

private:
    Ui::TagEditor* ui;
    QCompleter* m_completer;

    // QWidget interface
protected:
    void showEvent(QShowEvent *event);
};

#endif // TAGEDITOR_H
