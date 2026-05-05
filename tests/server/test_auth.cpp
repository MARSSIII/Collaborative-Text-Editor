#include <gtest/gtest.h>
#include "collab/auth_manager.h"

#include <optional>
#include <set>
#include <string>
#include <utility>

using namespace collab;

namespace {
const std::string ALICE = "alice";
const std::string BOB = "bob";
const std::string CHARLIE = "charlie";
const std::string PASSWORD = "password123";
}

class AuthTest : public ::testing::Test {
protected:
    AuthManager auth;
};

TEST_F(AuthTest, RegisterNewUser) {
    auto result = auth.register_user(ALICE, PASSWORD);
    EXPECT_TRUE(result.success);
    EXPECT_GT(result.userId, 0u);
    EXPECT_TRUE(result.error.empty());
}

TEST_F(AuthTest, LoginCorrectCredentials) {
    auto reg = auth.register_user(ALICE, PASSWORD);
    ASSERT_TRUE(reg.success);

    auto login = auth.login(ALICE, PASSWORD);
    EXPECT_TRUE(login.success);
    EXPECT_EQ(login.userId, reg.userId);
}

TEST_F(AuthTest, PasswordIsHashed) {
    auth.register_user(ALICE, PASSWORD);
    std::string stored_hash = auth.get_stored_hash(ALICE);
    EXPECT_FALSE(stored_hash.empty());
    EXPECT_NE(stored_hash, PASSWORD);
}

TEST_F(AuthTest, SamePasswordDifferentSalt) {
    const std::string shared_pw = "same_password";
    auth.register_user(ALICE, shared_pw);
    auth.register_user(BOB, shared_pw);

    std::string hash_alice = auth.get_stored_hash(ALICE);
    std::string hash_bob = auth.get_stored_hash(BOB);

    EXPECT_FALSE(hash_alice.empty());
    EXPECT_FALSE(hash_bob.empty());
    EXPECT_NE(hash_alice, hash_bob);
}

TEST_F(AuthTest, UniqueUserIds) {
    auto r1 = auth.register_user(ALICE,   "pass1");
    auto r2 = auth.register_user(BOB,     "pass2");
    auto r3 = auth.register_user(CHARLIE, "pass3");

    ASSERT_TRUE(r1.success);
    ASSERT_TRUE(r2.success);
    ASSERT_TRUE(r3.success);

    std::set<uint32_t> ids{r1.userId, r2.userId, r3.userId};
    EXPECT_EQ(ids.size(), 3u);
    EXPECT_FALSE(ids.contains(0u));
}

struct FailureCase {
    std::optional<std::pair<std::string, std::string>> precondition;
    std::string username;
    std::string password;
    std::string expected_error;
    const char* label;
};

class AuthFailureTest : public AuthTest,
                        public ::testing::WithParamInterface<FailureCase> {};

TEST_P(AuthFailureTest, RegisterRejects) {
    const auto& p = GetParam();
    if (p.precondition) {
        auto pre = auth.register_user(p.precondition->first, p.precondition->second);
        ASSERT_TRUE(pre.success);
    }

    auto result = auth.register_user(p.username, p.password);
    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.error, p.expected_error);
}

INSTANTIATE_TEST_SUITE_P(
    RegisterRejections,
    AuthFailureTest,
    ::testing::Values(
        FailureCase{std::nullopt, "", PASSWORD, "empty_username", "EmptyUsername"},
        FailureCase{std::nullopt, ALICE, "", "empty_password", "EmptyPassword"},
        FailureCase{std::make_pair(ALICE, PASSWORD), ALICE, "other", "duplicate_username", "DuplicateUsername"}
    ),
    [](const ::testing::TestParamInfo<FailureCase>& info) { return info.param.label; });

class AuthLoginFailureTest : public AuthTest,
                             public ::testing::WithParamInterface<FailureCase> {};

TEST_P(AuthLoginFailureTest, LoginRejects) {
    const auto& p = GetParam();
    if (p.precondition) {
        auto pre = auth.register_user(p.precondition->first, p.precondition->second);
        ASSERT_TRUE(pre.success);
    }

    auto result = auth.login(p.username, p.password);
    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.error, p.expected_error);
}

INSTANTIATE_TEST_SUITE_P(
    LoginRejections,
    AuthLoginFailureTest,
    ::testing::Values(
        FailureCase{std::nullopt, "nobody", "any", "invalid_credentials", "NonexistentUser"},
        FailureCase{std::make_pair(ALICE, PASSWORD), ALICE, "wrong", "invalid_credentials", "WrongPassword"}
    ),
    [](const ::testing::TestParamInfo<FailureCase>& info) { return info.param.label; });
