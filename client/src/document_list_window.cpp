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
#include <QMetaObject>
#include <QPushButton>
#include <QStatusBar>
#include <QVBoxLayout>
#include <QWidget>

namespace collab_client {

namespace {

QWidget* make_card(const server::DocListEntry& e) {
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

}

DocumentListWindow::DocumentListWindow(NetworkManager* nm, QWidget* parent)
    : QMainWindow(parent), nm_(nm) {
    setWindowTitle(tr("Documents — %1").arg(nm_->username()));
    resize(720, 560);

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
    delete_btn_ = new QPushButton(tr("Delete"));
    delete_btn_->setObjectName("danger");
    share_btn_ = new QPushButton(tr("Share"));
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

    qCInfo(logDocs) << "document list opened for user=" << nm_->username();
    refresh();
}

void DocumentListWindow::onDisconnected(QString reason) {
    qCWarning(logDocs) << "disconnected reason=" << reason;
    QMessageBox::information(this, tr("Connection lost"),
                             tr("Lost connection to server: %1").arg(reason));
    close();
}

void DocumentListWindow::refresh() {
    status_->setText(tr("Refreshing…"));
    QMetaObject::invokeMethod(nm_, "sendFrame", Qt::QueuedConnection,
                              Q_ARG(QByteArray, encode_doc_list_request()));
}

void DocumentListWindow::onNewClicked() {
    bool ok = false;
    auto title = QInputDialog::getText(this, tr("New document"),
                                       tr("Title:"), QLineEdit::Normal,
                                       QString(), &ok).trimmed();
    if (!ok || title.isEmpty()) return;

    status_->setText(tr("Creating \"%1\"…").arg(title));
    QMetaObject::invokeMethod(nm_, "sendFrame", Qt::QueuedConnection,
                              Q_ARG(QByteArray, encode_doc_create_request(title)));
}

void DocumentListWindow::onOpenClicked() {
    auto* item = list_->currentItem();
    if (!item) return;
    auto docId = static_cast<uint32_t>(item->data(Qt::UserRole).toUInt());
    requestJoin(docId);
}

void DocumentListWindow::onDeleteClicked() {
    auto* item = list_->currentItem();
    if (!item) return;
    auto docId = static_cast<uint32_t>(item->data(Qt::UserRole).toUInt());

    if (QMessageBox::question(this, tr("Delete document"),
                              tr("Delete document #%1? This cannot be undone.")
                                  .arg(docId)) != QMessageBox::Yes) {
        return;
    }
    QMetaObject::invokeMethod(nm_, "sendFrame", Qt::QueuedConnection,
                              Q_ARG(QByteArray, encode_doc_delete_request(docId)));
}

void DocumentListWindow::onShareClicked() {
    auto* item = list_->currentItem();
    if (!item) {
        QMessageBox::information(this, tr("Share"),
                                 tr("Select a document first."));
        return;
    }
    auto docId = static_cast<uint32_t>(item->data(Qt::UserRole).toUInt());

    bool ok = false;
    auto target = QInputDialog::getText(this, tr("Share document"),
                                        tr("Grant editor access to username:"),
                                        QLineEdit::Normal, QString(), &ok).trimmed();
    if (!ok || target.isEmpty()) return;

    QMetaObject::invokeMethod(nm_, "sendFrame", Qt::QueuedConnection,
                              Q_ARG(QByteArray,
                                    encode_doc_share_request(docId, target, "editor")));
    status_->setText(tr("Shared document #%1 with %2").arg(docId).arg(target));
}

void DocumentListWindow::requestJoin(uint32_t docId) {
    if (pending_join_doc_id_ != 0) return;
    pending_join_doc_id_ = docId;
    qCInfo(logDocs) << "joining doc=" << docId;
    status_->setText(tr("Opening document #%1…").arg(docId));
    QMetaObject::invokeMethod(nm_, "sendFrame", Qt::QueuedConnection,
                              Q_ARG(QByteArray, encode_doc_join_request(docId)));
}

void DocumentListWindow::onMessageReceived(QByteArray payload) {
    auto env = parse_envelope(payload);
    switch (env.type) {
    case server::MessageType::DocListResponse: {
        if (auto msg = parse_doc_list_response(payload)) applyDocList(*msg);
        break;
    }
    case server::MessageType::DocCreateResponse: {
        auto msg = parse_doc_create_response(payload);
        if (!msg) break;
        if (!msg->success) {
            QMessageBox::warning(this, tr("Create failed"),
                                 tr("Could not create the document."));
            refresh();
            break;
        }
        refresh();
        requestJoin(msg->docId);
        break;
    }
    case server::MessageType::DocJoinResponse: {
        auto msg = parse_doc_join_response(payload);
        if (!msg) break;
        if (msg->docId != pending_join_doc_id_ && pending_join_doc_id_ != 0) {
            break;
        }
        pending_join_doc_id_ = 0;
        if (!msg->success) {
            QMessageBox::warning(this, tr("Join failed"),
                                 tr("Could not open document (%1).")
                                     .arg(QString::fromStdString(msg->error)));
            break;
        }
        emit documentJoined(*msg);
        break;
    }
    case server::MessageType::DocDeleteResponse:
    case server::MessageType::DocShareResponse:
    case server::MessageType::RoleChanged:
    case server::MessageType::DocDeleted:
    case server::MessageType::UserJoined:
    case server::MessageType::UserLeft: {
        refresh();
        break;
    }
    case server::MessageType::Error: {
        if (auto err = parse_error(payload)) {
            status_->setText(QString::fromStdString(err->message));
        }
        pending_join_doc_id_ = 0;
        break;
    }
    default:
        break;
    }
}

void DocumentListWindow::applyDocList(const server::DocListResponseMsg& msg) {
    list_->clear();
    for (const auto& entry : msg.documents) {
        auto* item = new QListWidgetItem;
        item->setData(Qt::UserRole, entry.docId);
        auto* card = make_card(entry);
        item->setSizeHint(card->sizeHint());
        list_->addItem(item);
        list_->setItemWidget(item, card);
    }
    status_->setText(msg.documents.empty()
                         ? tr("No documents yet. Create your first one.")
                         : tr("%1 document(s).").arg(msg.documents.size()));
}

}
