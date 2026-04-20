#include <gtest/gtest.h>
#include "collab/access_control.h"

#include <algorithm>

using namespace collab;

class AccessControlTest : public ::testing::Test {
protected:
    AccessControl ac;
    uint32_t owner_id   = 1;
    uint32_t editor_id  = 2;
    uint32_t viewer_id  = 3;
    uint32_t stranger_id = 4;
    uint32_t doc_id = 100;

    void SetUp() override {
        ac.grant(doc_id, owner_id,  Role::Owner);
        ac.grant(doc_id, editor_id, Role::Editor);
        ac.grant(doc_id, viewer_id, Role::Viewer);
    }
};

TEST_F(AccessControlTest, OwnerCanEdit) {
    EXPECT_TRUE(ac.can_edit(doc_id, owner_id));
}

TEST_F(AccessControlTest, EditorCanEdit) {
    EXPECT_TRUE(ac.can_edit(doc_id, editor_id));
}

TEST_F(AccessControlTest, ViewerCannotEdit) {
    EXPECT_FALSE(ac.can_edit(doc_id, viewer_id));
}

TEST_F(AccessControlTest, StrangerCannotEdit) {
    EXPECT_FALSE(ac.can_edit(doc_id, stranger_id));
}

TEST_F(AccessControlTest, OwnerCanShare) {
    EXPECT_TRUE(ac.can_share(doc_id, owner_id));
}

TEST_F(AccessControlTest, EditorCannotShare) {
    EXPECT_FALSE(ac.can_share(doc_id, editor_id));
}

TEST_F(AccessControlTest, ViewerCannotShare) {
    EXPECT_FALSE(ac.can_share(doc_id, viewer_id));
}

TEST_F(AccessControlTest, OwnerCanDelete) {
    EXPECT_TRUE(ac.can_delete(doc_id, owner_id));
}

TEST_F(AccessControlTest, EditorCannotDelete) {
    EXPECT_FALSE(ac.can_delete(doc_id, editor_id));
}

TEST_F(AccessControlTest, ViewerCannotDelete) {
    EXPECT_FALSE(ac.can_delete(doc_id, viewer_id));
}

TEST_F(AccessControlTest, AllRolesCanRead) {
    EXPECT_TRUE(ac.can_read(doc_id, owner_id));
    EXPECT_TRUE(ac.can_read(doc_id, editor_id));
    EXPECT_TRUE(ac.can_read(doc_id, viewer_id));
}

TEST_F(AccessControlTest, StrangerCannotRead) {
    EXPECT_FALSE(ac.can_read(doc_id, stranger_id));
}

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

TEST_F(AccessControlTest, CannotRevokeOwnerSelf) {
    bool result = ac.try_revoke(doc_id, owner_id, owner_id);
    EXPECT_FALSE(result);
    EXPECT_TRUE(ac.can_read(doc_id, owner_id));
    EXPECT_TRUE(ac.can_edit(doc_id, owner_id));
}

TEST_F(AccessControlTest, EditorCannotRevoke) {
    bool result = ac.try_revoke(doc_id, editor_id, viewer_id);
    EXPECT_FALSE(result);
    EXPECT_TRUE(ac.can_read(doc_id, viewer_id));
}

TEST_F(AccessControlTest, ViewerCannotRevoke) {
    bool result = ac.try_revoke(doc_id, viewer_id, editor_id);
    EXPECT_FALSE(result);
    EXPECT_TRUE(ac.can_edit(doc_id, editor_id));
}

TEST_F(AccessControlTest, StrangerCannotRevoke) {
    bool result = ac.try_revoke(doc_id, stranger_id, editor_id);
    EXPECT_FALSE(result);
    EXPECT_TRUE(ac.can_edit(doc_id, editor_id));
}

TEST_F(AccessControlTest, RevokeNonexistentTarget) {
    bool result = ac.try_revoke(doc_id, owner_id, stranger_id);
    EXPECT_FALSE(result);
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
