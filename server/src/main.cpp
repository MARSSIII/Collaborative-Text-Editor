#include "server/server_config.h"
#include "server/async_logger.h"
#include "server/tcp_server.h"
#include "server/message_handler.h"
#include "server/document_manager.h"
#include "server/cursor_aggregator.h"
#include "server/autosave.h"
#include "collab/auth_manager.h"
#include "collab/access_control.h"
#include "collab/thread_pool.h"

#include <boost/asio.hpp>
#include <filesystem>
#include <format>
#include <iostream>
#include <csignal>

namespace fs = std::filesystem;

static void shutdown_server(server::TcpServer& tcp_server,
                            server::DocumentManager& doc_manager,
                            server::CursorAggregator& cursor_agg,
                            server::AutosaveThread& autosave,
                            server::AsyncLogger& logger,
                            boost::asio::io_context& io_ctx) {
    logger.info("Initiating graceful shutdown...");

    // 1. Stop accepting new connections
    tcp_server.stop();

    // 2. Broadcast shutdown to all clients
    tcp_server.broadcast_all(server::serialize(
        server::ServerShutdownMsg{"Server is shutting down"}));

    // 3. Stop cursor aggregation
    cursor_agg.stop();

    // 4. Stop all document sessions
    for (auto docId : doc_manager.all_active_doc_ids()) {
        doc_manager.remove_session(docId);
    }

    // 5. Final save
    autosave.force_save_all();
    autosave.stop();

    // 6. Close all client connections
    for (auto& session : tcp_server.all_sessions()) {
        session->close();
    }

    // 7. Stop io_context
    io_ctx.stop();

    logger.info("Shutdown complete");
}

int main(int argc, char* argv[]) {
    // 1. Load config
    std::string config_path = argc > 1 ? argv[1] : "config.json";
    auto config = server::load_config(config_path);

    // 2. Create data directories
    fs::create_directories(config.data_dir + "/documents");
    fs::create_directories(config.data_dir + "/snapshots");
    fs::create_directories(config.data_dir + "/logs");

    // 3. Logger
    server::AsyncLogger logger(
        config.data_dir + "/logs/server.log",
        config.log_level == "debug" ? server::LogLevel::Debug :
        config.log_level == "warn"  ? server::LogLevel::Warn :
        config.log_level == "error" ? server::LogLevel::Error :
                                      server::LogLevel::Info,
        config.log_console);

    logger.info("Starting server...");
    logger.info(std::format("Port: {}, Thread pool: {}, Data dir: {}",
                            config.port, config.effective_thread_pool_size(),
                            config.data_dir));

    // 4. Business logic
    collab::AuthManager auth;
    collab::AccessControl access;

    // 5. Document manager
    server::DocumentManager doc_manager(access, config.data_dir, logger);

    // 6. Asio
    boost::asio::io_context io_ctx;

    // 7. Thread pool
    collab::ThreadPool thread_pool(config.effective_thread_pool_size());

    // 8. TCP server + message handler
    server::MessageHandler* handler_ptr = nullptr;

    server::TcpServer tcp_server(io_ctx, config.port,
        // on_message: dispatch to thread pool
        [&](std::shared_ptr<server::ClientSession> session,
            const std::string& payload) {
            thread_pool.submit([&handler = *handler_ptr, session, payload] {
                handler.handle_message(session, payload);
            });
        },
        // on_disconnect: cleanup
        [&](std::shared_ptr<server::ClientSession> session) {
            if (session->current_doc_id() > 0) {
                auto doc_session = doc_manager.get_session(
                    session->current_doc_id());
                if (doc_session) {
                    server::DocCommand cmd;
                    cmd.type = server::DocCommand::Type::Leave;
                    cmd.userId = session->user_id();
                    cmd.username = session->username();
                    doc_session->enqueue_command(std::move(cmd));
                }
                doc_manager.remove_cursor(
                    session->current_doc_id(), session->user_id());
            }
            tcp_server.remove_session(session->session_id());
            if (session->is_authenticated()) {
                logger.info(std::format("User '{}' disconnected",
                                        session->username()));
            }
        });

    server::MessageHandler handler(auth, access, doc_manager,
                                   tcp_server, logger);
    handler_ptr = &handler;

    // 9. Cursor aggregation
    server::CursorAggregator cursor_agg(
        doc_manager,
        std::chrono::milliseconds(config.cursor_broadcast_interval_ms),
        logger);

    // 10. Autosave
    server::AutosaveThread autosave(
        doc_manager, config.data_dir,
        std::chrono::seconds(config.autosave_interval_sec),
        config.snapshot_revision_threshold, logger);

    // 11. Signal handler
    boost::asio::signal_set signals(io_ctx, SIGINT, SIGTERM);
    signals.async_wait(
        [&](boost::system::error_code, int sig) {
            logger.info(std::format("Received signal {}", sig));
            shutdown_server(tcp_server, doc_manager, cursor_agg,
                           autosave, logger, io_ctx);
        });

    // 12. Start everything
    tcp_server.start();
    cursor_agg.start();
    autosave.start();

    std::cout << std::format("Server listening on port {}\n", config.port);
    logger.info(std::format("Server started on port {}", config.port));

    // 13. Run event loop (blocks until shutdown)
    io_ctx.run();

    logger.info("Server stopped");
    logger.shutdown();

    return 0;
}
