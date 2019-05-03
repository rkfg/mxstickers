#include "accesstokenpage.h"
#include "ui_accesstokenpage.h"

AccessTokenPage::AccessTokenPage(QWidget* parent)
	: QWidget(parent)
	, ui(new Ui::AccessTokenPage)
{
	ui->setupUi(this);
}

void AccessTokenPage::setAccessToken(const QString& access_token)
{
	ui->pte_access_token->setPlainText(access_token);
}

QString AccessTokenPage::accessToken() const
{
    return ui->pte_access_token->toPlainText();
}

void AccessTokenPage::setServer(const QString &server)
{
    ui->le_server->setText(server);
}

QString AccessTokenPage::server() const
{
    return ui->le_server->text();
}

AccessTokenPage::~AccessTokenPage()
{
	delete ui;
}
