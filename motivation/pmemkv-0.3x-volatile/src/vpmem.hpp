#pragma once

#define PMEMOBJ_P_HPP

namespace pmem {
namespace obj {

template <typename T>
class p {
    typedef p<T> this_type;

public:
    p(const T &_val) noexcept : val{_val} {  }
    p() = default;

    p & operator=(const p &rhs) {
        this_type(rhs).swap(*this);
        return *this;
    }

    template <typename Y, typename = typename std::enable_if<
    std::is_convertible<Y, T>::value>::type>
    p & operator=(const p<Y> &rhs) {
        this_type(rhs).swap(*this);
        return *this;
    }

    operator T() const noexcept {
        return this->val;
    }

    T & get_rw() {
        return this->val;
    }

    const T & get_ro() const noexcept {
        return this->val;
    }

    void swap(p &other) {
        std::swap(this->val, other.val);
    }

private:
    T val;
};

template <class T>
inline void swap(p<T> &a, p<T> &b) {
    a.swap(b);
}

} // namespace obj
} // namespace pmem

#include "libpmemobj++/detail/array_traits.hpp"
#include <libpmemobj++/detail/check_persistent_ptr_array.hpp>
#include <libpmemobj++/detail/common.hpp>
#include <libpmemobj++/detail/life.hpp>
#include <libpmemobj++/detail/pexceptions.hpp>
#include <libpmemobj/tx_base.h>

#include <new>
#include <utility>

namespace pmem {
namespace obj {

template <typename T, typename... Args>
typename detail::pp_if_not_array<T>::type
make_persistent(PMEMobjpool *pop, Args &&... args) {

    PMEMoid oid;
    int ret = pmemobj_alloc(pop, &oid, sizeof(T),
            detail::type_num<T>(), NULL, NULL);

    if (ret != 0)
        throw transaction_alloc_error("failed to allocate "
                "persistent memory object");

    persistent_ptr<T> ptr = oid;
    try {
        detail::create<T, Args...>(ptr.get(),
                std::forward<Args>(args)...);
    } catch (...) {
        pmemobj_free(&oid);
        throw;
    }

    return ptr;
}

template <typename T>
void
delete_persistent(typename detail::pp_if_not_array<T>::type ptr) {

    if (ptr == nullptr) return;

    detail::destroy<T>(*ptr);

    if (pmemobj_free(ptr.raw()) != 0)
        throw transaction_free_error("failed to delete "
                "persistent memory object");
}

template <typename T>
typename detail::pp_if_array<T>::type
make_persistent(PMEMobjpool *pop, std::size_t N) {
    typedef typename detail::pp_array_type<T>::type I;

    PMEMoid oid;
    int ret = pmemobj_alloc(pop, &oid, sizeof(I) * N,
            detail::type_num<I>(), NULL, NULL);

    if (ret != 0)
        throw transaction_alloc_error("failed to allocate "
                "persistent memory array");

    persistent_ptr<T> ptr = oid;
    std::size_t i;
    try {
        for (i = 0; i < N; ++i)
            detail::create<I>(ptr.get() + i);

    } catch (...) {
        for (std::size_t j = 1; j <= i; ++j)
            detail::destroy<I>(ptr[i - j]);
        pmemobj_free(&oid);
        throw;
    }

    return ptr;
}

template <typename T>
void
delete_persistent(typename detail::pp_if_array<T>::type ptr, std::size_t N) {
    typedef typename detail::pp_array_type<T>::type I;

    if (ptr == nullptr) return;

    for (std::size_t i = 0; i < N; ++i)
        detail::destroy<I>(ptr[N - 1 - i]);

    pmemobj_free((PMEMoid *)&ptr.raw());
}

} // namespace obj
} // namespace pmem
