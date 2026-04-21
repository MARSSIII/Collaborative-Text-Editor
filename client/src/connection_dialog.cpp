#include "client/connection_dialog.h"

#include "client/logging.h"
#include "client/network_manager.h"
#include "client/protocol_codec.h"
#include "collab_protocol/protocol.h"

#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QIntValidator>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSettings>
#include <QVBoxLayout>

namespace collab_client {

namespace {

QLabel* makeFormLabel(const QString& text) {
    auto* l = new QLabel(text);
    l->setObjectName("formLabel");
    return l;
}

}

ConnectionDialog::ConnectionDialog(NetworkManager* nm, QWidget* parent)
    : QDialog(parent), nm_(nm) {
    setWindowTitle(tr("Collab Editor"));
    setMinimumWidth(420);

    buildUi();
    loadSettings();
    wireSignals();
}

void ConnectionDialog::buildUi() {
    auto* title = new QLabel(tr("Collab Editor"));
    title->setObjectName("h1");
    auto* subtitle = new QLabel(tr("Sign in or create a new account."));
    subtitle->setObjectName("subtitle");

    host_edit_ = new QLineEdit;
    host_edit_->setPlaceholderText(tr("127.0.0.1"));
    port_edit_ = new QLineEdit;
    port_edit_->setValidator(new QIntValidator(1, 65535, this));
    port_edit_->setPlaceholderText(tr("9000"));
    username_edit_ = new QLineEdit;
    username_edit_->setPlaceholderText(tr("username"));
    password_edit_ = new QLineEdit;
    password_edit_->setEchoMode(QLineEdit::Password);
    password_edit_->setPlaceholderText(tr("password"));

    login_btn_ = new QPushButton(tr("Sign in"));
    login_btn_->setObjectName("primary");
    login_btn_->setDefault(true);
    register_btn_ = new QPushButton(tr("Register"));
    status_label_ = new QLabel;
    status_label_->setObjectName("muted");
    status_label_->setWordWrap(true);

    auto* form = new QFormLayout;
    form->setLabelAlignment(Qt::AlignLeft);
    form->setFormAlignment(Qt::AlignTop);
    form->setHorizontalSpacing(12);
    form->setVerticalSpacing(8);
    form->addRow(makeFormLabel(tr("Host")), host_edit_);
    form->addRow(makeFormLabel(tr("Port")), port_edit_);
    form->addRow(makeFormLabel(tr("Username")), username_edit_);
    form->addRow(makeFormLabel(tr("Password")), password_edit_);

    auto* buttons = new QHBoxLayout;
    buttons->addWidget(register_btn_);
    buttons->addStretch();
    buttons->addWidget(login_btn_);

    auto* card = new QFrame;
    card->setObjectName("card");
    auto* cardLayout = new QVBoxLayout(card);
    cardLayout->setContentsMargins(24, 22, 24, 22);
    cardLayout->setSpacing(12);
    cardLayout->addWidget(title);
    cardLayout->addWidget(subtitle);
    cardLayout->addSpacing(6);
    cardLayout->addLayout(form);
    cardLayout->addSpacing(4);
    cardLayout->addLayout(buttons);
    cardLayout->addWidget(status_label_);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(16, 16, 16, 16);
    root->addWidget(card);
}

void ConnectionDialog::wireSignals() {
    connect(login_btn_, &QPushButton::clicked, this, &ConnectionDialog::onLoginClicked);
    connect(register_btn_, &QPushButton::clicked, this, &ConnectionDialog::onRegisterClicked);

    connect(nm_, &NetworkManager::connected, this, &ConnectionDialog::onConnected);
    connect(nm_, &NetworkManager::disconnected, this, &ConnectionDialog::onDisconnected);
    connect(nm_, &NetworkManager::errorOccurred, this, &ConnectionDialog::onErrorOccurred);
    connect(nm_, &NetworkManager::messageReceived, this, &ConnectionDialog::onMessageReceived);
}

void ConnectionDialog::loadSettings() {
    QSettings settings;
    host_edit_->setText(settings.value("lastHost", "127.0.0.1").toString());
    port_edit_->setText(settings.value("lastPort", 9000).toString());
    username_edit_->setText(settings.value("lastUser", "").toString());
}

void ConnectionDialog::persistSettings() {
    QSettings settings;
    settings.setValue("lastHost", host());
    settings.setValue("lastPort", port());
    settings.setValue("lastUser", username());
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
    nm_->requestConnect(host(), port());
}

void ConnectionDialog::onConnected() {
    if (pending_action_.isEmpty()) return;

    status_label_->setText(tr("Authenticating…"));
    nm_->send(encode_auth_request(pending_action_, username(), password_edit_->text()));
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

    if (parse_envelope(payload).type != server::MessageType::AuthResponse) return;

    auto auth = parse_auth_response(payload);
    if (!auth) {
        reportFailure(tr("Malformed auth_response"));
        return;
    }
    if (!auth->success) {
        const auto code = QString::fromStdString(auth->error);
        qCWarning(logAuth) << "auth failed code=" << code;
        reportFailure(humanAuthError(code));
        return;
    }

    qCInfo(logAuth) << "auth success user=" << username() << "userId=" << auth->userId;
    nm_->setIdentity(auth->userId, username());
    persistSettings();
    pending_action_.clear();
    accept();
}

QString ConnectionDialog::humanAuthError(const QString& code) const {
    if (code == QStringLiteral("invalid_credentials")) return tr("Invalid username or password.");
    if (code == QStringLiteral("username_taken"))      return tr("Username is already taken.");
    return tr("Auth error: %1").arg(code);
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
    nm_->requestDisconnect();
}

}
