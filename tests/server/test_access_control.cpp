#include <gtest/gtest.h>
#include "collab/access_control.h"

#include <algorithm>

using namespace collab;

enum class Who { Owner, Editor, Viewer, Stranger };

static const char* name_of(Who w) {
    switch (w) {
        case Who::Owner:    return "Owner";
        case Who::Editor:   return "Editor";
        case Who::Viewer:   return "Viewer";
        case Who::Stranger: return "Stranger";
    }
    return "Unknown";
}

class AccessControlTest : public ::testing::Test {
protected:
    AccessControl ac;
    uint32_t owner_id   = 1;
    uint32_t editor_id  = 2;
    uint32_t viewer_id  = 3;
    uint32_t stranger_id = 4;
    uint32_t doc_id = 100;

    uint32_t user_of(Who w) const {
        switch (w) {
            case Who::Owner:    return owner_id;
            case Who::Editor:   return editor_id;
            case Who::Viewer:   return viewer_id;
            case Who::Stranger: return stranger_id;
        }
        return 0;
    }

    void SetUp() override {
        ac.grant(doc_id, owner_id,  Role::Owner);
        ac.grant(doc_id, editor_id, Role::Editor);
        ac.grant(doc_id, viewer_id, Role::Viewer);
    }
};

struct PermissionCase {
    Who who;
    bool can_read;
    bool can_edit;
    bool can_share;
    bool can_delete;
};

class AccessControlPermMatrix
    : public AccessControlTest,
      public ::testing::WithParamInterface<PermissionCase> {};

TEST_P(AccessControlPermMatrix, RolePermissions) {
    const auto& p = GetParam();
    const uint32_t uid = user_of(p.who);

    EXPECT_EQ(ac.can_read  (doc_id, uid), p.can_read)   << name_of(p.who) << " read";
    EXPECT_EQ(ac.can_edit  (doc_id, uid), p.can_edit)   << name_of(p.who) << " edit";
    EXPECT_EQ(ac.can_share (doc_id, uid), p.can_share)  << name_of(p.who) << " share";
    EXPECT_EQ(ac.can_delete(doc_id, uid), p.can_delete) << name_of(p.who) << " delete";
}

INSTANTIATE_TEST_SUITE_P(
    PermissionMatrix,
    AccessControlPermMatrix,
    ::testing::Values(
        PermissionCase{Who::Owner,    true,  true,  true,  true},
        PermissionCase{Who::Editor,   true,  true,  false, false},
        PermissionCase{Who::Viewer,   true,  false, false, false},
        PermissionCase{Who::Stranger, false, false, false, false}
    ),
    [](const ::testing::TestParamInfo<PermissionCase>& info) {
        return name_of(info.param.who);
    });

struct TryRevokeCase {
    Who requester;
    Who target;
    bool expected_success;
    const char* label;
};

class AccessControlTryRevoke
    : public AccessControlTest,
      public ::testing::WithParamInterface<TryRevokeCase> {};

TEST_P(AccessControlTryRevoke, Attempt) {
    const auto& p = GetParam();
    bool result = ac.try_revoke(doc_id, user_of(p.requester), user_of(p.target));
    EXPECT_EQ(result, p.expected_success);
}

INSTANTIATE_TEST_SUITE_P(
    RevokePolicy,
    AccessControlTryRevoke,
    ::testing::Values(
        TryRevokeCase{Who::Owner,    Who::Owner,    false, "OwnerCannotRevokeSelf"},
        TryRevokeCase{Who::Owner,    Who::Editor,   true,  "OwnerCanRevokeEditor"},
        TryRevokeCase{Who::Owner,    Who::Stranger, false, "OwnerCannotRevokeNonMember"},
        TryRevokeCase{Who::Editor,   Who::Viewer,   false, "EditorCannotRevoke"},
        TryRevokeCase{Who::Viewer,   Who::Editor,   false, "ViewerCannotRevoke"},
        TryRevokeCase{Who::Stranger, Who::Editor,   false, "StrangerCannotRevoke"}
    ),
    [](const ::testing::TestParamInfo<TryRevokeCase>& info) {
        return info.param.label;
    });

TEST_F(AccessControlTest, RevokeAccess) {
    EXPECT_TRUE(ac.can_read(doc_id, editor_id));
    ac.revoke(doc_id, editor_id);
    EXPECT_FALSE(ac.can_read(doc_id, editor_id));
    EXPECT_FALSE(ac.can_edit(doc_id, editor_id));
}

TEST_F(AccessControlTest, ChangeRole) {
    EXPECT_FALSE(ac.can_edit(doc_id, viewer_id));
    ac.grant(doc_id, viewer_id, Role::Editor);
    EXPECT_TRUE(ac.can_edit(doc_id, viewer_id));
}

TEST_F(AccessControlTest, ListDocumentsForUser) {
    uint32_t doc_id_2 = 200;
    ac.grant(doc_id_2, editor_id, Role::Editor);

    auto docs = ac.list_documents_for_user(editor_id);
    EXPECT_EQ(docs.size(), 2u);

    std::sort(docs.begin(), docs.end());
    EXPECT_EQ(docs[0], doc_id);
    EXPECT_EQ(docs[1], doc_id_2);
}

TEST_F(AccessControlTest, RevokeAllForDocumentRemovesEveryone) {
    uint32_t other_doc = 200;
    ac.grant(other_doc, owner_id, Role::Owner);

    ac.revoke_all_for_document(doc_id);

    EXPECT_FALSE(ac.can_read(doc_id, owner_id));
    EXPECT_FALSE(ac.can_read(doc_id, editor_id));
    EXPECT_FALSE(ac.can_read(doc_id, viewer_id));
    EXPECT_TRUE(ac.can_read(other_doc, owner_id));

    auto docs = ac.list_documents_for_user(owner_id);
    ASSERT_EQ(docs.size(), 1u);
    EXPECT_EQ(docs[0], other_doc);
}
