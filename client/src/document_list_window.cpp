#include "client/document_list_window.h"

#include "client/network_manager.h"
#include "client/protocol_codec.h"

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

QString format_entry(const server::DocListEntry& e) {
    return QStringLiteral("[%1] %2  —  %3  (online: %4)")
        .arg(e.docId)
        .arg(QString::fromStdString(e.title))
        .arg(QString::fromStdString(e.role))
        .arg(e.onlineCount);
}

} // namespace

DocumentListWindow::DocumentListWindow(NetworkManager* nm, QWidget* parent)
    : QMainWindow(parent), nm_(nm) {
    setWindowTitle(tr("Documents — %1").arg(nm_->username()));
    resize(640, 480);

    auto* central = new QWidget(this);
    setCentralWidget(central);

    list_ = new QListWidget;
    new_btn_ = new QPushButton(tr("New"));
    open_btn_ = new QPushButton(tr("Open"));
    delete_btn_ = new QPushButton(tr("Delete"));
    refresh_btn_ = new QPushButton(tr("Refresh"));
    status_ = new QLabel(tr("Loading…"));

    auto* buttons = new QHBoxLayout;
    buttons->addWidget(new_btn_);
    buttons->addWidget(open_btn_);
    buttons->addWidget(delete_btn_);
    buttons->addWidget(refresh_btn_);
    buttons->addStretch();

    auto* root = new QVBoxLayout(central);
    root->addWidget(list_);
    root->addLayout(buttons);
    root->addWidget(status_);

    connect(new_btn_, &QPushButton::clicked, this, &DocumentListWindow::onNewClicked);
    connect(open_btn_, &QPushButton::clicked, this, &DocumentListWindow::onOpenClicked);
    connect(delete_btn_, &QPushButton::clicked, this, &DocumentListWindow::onDeleteClicked);
    connect(refresh_btn_, &QPushButton::clicked, this, &DocumentListWindow::refresh);
    connect(list_, &QListWidget::itemDoubleClicked, this,
            [this](QListWidgetItem*) { onOpenClicked(); });

    connect(nm_, &NetworkManager::messageReceived,
            this, &DocumentListWindow::onMessageReceived);

    refresh();
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

void DocumentListWindow::requestJoin(uint32_t docId) {
    if (pending_join_doc_id_ != 0) return;
    pending_join_doc_id_ = docId;
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
    case server::MessageType::DocDeleteResponse: {
        refresh();
        break;
    }
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
        auto* item = new QListWidgetItem(format_entry(entry));
        item->setData(Qt::UserRole, entry.docId);
        list_->addItem(item);
    }
    status_->setText(tr("%1 document(s).").arg(msg.documents.size()));
}

} // namespace collab_client
