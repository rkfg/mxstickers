#include "src/tageditor.h"
#include "ui_tageditor.h"

#include <QAction>
#include <QDebug>
#include <QMessageBox>
#include <QPainter>

TagEditor::TagEditor(QWidget* parent)
    : QDialog(parent)
    , ui(new Ui::TagEditor)
    , m_completer(new QCompleter(this))
{
    ui->setupUi(this);
    auto add_tag = new QAction();
    add_tag->setShortcut(QKeySequence("Return"));
    connect(add_tag, &QAction::triggered, [=] {
        tagAdded(ui->le_tag->text());
    });
    ui->le_tag->addAction(add_tag);
    connect(ui->lw_tags, &QListWidget::itemClicked, this, &TagEditor::removeTag);
    ui->lw_tags->setItemDelegate(new TagDelegate);
}

void TagEditor::setAvailableTags(const QStringList& tags)
{
    m_completer = new QCompleter(tags);
    ui->le_tag->setCompleter(m_completer);
    connect(m_completer, QOverload<const QString&>::of(&QCompleter::activated), this, &TagEditor::tagAdded, Qt::QueuedConnection);
}

QStringList TagEditor::getTags()
{
    QStringList result;
    for (int i = 0; i < ui->lw_tags->count(); ++i) {
        result << ui->lw_tags->item(i)->data(Qt::DisplayRole).toString();
    }
    return result;
}

void TagEditor::setTags(const QStringList& tags)
{
    ui->lw_tags->clear();
    for (auto& tag : tags) {
        tagAdded(tag);
    }
}

TagEditor::~TagEditor()
{
    delete ui;
}

void TagEditor::tagAdded(const QString& tag)
{
    if (tag.isEmpty()) {
        accept();
        return;
    }
    auto item = new QListWidgetItem(tag.toLower());
    item->setData(Qt::ForegroundRole, QColor("#39739D"));
    ui->lw_tags->addItem(item);
    ui->le_tag->clear();
}

void TagEditor::removeTag(QListWidgetItem* item)
{
    ui->lw_tags->removeItemWidget(item);
    delete item;
}

void TagEditor::showEvent(QShowEvent *event)
{
    ui->le_tag->setFocus();
    QDialog::showEvent(event);
}

void TagDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    auto rect = option.rect.marginsAdded(QMargins(2, 2, 2, 2));
    painter->save();
    painter->setRenderHints(QPainter::Antialiasing);
    painter->setBrush(QBrush(QColor("#E1ECF4")));
    painter->setPen(QColor("#39739D"));
    painter->drawRoundedRect(rect, 3, 3);
    QStyledItemDelegate::paint(painter, option, index);
    painter->restore();
}
