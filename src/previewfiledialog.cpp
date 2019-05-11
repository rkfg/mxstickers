#include "previewfiledialog.h"
#include <QGridLayout>

PreviewFileDialog::PreviewFileDialog(QWidget* parent, const QString & caption, const QString & directory,
		const QString & filter, int previewWidth) :
		QFileDialog(parent, caption, directory, filter) {
	QGridLayout *layout = (QGridLayout*) this->layout();
	if (!layout) {
		// this QFileDialog is a native one (Windows/KDE/...) and doesn't need to be extended with preview
		return;
	}
	QVBoxLayout* box = new QVBoxLayout();

    m_preview = new QLabel("", this);
    m_preview->setAlignment(Qt::AlignCenter);
    m_preview->setMinimumWidth(previewWidth);
    m_preview->setMinimumHeight(height());
	setMinimumWidth(width() + previewWidth);
    box->addWidget(m_preview);

	box->addStretch();

	// add to QFileDialog layout
	layout->addLayout(box, 1, 3, 3, 1);
    connect(this, &QFileDialog::currentChanged, this, &PreviewFileDialog::onCurrentChanged);
}

void PreviewFileDialog::onCurrentChanged(const QString & path) {
	QPixmap pixmap = QPixmap(path);
	if (pixmap.isNull()) {
        m_preview->clear();
	} else {
        m_preview->setPixmap(
                pixmap.scaled(m_preview->width(), height(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
	}
}
