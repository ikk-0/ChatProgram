#ifndef CHATWIDGET_H
#define CHATWIDGET_H

#include <QWidget>
#include <QListWidget>
#include <QTextEdit>
#include <QString>
#include <map>
#include <string>
#include <QPushButton>

class NetWorker;

QT_BEGIN_NAMESPACE
namespace Ui {
class ChatWidget;
}
QT_END_NAMESPACE

class ChatWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ChatWidget(const QString& username, NetWorker* netWorker, QWidget *parent = nullptr);
    ~ChatWidget() override;

private slots:
    void onSendClicked();
    void onUserListReceived(const QList<QString>& usernames);
    void onUserOnline(const QString& username);
    void onUserOffline(const QString& username);
    void onPrivateMessageReceived(const QString& fromUser, const QString& content);
    void onUserListItemClicked(QListWidgetItem* item);

private:
    void appendMessage(const QString& sender, const QString& msg, bool isSelf = false);
    void addUserToList(const QString& username);
    void removeUserFromList(const QString& username);
    void setCurrentChatUser(const QString& username);
    void updateUnreadDisplay(const QString& username);

    Ui::ChatWidget *ui;
    NetWorker* m_netWorker;
    QString m_username;

    // UI 组件
    QListWidget* m_userList;
    QTextEdit* m_displayEdit;
    QLineEdit* m_inputEdit;
    QPushButton* m_sendBtn;

    // 数据存储
    std::map<std::string, int> m_unreadCount;      // 用户名 -> 未读消息数
    std::string m_currentChatUser;                  // 当前正在聊天的用户
};
#endif // CHATWIDGET_H
