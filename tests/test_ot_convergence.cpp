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

TEST(ConvergenceThreeUsers, AllConverge) {
    std::string initial = "ABCDE";
    Operation op_a = make_insert(1, "X", 1, 0);
    Operation op_b = make_delete(3, 1, "D", 2, 0);
    Operation op_c = make_insert(5, "Z", 3, 0);

    auto [a1, b1] = transform(op_a, op_b);
    auto [a2, c1] = transform(op_a, op_c);
    auto [b2, c2] = transform(b1, c1);

    std::string server = initial;
    collab::apply(server, op_a);
    collab::apply(server, b1);
    collab::apply(server, c2);

    std::string client_a = initial;
    collab::apply(client_a, op_a);
    collab::apply(client_a, b1);
    collab::apply(client_a, c2);

    EXPECT_EQ(server, client_a);
}
