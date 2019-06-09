#include "preferences.h"
#include "ui_preferences.h"

Preferences::Preferences(QSettings* settings, QWidget* parent)
    : QDialog(parent)
    , ui(new Ui::Preferences)
    , m_settings(settings)
    , m_access_token_page(new AccessTokenPage)
    , m_rooms_editor_page(new RoomsEditorPage)
{
    ui->setupUi(this);
    ui->lw_pages->addItem(new QListWidgetItem(QIcon(":/res/icons/user-identity.png"), tr("Access token")));
    ui->sw_content->addWidget(m_access_token_page);
    connect(ui->lw_pages, &QListWidget::currentRowChanged, ui->sw_content, &QStackedWidget::setCurrentIndex);
    ui->lw_pages->setCurrentRow(0);
}

QList<Room> Preferences::loadRooms()
{
    m_settings->beginGroup("matrix/rooms");
    QList<Room> rooms;
    for (auto& r : m_settings->childKeys()) {
        rooms.append({ r, m_settings->value(r).toString(), "" });
    }
    m_settings->endGroup();
    return rooms;
}

void Preferences::saveRooms(const QList<Room>& rooms)
{
    m_settings->beginGroup("matrix/rooms");
    for (auto& r : m_settings->childKeys()) {
        m_settings->remove(r);
    }
    for (auto& r : rooms) {
        m_settings->setValue(r.address, r.name);
    }
    m_settings->endGroup();
}

Preferences::~Preferences()
{
    delete ui;
}

void Preferences::open()
{
    m_settings->beginGroup("matrix");
    m_access_token_page->setAccessToken(m_settings->value("access_token").toString());
    m_access_token_page->setServer(m_settings->value("server").toString());
    m_settings->endGroup();
    m_rooms_editor_page->setRooms(loadRooms());
    QDialog::open();
}

void Preferences::accept()
{
    m_settings->beginGroup("matrix");
    m_settings->setValue("access_token", m_access_token_page->accessToken());
    m_settings->setValue("server", m_access_token_page->server());
    m_settings->endGroup();
    saveRooms(m_rooms_editor_page->rooms());
    QDialog::accept();
    emit settingsUpdated();
}
