#ifndef LOGINWIDGET_H
#define LOGINWIDGET_H

#include <QWidget>
#include <memory>

class NetWorker;

namespace Ui {
class LoginWidget;
}

class LoginWidget : public QWidget
{
    Q_OBJECT

public:
    explicit LoginWidget(QWidget *parent = nullptr);
    ~LoginWidget();

private slots:
    void onLoginClicked();
    void onRegisterClicked();
    void onConnected();
    void onLoginResponse(int result, const QString& message);
    void onRegisterResponse(int result, const QString& message);
    void onError(const QString& error);

signals:
    void loginSuccess(const QString& username);

private:
    std::unique_ptr<Ui::LoginWidget> ui;
    void showMessage(const QString& msg, bool isError = false);
    NetWorker* m_netWorker;
};

#endif // LOGINWIDGET_H
