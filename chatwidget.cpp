#include "chatwidget.h"
#include "./ui_ChatWidget.h"
#include "networker.h"
#include <QDateTime>

ChatWidget::ChatWidget(const QString& username, NetWorker* networker, QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::ChatWidget)
    , m_networker(networker)
    , m_username(username)
{
    ui->setupUi(this);
    setWindowTitle("聊天客户端 - " + username);

    //连接信号与槽
    connect(networker,&NetWorker::disconnected,this,&ChatWidget::onDisconnected);
    connect(networker,&NetWorker::messageReceived,this,&ChatWidget::onMessageReceived);
    connect(networker,&NetWorker::errorOccurred,this,&ChatWidget::onError);

    //连接发送按钮
    connect(ui->send_btn,&QPushButton::clicked,
            this, &ChatWidget::onSendClicked);
    //回车发送消息
    connect(ui->input_lineE, &QLineEdit::returnPressed,
            this, &ChatWidget::onSendClicked);

    ui->input_lineE->setFocus();
}

ChatWidget::~ChatWidget() = default;


void ChatWidget::onSendClicked()
{
    QString msg = ui->input_lineE->text().trimmed();
    if (msg.isEmpty()) return;

    // 显示在界面上
    appendMessage(m_username, msg);

    // 将消息发送给服务器（QString 转 std::string）
    QByteArray utf8Data = msg.toUtf8();
    m_networker->SendChat(std::string(utf8Data.data(), utf8Data.size()));

    ui->input_lineE->clear();
    ui->input_lineE->setFocus();
}

void ChatWidget::onMessageReceived(const QString& msg)
{
    // 服务器发来的消息格式：可以解析发送者
    QString timestamp = QDateTime::currentDateTime().toString("hh:mm:ss");
    ui->display_textE->append(QString("[%1] %2").arg(timestamp).arg(msg));
}

void ChatWidget::onDisconnected()
{
    appendMessage("系统", "与服务器断开连接");
    ui->send_btn->setEnabled(false);
    ui->input_lineE->setEnabled(false);
}

void ChatWidget::onError(const QString& error)
{
    appendMessage("错误", error);
}

void ChatWidget::appendMessage(const QString& sender, const QString& msg)
{
    QString timestamp = QDateTime::currentDateTime().toString("hh:mm:ss");
    if (sender == m_username) {
        ui->display_textE->append(QString("[%1] 我：%2").arg(timestamp).arg(msg));
    } else {
        ui->display_textE->append(QString("[%1] %2：%3").arg(timestamp).arg(sender).arg(msg));
    }
}