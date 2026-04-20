#include "client/connection_dialog.h"

#include "client/logging.h"
#include "client/network_manager.h"
#include "client/protocol_codec.h"
#include "collab_protocol/protocol.h"

#include <QFormLayout>
#include <QHBoxLayout>
#include <QIntValidator>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QMetaObject>
#include <QPushButton>
#include <QSettings>
#include <QVBoxLayout>

namespace collab_client {

ConnectionDialog::ConnectionDialog(NetworkManager* nm, QWidget* parent)
    : QDialog(parent), nm_(nm) {
    setWindowTitle(tr("Connect to server"));

    host_edit_ = new QLineEdit;
    port_edit_ = new QLineEdit;
    port_edit_->setValidator(new QIntValidator(1, 65535, this));
    username_edit_ = new QLineEdit;
    password_edit_ = new QLineEdit;
    password_edit_->setEchoMode(QLineEdit::Password);

    QSettings settings;
    host_edit_->setText(settings.value("lastHost", "127.0.0.1").toString());
    port_edit_->setText(settings.value("lastPort", 9000).toString());
    username_edit_->setText(settings.value("lastUser", "").toString());

    login_btn_ = new QPushButton(tr("Login"));
    register_btn_ = new QPushButton(tr("Register"));
    status_label_ = new QLabel;
    status_label_->setWordWrap(true);

    auto* form = new QFormLayout;
    form->addRow(tr("Host:"), host_edit_);
    form->addRow(tr("Port:"), port_edit_);
    form->addRow(tr("Username:"), username_edit_);
    form->addRow(tr("Password:"), password_edit_);

    auto* buttons = new QHBoxLayout;
    buttons->addWidget(login_btn_);
    buttons->addWidget(register_btn_);

    auto* root = new QVBoxLayout(this);
    root->addLayout(form);
    root->addLayout(buttons);
    root->addWidget(status_label_);

    connect(login_btn_, &QPushButton::clicked, this, &ConnectionDialog::onLoginClicked);
    connect(register_btn_, &QPushButton::clicked, this, &ConnectionDialog::onRegisterClicked);

    connect(nm_, &NetworkManager::connected, this, &ConnectionDialog::onConnected);
    connect(nm_, &NetworkManager::disconnected, this, &ConnectionDialog::onDisconnected);
    connect(nm_, &NetworkManager::errorOccurred, this, &ConnectionDialog::onErrorOccurred);
    connect(nm_, &NetworkManager::messageReceived, this, &ConnectionDialog::onMessageReceived);
}

QString ConnectionDialog::host() const { return host_edit_->text().trimmed(); }
quint16 ConnectionDialog::port() const {
    return static_cast<quint16>(port_edit_->text().toUInt());
}
QString ConnectionDialog::username() const { return username_edit_->text().trimmed(); }

void ConnectionDialog::onLoginClicked() { startAuth("login"); }
void ConnectionDialog::onRegisterClicked() { startAuth("register"); }

void ConnectionDialog::startAuth(const QString& action) {
    if (host().isEmpty() || port() == 0 || username().isEmpty()) {
        reportFailure(tr("Host, port, and username are required."));
        return;
    }
    pending_action_ = action;
    setInputsEnabled(false);
    status_label_->setText(tr("Connecting to %1:%2…").arg(host()).arg(port()));

    qCInfo(logAuth) << action << "requested for user=" << username()
                    << "host=" << host() << "port=" << port();
    QMetaObject::invokeMethod(nm_, "connectToHost", Qt::QueuedConnection,
                              Q_ARG(QString, host()), Q_ARG(quint16, port()));
}

void ConnectionDialog::onConnected() {
    if (pending_action_.isEmpty()) return;
    status_label_->setText(tr("Authenticating…"));
    auto payload = encode_auth_request(pending_action_, username(), password_edit_->text());
    QMetaObject::invokeMethod(nm_, "sendFrame", Qt::QueuedConnection,
                              Q_ARG(QByteArray, payload));
}

void ConnectionDialog::onDisconnected(QString reason) {
    if (pending_action_.isEmpty()) return;
    reportFailure(tr("Disconnected: %1").arg(reason));
}

void ConnectionDialog::onErrorOccurred(QString message) {
    if (pending_action_.isEmpty()) return;
    reportFailure(message);
}

void ConnectionDialog::onMessageReceived(QByteArray payload) {
    if (pending_action_.isEmpty()) return;

    auto env = parse_envelope(payload);
    if (env.type != server::MessageType::AuthResponse) {
        return;
    }

    auto auth = parse_auth_response(payload);
    if (!auth) {
        reportFailure(tr("Malformed auth_response"));
        return;
    }
    if (!auth->success) {
        qCWarning(logAuth) << "auth failed code=" << QString::fromStdString(auth->error);
        const auto code = QString::fromStdString(auth->error);
        const auto human = code == "invalid_credentials"
            ? tr("Invalid username or password.")
            : code == "username_taken"
                ? tr("Username is already taken.")
                : tr("Auth error: %1").arg(code);
        reportFailure(human);
        return;
    }

    qCInfo(logAuth) << "auth success user=" << username() << "userId=" << auth->userId;
    nm_->setIdentity(auth->userId, username());

    QSettings settings;
    settings.setValue("lastHost", host());
    settings.setValue("lastPort", port());
    settings.setValue("lastUser", username());

    pending_action_.clear();
    accept();
}

void ConnectionDialog::setInputsEnabled(bool enabled) {
    host_edit_->setEnabled(enabled);
    port_edit_->setEnabled(enabled);
    username_edit_->setEnabled(enabled);
    password_edit_->setEnabled(enabled);
    login_btn_->setEnabled(enabled);
    register_btn_->setEnabled(enabled);
}

void ConnectionDialog::reportFailure(const QString& message) {
    pending_action_.clear();
    setInputsEnabled(true);
    status_label_->setText(message);
    QMessageBox::warning(this, tr("Connection failed"), message);
    QMetaObject::invokeMethod(nm_, "disconnectFromHost", Qt::QueuedConnection);
}

} // namespace collab_client
