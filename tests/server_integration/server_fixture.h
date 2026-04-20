#pragma once

#include "collab/auth_manager.h"
#include "collab/access_control.h"
#include "collab/thread_pool.h"
#include "server/async_logger.h"
#include "server/log_sink.h"
#include "server/autosave.h"
#include "server/cursor_aggregator.h"
#include "server/document_manager.h"
#include "server/message_handler.h"
#include "server/tcp_server.h"
#include "collab_protocol/protocol.h"

#include <boost/asio.hpp>
#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <filesystem>
#include <memory>
#include <string>
#include <thread>

namespace test {

class ServerFixture : public ::testing::Test {
protected:
    void SetUp() override {
        auto test_info = ::testing::UnitTest::GetInstance()->current_test_info();
        data_dir_ = std::filesystem::temp_directory_path() /
            (std::string("collab_test_") + test_info->test_suite_name() + "_" +
             test_info->name() + "_" +
             std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        std::filesystem::create_directories(data_dir_ / "documents");
        std::filesystem::create_directories(data_dir_ / "snapshots");
        std::filesystem::create_directories(data_dir_ / "logs");

        std::vector<std::unique_ptr<server::ILogSink>> sinks;
        sinks.push_back(std::make_unique<server::FileSink>(
            (data_dir_ / "logs" / "server.log").string()));
        logger_ = std::make_unique<server::AsyncLogger>(
            std::move(sinks), server::LogLevel::Warn);

        auth_ = std::make_unique<collab::AuthManager>();
        access_ = std::make_unique<collab::AccessControl>();

        doc_manager_ = std::make_unique<server::DocumentManager>(
            *access_, data_dir_.string(), *logger_);

        io_ctx_ = std::make_unique<boost::asio::io_context>();
        work_guard_ = std::make_unique<
            boost::asio::executor_work_guard<boost::asio::io_context::executor_type>>(
            boost::asio::make_work_guard(*io_ctx_));

        thread_pool_ = std::make_unique<collab::ThreadPool>(4);

        tcp_server_ = std::make_unique<server::TcpServer>(
            *io_ctx_, /*port=*/0,
            [this](std::shared_ptr<server::ClientSession> session,
                   const std::string& payload) {
                thread_pool_->submit([this, session, payload] {
                    handler_->handle_message(session, payload);
                });
            },
            [this](std::shared_ptr<server::ClientSession> session) {
                if (session->current_doc_id() > 0) {
                    auto doc_session =
                        doc_manager_->get_session(session->current_doc_id());
                    if (doc_session) {
                        server::DocCommand cmd;
                        cmd.type = server::DocCommand::Type::Leave;
                        cmd.userId = session->user_id();
                        cmd.username = session->username();
                        doc_session->enqueue_command(std::move(cmd));
                    }
                    doc_manager_->remove_cursor(session->current_doc_id(),
                                                session->user_id());
                }
                tcp_server_->remove_session(session->session_id());
            });

        handler_ = std::make_unique<server::MessageHandler>(
            *auth_, *access_, *doc_manager_, *tcp_server_, *logger_);

        cursor_agg_ = std::make_unique<server::CursorAggregator>(
            *doc_manager_, std::chrono::milliseconds(50), *logger_);

        autosave_ = std::make_unique<server::AutosaveThread>(
            *doc_manager_, data_dir_.string(),
            std::chrono::seconds(3600),  // effectively disabled during tests
            100, *logger_);

        tcp_server_->start();
        cursor_agg_->start();
        autosave_->start();

        io_thread_ = std::thread([this] { io_ctx_->run(); });

        // Tiny yield so async_accept is primed before tests connect.
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }

    void TearDown() override {
        if (!shutdown_fired_) {
            shutdown_server_();
        }
        if (io_thread_.joinable()) {
            work_guard_.reset();
            io_ctx_->stop();
            io_thread_.join();
        }

        // Release in reverse order of construction.
        autosave_.reset();
        cursor_agg_.reset();
        handler_.reset();
        tcp_server_.reset();
        thread_pool_.reset();
        io_ctx_.reset();
        doc_manager_.reset();
        access_.reset();
        auth_.reset();
        logger_.reset();

        std::error_code ec;
        std::filesystem::remove_all(data_dir_, ec);
    }

    uint16_t port() const { return tcp_server_->local_port(); }

    // Triggers graceful shutdown (mirrors main.cpp::shutdown_server).
    // Tests that verify shutdown behavior should call this explicitly.
    void shutdown_server_() {
        shutdown_fired_ = true;
        tcp_server_->stop();
        tcp_server_->broadcast_all(server::serialize(
            server::ServerShutdownMsg{"Server is shutting down"}));
        cursor_agg_->stop();
        for (auto docId : doc_manager_->all_active_doc_ids()) {
            doc_manager_->remove_session(docId);
        }
        autosave_->force_save_all();
        autosave_->stop();
        for (auto& s : tcp_server_->all_sessions()) s->close();
    }

    std::filesystem::path data_dir_;
    std::unique_ptr<server::AsyncLogger> logger_;
    std::unique_ptr<collab::AuthManager> auth_;
    std::unique_ptr<collab::AccessControl> access_;
    std::unique_ptr<server::DocumentManager> doc_manager_;
    std::unique_ptr<boost::asio::io_context> io_ctx_;
    std::unique_ptr<boost::asio::executor_work_guard<
        boost::asio::io_context::executor_type>> work_guard_;
    std::unique_ptr<collab::ThreadPool> thread_pool_;
    std::unique_ptr<server::TcpServer> tcp_server_;
    std::unique_ptr<server::MessageHandler> handler_;
    std::unique_ptr<server::CursorAggregator> cursor_agg_;
    std::unique_ptr<server::AutosaveThread> autosave_;
    std::thread io_thread_;
    bool shutdown_fired_{false};
};

} // namespace test
