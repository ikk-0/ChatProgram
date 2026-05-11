#include "loginwidget.h"
#include "ui_loginwidget.h"
#include "networker.h"
#include "chatwidget.h"
#include <QMessageBox>

#define SER_IP "192.168.233.129"
#define SER_PORT 8888

LoginWidget::LoginWidget(QWidget *parent)
    : QWidget(parent)
    ,m_netWorker(nullptr)
    , ui(new Ui::LoginWidget)
{
    ui->setupUi(this);
    setWindowTitle("登录");

    m_netWorker=new NetWorker(this);
    //将当前对象（LoginWidget）设置为 NetWorker 的父对象
    //当 LoginWidget 被销毁时，Qt 会自动销毁 m_netWorker

    //连接信号与槽
    connect(m_netWorker,&NetWorker::connected
            ,this,&LoginWidget::onConnected);
    connect(m_netWorker,&NetWorker::loginResponse
            ,this,&LoginWidget::onLoginResponse);
    connect(m_netWorker,&NetWorker::registerResponse
            ,this,&LoginWidget::onRegisterResponse);
    connect(m_netWorker,&NetWorker::errorOccurred
            ,this,&LoginWidget::onError);
    //连接按钮
    connect(ui->loginBtn,&QPushButton::clicked
            ,this,&LoginWidget::onLoginClicked);
    connect(ui->registerBtn,&QPushButton::clicked
            ,this,&LoginWidget::onRegisterClicked);

    //防止用户在没连上服务器的时候就点击登录注册
    ui->loginBtn->setEnabled(false);
    ui->registerBtn->setEnabled(false);

    m_netWorker->Start(SER_IP,SER_PORT);
}

LoginWidget::~LoginWidget()=default;

void LoginWidget::onConnected()
{
    showMessage("已连接到服务器");
    ui->loginBtn->setEnabled(true);
    ui->registerBtn->setEnabled(true);
}

void LoginWidget::onLoginClicked()
{
    QString username=ui->usernameEdit->text().trimmed();
    QString password=ui->passwordEdit->text();

    if(username.isEmpty()||password.isEmpty())
    {
        showMessage("请输入用户名和密码",true);
        return;
    }

    ui->loginBtn->setEnabled(false);
    showMessage("登录中...");
    m_netWorker->SendLogin(username.toStdString(),password.toStdString());
}

void LoginWidget::onRegisterClicked()
{
    QString username=ui->usernameEdit->text().trimmed();
    QString password=ui->passwordEdit->text();

    if(username.isEmpty()||password.isEmpty())
    {
        showMessage("请输入用户名和密码",true);
        return;
    }

    ui->registerBtn->setEnabled(false);
    showMessage("注册中...");
    m_netWorker->SendRegister(username.toStdString(),password.toStdString(),username.toStdString());
}

void LoginWidget::onLoginResponse(int result, const QString& message)
{
    ui->loginBtn->setEnabled(true);

    if(result==0)
    {//登录成功
        showMessage("登录成功！");
        //打开聊天窗口
        QString username = ui->usernameEdit->text();
        ChatWidget* chatWindow=new ChatWidget(username,m_netWorker);
        chatWindow->show();
        this->hide();
    }else {
        showMessage("登录失败：" + message, true);
    }
}

void LoginWidget::onRegisterResponse(int result, const QString& message)
{
    ui->registerBtn->setEnabled(true);

    if(result==0)
    {
        showMessage("注册成功！请登录");
        ui->passwordEdit->clear();
    }else{
        showMessage("注册失败：" + message, true);
    }
}

void LoginWidget::onError(const QString& error)
{
    ui->loginBtn->setEnabled(true);
    ui->registerBtn->setEnabled(true);
    showMessage("网络错误：" + error, true);
}

void LoginWidget::showMessage(const QString& msg, bool isError)
{
    ui->statusLabel->setText(msg);
    if (isError) {
        ui->statusLabel->setStyleSheet("color: red;");
    } else {
        ui->statusLabel->setStyleSheet("color: green;");
    }
}









