#include "client/document_list_window.h"

#include "client/logging.h"
#include "client/network_manager.h"
#include "client/protocol_codec.h"
#include "client/theme.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMessageBox>
#include <QPushButton>
#include <QStatusBar>
#include <QVBoxLayout>
#include <QWidget>

namespace collab_client {

namespace {

QWidget* makeDocCard(const server::DocListEntry& e) {
    auto* card = new QFrame;
    card->setObjectName("docCard");

    auto* title = new QLabel(QString::fromStdString(e.title));
    auto tf = title->font();
    tf.setPointSizeF(tf.pointSizeF() + 1.5);
    tf.setBold(true);
    title->setFont(tf);

    auto* id_label = new QLabel(QStringLiteral("#%1").arg(e.docId));
    id_label->setObjectName("muted");

    const QString role = QString::fromStdString(e.role);
    auto* role_badge = theme::makeBadge(role, theme::roleColor(role));
    auto* online_badge = theme::makeBadge(
        QObject::tr("online %1").arg(e.onlineCount), QColor("#2e7d32"));

    auto* top = new QHBoxLayout;
    top->setContentsMargins(0, 0, 0, 0);
    top->addWidget(title);
    top->addStretch();
    top->addWidget(id_label);

    auto* meta = new QHBoxLayout;
    meta->setContentsMargins(0, 0, 0, 0);
    meta->setSpacing(6);
    meta->addWidget(role_badge);
    meta->addWidget(online_badge);
    meta->addStretch();

    auto* v = new QVBoxLayout(card);
    v->setContentsMargins(12, 10, 12, 10);
    v->setSpacing(6);
    v->addLayout(top);
    v->addLayout(meta);

    return card;
}

QString promptForText(QWidget* parent, const QString& title, const QString& label) {
    bool ok = false;
    const auto text = QInputDialog::getText(parent, title, label, QLineEdit::Normal,
                                            QString(), &ok).trimmed();
    return ok ? text : QString();
}

}

DocumentListWindow::DocumentListWindow(NetworkManager* nm, QWidget* parent)
    : QMainWindow(parent), nm_(nm) {
    setWindowTitle(tr("Documents — %1").arg(nm_->username()));
    resize(720, 560);

    buildUi();
    wireSignals();

    qCInfo(logDocs) << "document list opened for user=" << nm_->username();
    refresh();
}

void DocumentListWindow::buildUi() {
    auto* central = new QWidget(this);
    setCentralWidget(central);

    auto* header = new QLabel(tr("Your documents"));
    header->setObjectName("h1");
    auto* subheader = new QLabel(tr("Signed in as %1").arg(nm_->username()));
    subheader->setObjectName("subtitle");

    list_ = new QListWidget;
    list_->setSpacing(0);
    list_->setSelectionMode(QAbstractItemView::SingleSelection);
    list_->setUniformItemSizes(false);

    new_btn_ = new QPushButton(tr("New document"));
    new_btn_->setObjectName("primary");
    open_btn_ = new QPushButton(tr("Open"));
    share_btn_ = new QPushButton(tr("Share"));
    delete_btn_ = new QPushButton(tr("Delete"));
    delete_btn_->setObjectName("danger");
    refresh_btn_ = new QPushButton(tr("Refresh"));

    status_ = new QLabel(tr("Loading…"));
    status_->setObjectName("muted");

    auto* buttons = new QHBoxLayout;
    buttons->setSpacing(8);
    buttons->addWidget(new_btn_);
    buttons->addWidget(open_btn_);
    buttons->addWidget(share_btn_);
    buttons->addWidget(delete_btn_);
    buttons->addStretch();
    buttons->addWidget(refresh_btn_);

    auto* root = new QVBoxLayout(central);
    root->setContentsMargins(20, 18, 20, 16);
    root->setSpacing(10);
    root->addWidget(header);
    root->addWidget(subheader);
    root->addSpacing(4);
    root->addLayout(buttons);
    root->addWidget(list_, 1);
    root->addWidget(status_);
}

void DocumentListWindow::wireSignals() {
    connect(new_btn_, &QPushButton::clicked, this, &DocumentListWindow::onNewClicked);
    connect(open_btn_, &QPushButton::clicked, this, &DocumentListWindow::onOpenClicked);
    connect(delete_btn_, &QPushButton::clicked, this, &DocumentListWindow::onDeleteClicked);
    connect(share_btn_, &QPushButton::clicked, this, &DocumentListWindow::onShareClicked);
    connect(refresh_btn_, &QPushButton::clicked, this, &DocumentListWindow::refresh);
    connect(list_, &QListWidget::itemDoubleClicked, this,
            [this](QListWidgetItem*) { onOpenClicked(); });

    connect(nm_, &NetworkManager::messageReceived,
            this, &DocumentListWindow::onMessageReceived);
    connect(nm_, &NetworkManager::disconnected,
            this, &DocumentListWindow::onDisconnected);
}

std::optional<uint32_t> DocumentListWindow::selectedDocId() const {
    auto* item = list_->currentItem();
    if (!item) return std::nullopt;
    return static_cast<uint32_t>(item->data(Qt::UserRole).toUInt());
}

void DocumentListWindow::onDisconnected(QString reason) {
    qCWarning(logDocs) << "disconnected reason=" << reason;
    QMessageBox::information(this, tr("Connection lost"),
                             tr("Lost connection to server: %1").arg(reason));
    close();
}

void DocumentListWindow::refresh() {
    status_->setText(tr("Refreshing…"));
    nm_->send(encode_doc_list_request());
}

void DocumentListWindow::onNewClicked() {
    const auto title = promptForText(this, tr("New document"), tr("Title:"));
    if (title.isEmpty()) return;
    status_->setText(tr("Creating \"%1\"…").arg(title));
    nm_->send(encode_doc_create_request(title));
}

void DocumentListWindow::onOpenClicked() {
    if (auto id = selectedDocId()) requestJoin(*id);
}

void DocumentListWindow::onDeleteClicked() {
    auto id = selectedDocId();
    if (!id) return;

    if (QMessageBox::question(this, tr("Delete document"),
                              tr("Delete document #%1? This cannot be undone.")
                                  .arg(*id)) != QMessageBox::Yes) {
        return;
    }
    nm_->send(encode_doc_delete_request(*id));
}

void DocumentListWindow::onShareClicked() {
    auto id = selectedDocId();
    if (!id) {
        QMessageBox::information(this, tr("Share"), tr("Select a document first."));
        return;
    }
    const auto target = promptForText(this, tr("Share document"),
                                      tr("Grant editor access to username:"));
    if (target.isEmpty()) return;

    nm_->send(encode_doc_share_request(*id, target, "editor"));
    status_->setText(tr("Shared document #%1 with %2").arg(*id).arg(target));
}

void DocumentListWindow::requestJoin(uint32_t docId) {
    if (pending_join_doc_id_ != 0) return;
    pending_join_doc_id_ = docId;
    qCInfo(logDocs) << "joining doc=" << docId;
    status_->setText(tr("Opening document #%1…").arg(docId));
    nm_->send(encode_doc_join_request(docId));
}

void DocumentListWindow::onMessageReceived(QByteArray payload) {
    switch (parse_envelope(payload).type) {
    case server::MessageType::DocListResponse:   handleDocListResponse(payload);   break;
    case server::MessageType::DocCreateResponse: handleDocCreateResponse(payload); break;
    case server::MessageType::DocJoinResponse:   handleDocJoinResponse(payload);   break;
    case server::MessageType::Error:             handleErrorMessage(payload);      break;

    case server::MessageType::DocDeleteResponse:
    case server::MessageType::DocShareResponse:
    case server::MessageType::RoleChanged:
    case server::MessageType::DocDeleted:
    case server::MessageType::UserJoined:
    case server::MessageType::UserLeft:
        refresh();
        break;

    default:
        break;
    }
}

void DocumentListWindow::handleDocListResponse(const QByteArray& payload) {
    if (auto msg = parse_doc_list_response(payload)) applyDocList(*msg);
}

void DocumentListWindow::handleDocCreateResponse(const QByteArray& payload) {
    auto msg = parse_doc_create_response(payload);
    if (!msg) return;
    if (!msg->success) {
        QMessageBox::warning(this, tr("Create failed"),
                             tr("Could not create the document."));
        refresh();
        return;
    }
    refresh();
    requestJoin(msg->docId);
}

void DocumentListWindow::handleDocJoinResponse(const QByteArray& payload) {
    auto msg = parse_doc_join_response(payload);
    if (!msg) return;
    if (pending_join_doc_id_ != 0 && msg->docId != pending_join_doc_id_) return;

    pending_join_doc_id_ = 0;
    if (!msg->success) {
        QMessageBox::warning(this, tr("Join failed"),
                             tr("Could not open document (%1).")
                                 .arg(QString::fromStdString(msg->error)));
        return;
    }
    emit documentJoined(*msg);
}

void DocumentListWindow::handleErrorMessage(const QByteArray& payload) {
    if (auto err = parse_error(payload)) {
        status_->setText(QString::fromStdString(err->message));
    }
    pending_join_doc_id_ = 0;
}

void DocumentListWindow::applyDocList(const server::DocListResponseMsg& msg) {
    list_->clear();
    for (const auto& entry : msg.documents) {
        auto* item = new QListWidgetItem;
        item->setData(Qt::UserRole, entry.docId);

        auto* card = makeDocCard(entry);
        item->setSizeHint(card->sizeHint());
        list_->addItem(item);
        list_->setItemWidget(item, card);
    }
    status_->setText(msg.documents.empty()
                         ? tr("No documents yet. Create your first one.")
                         : tr("%1 document(s).").arg(msg.documents.size()));
}

}
