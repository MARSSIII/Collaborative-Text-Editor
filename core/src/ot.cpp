#include "collab/ot.h"
#include <algorithm>

namespace collab {

static std::pair<Operation, Operation>
transform_insert_insert(const Operation& a, const Operation& b) {
    Operation a_prime = a;
    Operation b_prime = b;

    if (a.position < b.position) {
        b_prime.position = b.position + a.length;
    } else if (a.position > b.position) {
        a_prime.position = a.position + b.length;
    } else {
        if (a.userId < b.userId) {
            b_prime.position = b.position + a.length;
        } else if (a.userId > b.userId) {
            a_prime.position = a.position + b.length;
        }
    }

    return {a_prime, b_prime};
}

static std::pair<Operation, Operation>
transform_insert_delete(const Operation& ins, const Operation& del) {
    Operation ins_prime = ins;
    Operation del_prime = del;

    uint32_t pos_ins = ins.position;
    uint32_t pos_del = del.position;

    if (pos_ins <= pos_del) {
        del_prime.position = pos_del + ins.length;
    } else if (pos_ins >= pos_del + del.length) {
        ins_prime.position = pos_ins - del.length;
    } else {
        ins_prime.position = pos_del;
        ins_prime.text     = "";
        ins_prime.length   = 0;

        del_prime.position = pos_del;
        del_prime.length   = del.length + ins.length;
    }

    return {ins_prime, del_prime};
}

static std::pair<Operation, Operation>
transform_delete_delete(const Operation& a, const Operation& b) {
    Operation a_prime = a;
    Operation b_prime = b;

    uint32_t end_a = a.position + a.length;
    uint32_t end_b = b.position + b.length;

    int32_t overlap = static_cast<int32_t>(
        std::min(end_a, end_b)) - static_cast<int32_t>(std::max(a.position, b.position));
    if (overlap < 0) {
        overlap = 0;
    }

    if (overlap == 0) {
        if (a.position < b.position) {
            b_prime.position = b.position - a.length;
        } else if (a.position > b.position) {
            a_prime.position = a.position - b.length;
        }
    } else {
        uint32_t min_pos = std::min(a.position, b.position);

        a_prime.position = min_pos;
        a_prime.length   = a.length - static_cast<uint32_t>(overlap);

        b_prime.position = min_pos;
        b_prime.length   = b.length - static_cast<uint32_t>(overlap);
    }

    return {a_prime, b_prime};
}

std::pair<Operation, Operation> transform(const Operation& a, const Operation& b) {
    using Type = Operation::Type;

    if (a.type == Type::Insert && b.type == Type::Insert) {
        return transform_insert_insert(a, b);
    }

    if (a.type == Type::Insert && b.type == Type::Delete) {
        return transform_insert_delete(a, b);
    }

    if (a.type == Type::Delete && b.type == Type::Insert) {
        auto [ins_prime, del_prime] = transform_insert_delete(b, a);
        return {del_prime, ins_prime};
    }

    return transform_delete_delete(a, b);
}

void apply(std::string& document, const Operation& op) {
    if (op.is_noop()) {
        return;
    }

    if (op.type == Operation::Type::Insert) {
        uint32_t pos = std::min(op.position, static_cast<uint32_t>(document.size()));
        document.insert(pos, op.text);
    } else {
        uint32_t pos = std::min(op.position, static_cast<uint32_t>(document.size()));
        uint32_t actual_len = std::min(op.length,
                                       static_cast<uint32_t>(document.size() - pos));
        document.erase(pos, actual_len);
    }
}

}
