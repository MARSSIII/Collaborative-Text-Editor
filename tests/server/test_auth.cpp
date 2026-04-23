#include <gtest/gtest.h>
#include "collab/auth_manager.h"

using namespace collab;

class AuthTest : public ::testing::Test {
protected:
    AuthManager auth;
};

TEST_F(AuthTest, RegisterNewUser) {
    auto result = auth.register_user("alice", "password123");
    EXPECT_TRUE(result.success);
    EXPECT_GT(result.userId, 0u);
    EXPECT_TRUE(result.error.empty());
}

TEST_F(AuthTest, RegisterDuplicateUsername) {
    auth.register_user("alice", "password123");
    auto result = auth.register_user("alice", "other_password");
    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.error, "duplicate_username");
}

TEST_F(AuthTest, LoginCorrectCredentials) {
    auto reg = auth.register_user("alice", "password123");
    EXPECT_TRUE(reg.success);

    auto login = auth.login("alice", "password123");
    EXPECT_TRUE(login.success);
    EXPECT_EQ(login.userId, reg.userId);
}

TEST_F(AuthTest, LoginWrongPassword) {
    auth.register_user("alice", "password123");
    auto result = auth.login("alice", "wrong_password");
    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.error, "invalid_credentials");
}

TEST_F(AuthTest, LoginNonexistentUser) {
    auto result = auth.login("nobody", "password");
    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.error, "invalid_credentials");
}

TEST_F(AuthTest, PasswordIsHashed) {
    auth.register_user("alice", "password123");
    std::string stored_hash = auth.get_stored_hash("alice");
    EXPECT_FALSE(stored_hash.empty());
    EXPECT_NE(stored_hash, "password123");
}

TEST_F(AuthTest, SamePasswordDifferentSalt) {
    auth.register_user("alice", "same_password");
    auth.register_user("bob", "same_password");

    std::string hash_alice = auth.get_stored_hash("alice");
    std::string hash_bob = auth.get_stored_hash("bob");

    EXPECT_FALSE(hash_alice.empty());
    EXPECT_FALSE(hash_bob.empty());
    EXPECT_NE(hash_alice, hash_bob);
}

TEST_F(AuthTest, UniqueUserIds) {
    auto r1 = auth.register_user("alice", "pass1");
    auto r2 = auth.register_user("bob", "pass2");
    auto r3 = auth.register_user("charlie", "pass3");

    EXPECT_TRUE(r1.success);
    EXPECT_TRUE(r2.success);
    EXPECT_TRUE(r3.success);

    EXPECT_LT(r1.userId, r2.userId);
    EXPECT_LT(r2.userId, r3.userId);
}

TEST_F(AuthTest, RegisterEmptyUsernameAndPassword) {
    auto r1 = auth.register_user("", "password123");
    EXPECT_FALSE(r1.success);
    EXPECT_EQ(r1.error, "empty_username");

    auto r2 = auth.register_user("alice", "");
    EXPECT_FALSE(r2.success);
    EXPECT_EQ(r2.error, "empty_password");
}
