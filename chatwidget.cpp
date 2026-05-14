#include "chatwidget.h"
#include "ui_ChatWidget.h"
#include "networker.h"
#include <QDateTime>
#include <QVBoxLayout>
#include <QSplitter>
#include <QDebug>

ChatWidget::ChatWidget(const QString& username, NetWorker* networker, QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::ChatWidget)
    , m_netWorker(networker)
    , m_username(username)
    , m_unreadCount()
    , m_currentChatUser()
{
    ui->setupUi(this);
    setWindowTitle("聊天室 - " + username);

    // 直接从 UI 获取控件指针
    m_userList = ui->userList;
    m_displayEdit = ui->display_textE;
    m_inputEdit = ui->input_lineE;
    m_sendBtn = ui->send_btn;

    m_userList->setFixedWidth(150);

    // 连接信号槽（与之前相同）
    connect(m_netWorker, &NetWorker::userListReceived,
            this, &ChatWidget::onUserListReceived);
    connect(m_netWorker, &NetWorker::userOnline,
            this, &ChatWidget::onUserOnline);
    connect(m_netWorker, &NetWorker::userOffline,
            this, &ChatWidget::onUserOffline);
    connect(m_netWorker, &NetWorker::privateMessageReceived,
            this, &ChatWidget::onPrivateMessageReceived);
    connect(m_userList, &QListWidget::itemClicked,
            this, &ChatWidget::onUserListItemClicked);
    connect(m_sendBtn, &QPushButton::clicked,
            this, &ChatWidget::onSendClicked);
    connect(m_inputEdit, &QLineEdit::returnPressed,
            this, &ChatWidget::onSendClicked);

    m_inputEdit->setFocus();
}

ChatWidget::~ChatWidget() = default;

void ChatWidget::onSendClicked()
{
    QString msg = m_inputEdit->text().trimmed();
    if (msg.isEmpty()) return;

    if (m_currentChatUser.empty()) {
        appendMessage("系统", "请先选择要聊天的用户", true);
        return;
    }

    // 显示在界面上
    appendMessage(m_username, msg, true);

    // 发送私聊消息
    m_netWorker->SendPrivateChat(m_currentChatUser, msg.toStdString());

    m_inputEdit->clear();
    m_inputEdit->setFocus();
}

void ChatWidget::onUserListReceived(const QList<QString>& usernames)
{
    m_userList->clear();
    m_unreadCount.clear();

    for (const QString& name : usernames) {
        if (name != m_username) {
            QListWidgetItem* item = new QListWidgetItem(name);
            item->setForeground(Qt::green);
            m_userList->addItem(item);
        }
    }
    //appendMessage("系统", QString("当前在线 %1 人").arg(m_userList->count()), false);
}

void ChatWidget::onUserOnline(const QString& username)
{
    if (username == m_username) return;

    // 检查是否已在列表中
    bool exists = false;
    for (int i = 0; i < m_userList->count(); i++) {
        QString itemText = m_userList->item(i)->text();
        if (itemText == username) {
            exists = true;
            m_userList->item(i)->setForeground(Qt::green);
            break;
        }
    }

    if (!exists) {
        QListWidgetItem* item = new QListWidgetItem(username);
        item->setForeground(Qt::green);
        m_userList->addItem(item);
    }

    appendMessage("系统", username + " 上线了", false);
}

void ChatWidget::onUserOffline(const QString& username)
{
    if (username == m_username) return;

    for (int i = 0; i < m_userList->count(); i++) {
        QString itemText = m_userList->item(i)->text();
        if (itemText == username) {
            m_userList->item(i)->setForeground(Qt::gray);
            break;
        }
    }

    appendMessage("系统", username + " 下线了", false);
}

void ChatWidget::onPrivateMessageReceived(const QString& fromUser, const QString& content)
{
    std::string fromUserStd = fromUser.toStdString();

    // 如果当前正在和该用户聊天，直接显示
    if (m_currentChatUser == fromUserStd) {
        appendMessage(fromUser, content, false);
    } else {
        // 否则标记未读
        m_unreadCount[fromUserStd]++;
        updateUnreadDisplay(fromUser);
    }
}

void ChatWidget::onUserListItemClicked(QListWidgetItem* item)
{
    QString username = item->text();
    setCurrentChatUser(username);
}

void ChatWidget::setCurrentChatUser(const QString& username)
{
    m_currentChatUser = username.toStdString();

    // 清除该用户的未读计数
    auto it = m_unreadCount.find(m_currentChatUser);
    if (it != m_unreadCount.end()) {
        it->second = 0;
        updateUnreadDisplay(username);
    }

    // 更新左侧列表选中样式
    for (int i = 0; i < m_userList->count(); i++) {
        QListWidgetItem* item = m_userList->item(i);
        if (item->text() == username) {
            item->setSelected(true);
            break;
        }
    }

    // 清空显示区域
    m_displayEdit->clear();
    appendMessage("系统", "开始与 " + username + " 聊天", false);
}

void ChatWidget::updateUnreadDisplay(const QString& username)
{
    std::string usernameStd = username.toStdString();
    int count = m_unreadCount[usernameStd];

    for (int i = 0; i < m_userList->count(); i++) {
        QListWidgetItem* item = m_userList->item(i);
        if (item->text().startsWith(username)) {
            if (count > 0) {
                item->setText(username + " (" + QString::number(count) + ")");
            } else {
                item->setText(username);
            }
            break;
        }
    }
}

void ChatWidget::appendMessage(const QString& sender, const QString& msg, bool isSelf)
{
    QString timestamp = QDateTime::currentDateTime().toString("hh:mm:ss");
    if (isSelf) {
        m_displayEdit->append(QString("[%1] 我：%2").arg(timestamp).arg(msg));
    } else {
        m_displayEdit->append(QString("[%1] %2：%3").arg(timestamp).arg(sender).arg(msg));
    }
}