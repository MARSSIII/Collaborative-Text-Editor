#include "server_fixture.h"
#include "test_client.h"

#include <gtest/gtest.h>

#include <chrono>
#include <thread>

using namespace test;
using namespace std::chrono_literals;

TEST_F(ServerFixture, AuthRegisterAndLogin) {
    {
        TestClient c(port());
        auto uid = c.register_and_login("alice", "pw123");
        EXPECT_GT(uid, 0u);
    }
    TestClient c2(port());
    auto uid2 = c2.login("alice", "pw123");
    EXPECT_GT(uid2, 0u);

    TestClient c3(port());
    c3.send({
        {"type", "auth_request"},
        {"action", "login"},
        {"username", "alice"},
        {"password", "wrong"},
    });
    auto resp = c3.recv(1s);
    ASSERT_TRUE(resp.has_value());
    EXPECT_FALSE(resp->value("success", true));
}

TEST_F(ServerFixture, DocumentCreateAndList) {
    TestClient c(port());
    c.register_and_login("alice", "pw");
    auto doc_id = c.create_document("Draft");
    EXPECT_GT(doc_id, 0u);

    auto list = c.doc_list();
    ASSERT_EQ(list.value("type", ""), "doc_list_response");
    auto& docs = list.at("documents");
    ASSERT_TRUE(docs.is_array());
    bool found = false;
    for (auto& d : docs) {
        if (d.at("docId").get<uint32_t>() == doc_id) {
            EXPECT_EQ(d.at("title").get<std::string>(), "Draft");
            EXPECT_EQ(d.at("role").get<std::string>(), "owner");
            found = true;
        }
    }
    EXPECT_TRUE(found);
}

TEST_F(ServerFixture, JoinLeaveBroadcast) {
    TestClient alice(port());
    auto alice_id = alice.register_and_login("alice", "pw");
    auto doc_id = alice.create_document("Shared");

    TestClient bob(port());
    auto bob_id = bob.register_and_login("bob", "pw");

    alice.share_document(doc_id, "bob", "editor");

    auto alice_join = alice.join_document(doc_id);
    ASSERT_TRUE(alice_join.value("success", false));
    EXPECT_EQ(alice_join.at("role").get<std::string>(), "owner");

    auto bob_join = bob.join_document(doc_id);
    ASSERT_TRUE(bob_join.value("success", false));
    EXPECT_EQ(bob_join.at("role").get<std::string>(), "editor");

    auto user_joined = alice.recv_until_type("user_joined", 2s);
    ASSERT_FALSE(user_joined.is_null());
    EXPECT_EQ(user_joined.at("username").get<std::string>(), "bob");
    EXPECT_EQ(user_joined.at("userId").get<uint32_t>(), bob_id);

    bob.send_insert(doc_id, /*revision=*/0, /*pos=*/0, "HELLO");

    auto ack = bob.recv_until_type("operation_ack", 2s);
    ASSERT_FALSE(ack.is_null());
    EXPECT_EQ(ack.at("revision").get<uint32_t>(), 1u);

    auto broadcast = alice.recv_until_type("operation_broadcast", 2s);
    ASSERT_FALSE(broadcast.is_null());
    EXPECT_EQ(broadcast.at("userId").get<uint32_t>(), bob_id);
    EXPECT_EQ(broadcast.at("revision").get<uint32_t>(), 1u);

    bob.leave_document(doc_id);

    auto user_left = alice.recv_until_type("user_left", 2s);
    ASSERT_FALSE(user_left.is_null());
    EXPECT_EQ(user_left.at("username").get<std::string>(), "bob");
    EXPECT_EQ(user_left.at("userId").get<uint32_t>(), bob_id);
    (void)alice_id;
}

