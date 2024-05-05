// SPDX-FileCopyrightText: (c) 2024 Inseo Oh <dhdlstjtr@gmail.com>
//
// SPDX-License-Identifier: BSD-2-Clause
#pragma once

namespace Kernel {

enum class MemoryOrder {
    RELEAXED = __ATOMIC_RELAXED,
    CONSUME = __ATOMIC_CONSUME,
    ACQUIRE = __ATOMIC_ACQUIRE,
    RELEASE = __ATOMIC_RELEASE,
    ACQ_REL = __ATOMIC_ACQ_REL,
    SEQ_CST = __ATOMIC_SEQ_CST
};

template<typename T> class Atomic {
public:
    constexpr Atomic(T value) : m_value(value) {}

    T load(MemoryOrder order = MemoryOrder::SEQ_CST) {
        return __atomic_load_n(&m_value, static_cast<int>(order));
    }
    void store(T value, MemoryOrder order = MemoryOrder::SEQ_CST) {
        __atomic_store_n(&m_value, value, static_cast<int>(order));
    }
    T exchange(T value, MemoryOrder order = MemoryOrder::SEQ_CST) {
        return __atomic_exchange_n(&m_value, value, static_cast<int>(order));
    }
    T compare_exchange(T expected, T desired, MemoryOrder success_order = MemoryOrder::SEQ_CST, MemoryOrder fail_order = MemoryOrder::SEQ_CST) {
        return __atomic_exchange_n(&m_value, &expected, desired, false, success_order, fail_order);
    }
    T add_fetch(T value, MemoryOrder order = MemoryOrder::SEQ_CST) {
        return __atomic_add_fetch(&m_value, value, static_cast<int>(order));
    }
    T sub_fetch(T value, MemoryOrder order = MemoryOrder::SEQ_CST) {
        return __atomic_sub_fetch(&m_value, value, static_cast<int>(order));
    }
    T and_fetch(T value, MemoryOrder order = MemoryOrder::SEQ_CST) {
        return __atomic_and_fetch(&m_value, value, static_cast<int>(order));
    }
    T xor_fetch(T value, MemoryOrder order = MemoryOrder::SEQ_CST) {
        return __atomic_xor_fetch(&m_value, value, static_cast<int>(order));
    }
    T or_fetch(T value, MemoryOrder order = MemoryOrder::SEQ_CST) {
        return __atomic_or_fetch(&m_value, value, static_cast<int>(order));
    }
    T nand_fetch(T value, MemoryOrder order = MemoryOrder::SEQ_CST) {
        return __atomic_nand_fetch(&m_value, value, static_cast<int>(order));
    }
    T fetch_add(T value, MemoryOrder order = MemoryOrder::SEQ_CST) {
        return __atomic_fetch_add(&m_value, value, static_cast<int>(order));
    }
    T fetch_sub_fetch(T value, MemoryOrder order = MemoryOrder::SEQ_CST) {
        return __atomic_fetch_sub(&m_value, value, static_cast<int>(order));
    }
    T fetch_and(T value, MemoryOrder order = MemoryOrder::SEQ_CST) {
        return __atomic_fetch_and(&m_value, value, static_cast<int>(order));
    }
    T fetch_xor(T value, MemoryOrder order = MemoryOrder::SEQ_CST) {
        return __atomic_fetch_xor(&m_value, value, static_cast<int>(order));
    }
    T fetch_or(T value, MemoryOrder order = MemoryOrder::SEQ_CST) {
        return __atomic_fetch_or(&m_value, value, static_cast<int>(order));
    }
    T fetch_nand(T value, MemoryOrder order = MemoryOrder::SEQ_CST) {
        return __atomic_fetch(&m_value, value, static_cast<int>(order));
    }

private:
    T m_value;
};

}