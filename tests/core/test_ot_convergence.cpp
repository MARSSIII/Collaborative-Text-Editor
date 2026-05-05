#include <gtest/gtest.h>
#include "collab/operation.h"
#include "collab/ot.h"

using namespace collab;

struct ConvergenceCase {
    std::string name;
    std::string initial;
    Operation op_a;
    Operation op_b;
};

class ConvergenceTest : public ::testing::TestWithParam<ConvergenceCase> {};

TEST_P(ConvergenceTest, BothPathsConverge) {
    auto& c = GetParam();
    auto [a_prime, b_prime] = transform(c.op_a, c.op_b);

    std::string path1 = c.initial;
    collab::apply(path1, c.op_a);
    collab::apply(path1, b_prime);

    std::string path2 = c.initial;
    collab::apply(path2, c.op_b);
    collab::apply(path2, a_prime);

    EXPECT_EQ(path1, path2) << "Divergence for case: " << c.name;
}

INSTANTIATE_TEST_SUITE_P(TwoUser, ConvergenceTest, ::testing::Values(
    ConvergenceCase{
        "ins_ins_same_pos",
        "HELLO",
        make_insert(2, "X", 1, 0),
        make_insert(2, "Y", 2, 0)
    },
    ConvergenceCase{
        "ins_ins_diff_pos",
        "HELLO",
        make_insert(1, "AB", 1, 0),
        make_insert(4, "CD", 2, 0)
    },
    ConvergenceCase{
        "ins_del_before",
        "HELLO",
        make_insert(1, "X", 1, 0),
        make_delete(3, 2, "LO", 2, 0)
    },
    ConvergenceCase{
        "ins_del_inside",
        "HELLO",
        make_insert(2, "X", 1, 0),
        make_delete(1, 3, "ELL", 2, 0)
    },
    ConvergenceCase{
        "ins_del_after",
        "HELLO",
        make_insert(5, "X", 1, 0),
        make_delete(0, 2, "HE", 2, 0)
    },
    ConvergenceCase{
        "del_del_no_overlap",
        "HELLO WORLD",
        make_delete(0, 3, "HEL", 1, 0),
        make_delete(6, 5, "WORLD", 2, 0)
    },
    ConvergenceCase{
        "del_del_partial_overlap",
        "HELLO WORLD",
        make_delete(2, 5, "LLO W", 1, 0),
        make_delete(4, 5, "O WOR", 2, 0)
    },
    ConvergenceCase{
        "del_del_full_overlap",
        "HELLO",
        make_delete(0, 5, "HELLO", 1, 0),
        make_delete(1, 3, "ELL", 2, 0)
    },
    ConvergenceCase{
        "empty_doc_two_inserts",
        "",
        make_insert(0, "A", 1, 0),
        make_insert(0, "B", 2, 0)
    },
    ConvergenceCase{
        "single_char_delete_vs_insert",
        "X",
        make_delete(0, 1, "X", 1, 0),
        make_insert(0, "Y", 2, 0)
    }
));

namespace {

std::string serialize_three(const std::string& initial,
                            const Operation& first,
                            const Operation& second,
                            const Operation& third) {
    std::string doc = initial;
    collab::apply(doc, first);

    auto [_a, second_t] = transform(first, second);
    collab::apply(doc, second_t);

    auto [_b, third_via_first] = transform(first, third);
    auto [_c, third_t]         = transform(second_t, third_via_first);
    collab::apply(doc, third_t);

    return doc;
}

}

TEST(ConvergenceThreeUsers, AllSixServerOrderingsConverge) {
    const std::string initial = "ABCDE";
    Operation op_a = make_insert(1, "X", 1, 0);
    Operation op_b = make_delete(3, 1, "D", 2, 0);
    Operation op_c = make_insert(5, "Z", 3, 0);

    std::string r_abc = serialize_three(initial, op_a, op_b, op_c);
    std::string r_acb = serialize_three(initial, op_a, op_c, op_b);
    std::string r_bac = serialize_three(initial, op_b, op_a, op_c);
    std::string r_bca = serialize_three(initial, op_b, op_c, op_a);
    std::string r_cab = serialize_three(initial, op_c, op_a, op_b);
    std::string r_cba = serialize_three(initial, op_c, op_b, op_a);

    EXPECT_EQ(r_abc, r_acb);
    EXPECT_EQ(r_acb, r_bac);
    EXPECT_EQ(r_bac, r_bca);
    EXPECT_EQ(r_bca, r_cab);
    EXPECT_EQ(r_cab, r_cba);
}