TEST_F(ServerFixture, ConcurrentOTConvergence) {
    TestClient alice(port());
    alice.register_and_login("alice", "pw");
    auto doc_id = alice.create_document("Race");

    TestClient bob(port());
    bob.register_and_login("bob", "pw");
    alice.share_document(doc_id, "bob", "editor");

    auto a_join = alice.join_document(doc_id);
    ASSERT_TRUE(a_join.value("success", false));
    auto b_join = bob.join_document(doc_id);
    ASSERT_TRUE(b_join.value("success", false));

    (void)alice.recv_until_type("user_joined", 2s);

    alice.send_insert(doc_id, 0, 0, "A");
    bob.send_insert(doc_id, 0, 0, "B");

    auto a_ack = alice.recv_until_type("operation_ack", 2s);
    auto b_ack = bob.recv_until_type("operation_ack", 2s);
    ASSERT_FALSE(a_ack.is_null());
    ASSERT_FALSE(b_ack.is_null());

    auto a_broadcast = alice.recv_until_type("operation_broadcast", 2s);
    auto b_broadcast = bob.recv_until_type("operation_broadcast", 2s);
    ASSERT_FALSE(a_broadcast.is_null());
    ASSERT_FALSE(b_broadcast.is_null());

    // Both sides eventually leave and rejoin to snapshot final content.
    alice.leave_document(doc_id);
    bob.leave_document(doc_id);

    std::this_thread::sleep_for(50ms);

    TestClient alice2(port());
    alice2.login("alice", "pw");
    auto final_a = alice2.join_document(doc_id);
    ASSERT_TRUE(final_a.value("success", false));
    auto content_a = final_a.at("content").get<std::string>();
    auto revision_a = final_a.at("revision").get<uint32_t>();

    TestClient bob2(port());
    bob2.login("bob", "pw");
    auto final_b = bob2.join_document(doc_id);
    ASSERT_TRUE(final_b.value("success", false));
    auto content_b = final_b.at("content").get<std::string>();
    auto revision_b = final_b.at("revision").get<uint32_t>();

    EXPECT_EQ(content_a, content_b) << "OT divergence: alice='" << content_a
                                    << "', bob='" << content_b << "'";
    EXPECT_EQ(revision_a, revision_b);
    EXPECT_EQ(content_a.size(), 2u) << "Expected both inserts applied";
    EXPECT_EQ(revision_a, 2u);
}

TEST_F(ServerFixture, ViewerCannotEdit) {
    TestClient alice(port());
    alice.register_and_login("alice", "pw");
    auto doc_id = alice.create_document("ReadOnly");

    TestClient bob(port());
    bob.register_and_login("bob", "pw");
    alice.share_document(doc_id, "bob", "viewer");

    auto b_join = bob.join_document(doc_id);
    ASSERT_TRUE(b_join.value("success", false));
    EXPECT_EQ(b_join.at("role").get<std::string>(), "viewer");

    bob.send_insert(doc_id, 0, 0, "X");

    auto err = bob.recv_until_type("error", 2s);
    ASSERT_FALSE(err.is_null());
    EXPECT_EQ(err.at("code").get<std::string>(), "access_denied");
}

TEST_F(ServerFixture, MalformedJsonReturnsError) {
    TestClient c(port());
    c.register_and_login("alice", "pw");

    c.send_raw_frame("not-a-json-object");

    auto err = c.recv_until_type("error", 1s);
    ASSERT_FALSE(err.is_null());
    EXPECT_EQ(err.at("code").get<std::string>(), "invalid_message");

    c.send_raw_frame("{\"no_type_field\": true}");
    auto err2 = c.recv_until_type("error", 1s);
    ASSERT_FALSE(err2.is_null());
    EXPECT_EQ(err2.at("code").get<std::string>(), "invalid_message");
}

TEST_F(ServerFixture, GracefulShutdownNotifiesClients) {
    TestClient alice(port());
    alice.register_and_login("alice", "pw");

    TestClient bob(port());
    bob.register_and_login("bob", "pw");

    shutdown_server_();

    auto msg_a = alice.recv_until_type("server_shutdown", 2s);
    auto msg_b = bob.recv_until_type("server_shutdown", 2s);
    EXPECT_FALSE(msg_a.is_null()) << "alice did not get server_shutdown";
    EXPECT_FALSE(msg_b.is_null()) << "bob did not get server_shutdown";
}
