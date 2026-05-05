#include <gtest/gtest.h>
#include "collab/operation.h"
#include "collab/ot.h"

using namespace collab;

struct TransformCase {
    std::string name;
    Operation op_a;
    Operation op_b;
    Operation expected_a_prime;
    Operation expected_b_prime;
};

class OTTransformTest : public ::testing::TestWithParam<TransformCase> {};

TEST_P(OTTransformTest, TransformPair) {
    auto [a_prime, b_prime] = transform(GetParam().op_a, GetParam().op_b);
    EXPECT_EQ(a_prime, GetParam().expected_a_prime)
        << "a_prime mismatch for case: " << GetParam().name
        << "\n  got: " << a_prime.to_string()
        << "\n  expected: " << GetParam().expected_a_prime.to_string();
    EXPECT_EQ(b_prime, GetParam().expected_b_prime)
        << "b_prime mismatch for case: " << GetParam().name
        << "\n  got: " << b_prime.to_string()
        << "\n  expected: " << GetParam().expected_b_prime.to_string();
}

INSTANTIATE_TEST_SUITE_P(CoreCases, OTTransformTest, ::testing::Values(
    TransformCase{
        "InsertInsert_ABeforeB",
        make_insert(1, "X", 1, 0),
        make_insert(3, "Y", 2, 0),
        make_insert(1, "X", 1, 0),
        make_insert(4, "Y", 2, 0)
    },
    TransformCase{
        "InsertInsert_AAfterB",
        make_insert(3, "X", 1, 0),
        make_insert(1, "Y", 2, 0),
        make_insert(4, "X", 1, 0),
        make_insert(1, "Y", 2, 0)
    },
    TransformCase{
        "InsertInsert_SamePos_Tiebreak",
        make_insert(2, "X", 1, 0),
        make_insert(2, "Y", 2, 0),
        make_insert(2, "X", 1, 0),
        make_insert(3, "Y", 2, 0)
    },
    TransformCase{
        "InsertDelete_InsBeforeDel",
        make_insert(1, "X", 1, 0),
        make_delete(3, 2, "", 2, 0),
        make_insert(1, "X", 1, 0),
        make_delete(4, 2, "", 2, 0)
    },
    TransformCase{
        "InsertDelete_InsAfterDel",
        make_insert(5, "X", 1, 0),
        make_delete(1, 2, "", 2, 0),
        make_insert(3, "X", 1, 0),
        make_delete(1, 2, "", 2, 0)
    },
    TransformCase{
        "InsertDelete_InsInsideDel",
        make_insert(3, "X", 1, 0),
        make_delete(2, 4, "", 2, 0),
        make_insert(2, "", 1, 0),
        make_delete(2, 5, "", 2, 0)
    },
    TransformCase{
        "DeleteDelete_NoOverlap_ALeft",
        make_delete(0, 2, "", 1, 0),
        make_delete(5, 3, "", 2, 0),
        make_delete(0, 2, "", 1, 0),
        make_delete(3, 3, "", 2, 0)
    },
    TransformCase{
        "DeleteDelete_PartialOverlap",
        make_delete(2, 4, "", 1, 0),
        make_delete(4, 4, "", 2, 0),
        make_delete(2, 2, "", 1, 0),
        make_delete(2, 2, "", 2, 0)
    },
    TransformCase{
        "DeleteDelete_FullContainment",
        make_delete(1, 5, "", 1, 0),
        make_delete(2, 2, "", 2, 0),
        make_delete(1, 3, "", 1, 0),
        make_delete(1, 0, "", 2, 0)
    }
));

INSTANTIATE_TEST_SUITE_P(DeleteVsInsert, OTTransformTest, ::testing::Values(
    TransformCase{
        "DeleteInsert_DelBeforeIns",
        make_delete(1, 2, "", 1, 0),
        make_insert(5, "X", 2, 0),
        make_delete(1, 2, "", 1, 0),
        make_insert(3, "X", 2, 0)
    },
    TransformCase{
        "DeleteInsert_DelAfterIns",
        make_delete(5, 2, "", 1, 0),
        make_insert(1, "X", 2, 0),
        make_delete(6, 2, "", 1, 0),
        make_insert(1, "X", 2, 0)
    },
    TransformCase{
        "DeleteInsert_InsInsideDel",
        make_delete(2, 4, "", 1, 0),
        make_insert(3, "X", 2, 0),
        make_delete(2, 5, "", 1, 0),
        make_insert(2, "", 2, 0)
    }
));

TEST(OTTransformEdge, InsertEmptyStringIsNeutral) {
    auto [a_prime, b_prime] = transform(
        make_insert(0, "", 1, 0),
        make_insert(0, "X", 2, 0)
    );
    EXPECT_EQ(a_prime.position, 0u);
    EXPECT_EQ(a_prime.text, "");
    EXPECT_EQ(b_prime.position, 0u);
    EXPECT_EQ(b_prime.text, "X");
}

TEST(OTTransformEdge, DeleteZeroLengthAtInsertPos) {
    auto [a_prime, b_prime] = transform(
        make_delete(3, 0, "", 1, 0),
        make_insert(3, "X", 2, 0)
    );
    EXPECT_EQ(a_prime.position, 4u);
    EXPECT_EQ(a_prime.length, 0u);
    EXPECT_EQ(b_prime.position, 3u);
    EXPECT_EQ(b_prime.text, "X");
}

TEST(OTTransformEdge, MultiCharInsertShiftsByByteLength) {
    auto [a_prime, b_prime] = transform(
        make_insert(2, "HELLO", 1, 0),
        make_insert(5, "Y", 2, 0)
    );
    EXPECT_EQ(a_prime.position, 2u);
    EXPECT_EQ(a_prime.text, "HELLO");
    EXPECT_EQ(b_prime.position, 10u);
    EXPECT_EQ(b_prime.text, "Y");
}
