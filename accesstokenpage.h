#ifndef ACCESSTOKENPAGE_H
#define ACCESSTOKENPAGE_H

#include <QWidget>
#include <QSettings>

namespace Ui {
class AccessTokenPage;
}

class AccessTokenPage : public QWidget
{
 Q_OBJECT

public:
 explicit AccessTokenPage(QWidget *parent = nullptr);
 void setAccessToken(const QString& access_token);
 QString accessToken() const;
 ~AccessTokenPage();

private:
 Ui::AccessTokenPage *ui;
};

#endif // ACCESSTOKENPAGE_H
