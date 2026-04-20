#pragma once

#include <QDialog>
#include <QString>

class QLineEdit;
class QPushButton;
class QLabel;

namespace collab_client {

class NetworkManager;

class ConnectionDialog : public QDialog {
    Q_OBJECT

public:
    explicit ConnectionDialog(NetworkManager* nm, QWidget* parent = nullptr);

    QString host() const;
    quint16 port() const;
    QString username() const;

private slots:
    void onLoginClicked();
    void onRegisterClicked();
    void onConnected();
    void onDisconnected(QString reason);
    void onErrorOccurred(QString message);
    void onMessageReceived(QByteArray payload);

private:
    void buildUi();
    void wireSignals();
    void loadSettings();
    void persistSettings();
    void startAuth(const QString& action);
    void setInputsEnabled(bool enabled);
    void reportFailure(const QString& message);
    QString humanAuthError(const QString& code) const;

    NetworkManager* nm_;

    QLineEdit* host_edit_;
    QLineEdit* port_edit_;
    QLineEdit* username_edit_;
    QLineEdit* password_edit_;
    QPushButton* login_btn_;
    QPushButton* register_btn_;
    QLabel* status_label_;

    QString pending_action_;
};

}
