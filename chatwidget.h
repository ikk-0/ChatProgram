#ifndef CHATWIDGET_H
#define CHATWIDGET_H

#include <QWidget>
#include "networker.h"

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
    void onMessageReceived(const QString& msg);
    void onDisconnected();
    void onError(const QString& error);

private:
    void appendMessage(const QString& sender, const QString& msg);

    std::unique_ptr<Ui::ChatWidget> ui;
    NetWorker* m_networker;
    QString m_username;
};
#endif // CHATWIDGET_H
