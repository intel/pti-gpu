/*
 Copyright 2016-2020 Intel Corporation
 
 Licensed under the Apache License, Version 2.0 (the "License");
 you may not use this file except in compliance with the License.
 You may obtain a copy of the License at
 
     http://www.apache.org/licenses/LICENSE-2.0
 
 Unless required by applicable law or agreed to in writing, software
 distributed under the License is distributed on an "AS IS" BASIS,
 WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 See the License for the specific language governing permissions and
 limitations under the License.
*/
#pragma once

#include <algorithm>
#include <cstdlib>
#include <iterator>
#include <new>
#include <sstream>
#include <iostream>
#include <tuple>
#include <utility>
#include <vector>

template <int CurIndex, class T, class U, class... Args>
struct get_tuple_elem_index {
    static constexpr int index = get_tuple_elem_index<CurIndex + 1, T, Args...>::index;
};

template <int CurIndex, class T, class... Args>
struct get_tuple_elem_index<CurIndex, T, T, Args...> {
    static constexpr int index = CurIndex;
};

template <class T, class... Args>
typename std::remove_reference<typename std::remove_cv<T>::type>::type& ccl_tuple_get(
    std::tuple<Args...>& t) {
    using non_cv_type = typename std::remove_cv<T>::type;
    using non_ref_type = typename std::remove_reference<non_cv_type>::type;
    return std::get<get_tuple_elem_index<0, non_ref_type, Args...>::index>(t);
}

template <class T, class... Args>
const typename std::remove_reference<typename std::remove_cv<T>::type>::type& ccl_tuple_get(
    const std::tuple<Args...>& t) {
    using non_cv_type = typename std::remove_cv<T>::type;
    using non_ref_type = typename std::remove_reference<non_cv_type>::type;
    return std::get<get_tuple_elem_index<0, non_ref_type, Args...>::index>(t);
}

template <class specific_tuple, class functor, size_t cur_index>
void ccl_tuple_for_each_impl(specific_tuple&& t, functor f, std::true_type tuple_finished) {
    // nothing to do
}

template <class specific_tuple, class functor, size_t cur_index>
void ccl_tuple_for_each_impl(specific_tuple&& t, functor f, std::false_type tuple_not_finished) {
    f(std::get<cur_index>(std::forward<specific_tuple>(t)));

    constexpr std::size_t tuple_size =
        std::tuple_size<typename std::remove_reference<specific_tuple>::type>::value;

    using is_tuple_finished_t = std::integral_constant<bool, cur_index + 1 >= tuple_size>;

    ccl_tuple_for_each_impl<specific_tuple, functor, cur_index + 1>(
        std::forward<specific_tuple>(t), f, is_tuple_finished_t{});
}

template <class specific_tuple, class functor, size_t cur_index = 0>
void ccl_tuple_for_each(specific_tuple&& t, functor f) {
    constexpr std::size_t tuple_size =
        std::tuple_size<typename std::remove_reference<specific_tuple>::type>::value;
    static_assert(tuple_size != 0, "Nothing to do, tuple is empty");

    using is_tuple_finished_t = std::integral_constant<bool, cur_index >= tuple_size>;
    ccl_tuple_for_each_impl<specific_tuple, functor, cur_index>(
        std::forward<specific_tuple>(t), f, is_tuple_finished_t{});
}

template <typename specific_tuple, size_t cur_index, typename functor, class... FunctionArgs>
void ccl_tuple_for_each_indexed_impl(functor,
                                     std::true_type tuple_finished,
                                     const FunctionArgs&... args) {}

template <typename specific_tuple, size_t cur_index, typename functor, class... FunctionArgs>
void ccl_tuple_for_each_indexed_impl(functor f,
                                     std::false_type tuple_not_finished,
                                     const FunctionArgs&... args) {
    using tuple_element_t = typename std::tuple_element<cur_index, specific_tuple>::type;

    f.template invoke<cur_index, tuple_element_t>(args...);

    constexpr std::size_t tuple_size =
        std::tuple_size<typename std::remove_reference<specific_tuple>::type>::value;

    using is_tuple_finished_t = std::integral_constant<bool, cur_index + 1 >= tuple_size>;

    ccl_tuple_for_each_indexed_impl<specific_tuple, cur_index + 1, functor>(
        f, is_tuple_finished_t{}, args...);
}

template <typename specific_tuple, typename functor, class... FunctionArgs>
void ccl_tuple_for_each_indexed(functor f, const FunctionArgs&... args) {
    constexpr std::size_t tuple_size =
        std::tuple_size<typename std::remove_reference<specific_tuple>::type>::value;
    static_assert(tuple_size != 0, "Nothing to do, tuple is empty");

    using is_tuple_finished_t = std::false_type; //non-empty tuple started
    ccl_tuple_for_each_indexed_impl<specific_tuple, 0, functor, FunctionArgs...>(
        f, is_tuple_finished_t{}, args...);
}

template <class T, size_t align>
struct aligned_allocator {
    using value_type = T;
    using pointer = T*;

    template <class U>
    struct rebind {
        using other = aligned_allocator<U, align>;
    };

    aligned_allocator() = default;
    ~aligned_allocator() = default;

    template <class U, size_t Ualign>
    constexpr aligned_allocator(const aligned_allocator<U, Ualign>&) noexcept {}

    inline pointer allocate(size_t n) {
        size_t size = sizeof(value_type) * n;
        size_t aligned_size = (size + align - 1) & ~(align - 1);
        void* ptr = aligned_alloc(align, aligned_size);
        if (!ptr) {
            throw std::bad_alloc();
        }
        return reinterpret_cast<pointer>(ptr);
    }

    inline void deallocate(pointer ptr, size_t size) noexcept {
        free(ptr);
    }
};

template <class T, size_t align = 4 * 1024>
using aligned_vector = std::vector<T, aligned_allocator<T, align>>;

namespace utils {

template <typename T>
inline void str_to_array(const char* input, std::vector<T>& output, char delimiter) {
    if (!input) {
        return;
    }
    std::stringstream ss(input);
    T temp{};
    while (ss >> temp) {
        output.push_back(temp);
        if (ss.peek() == delimiter) {
            ss.ignore();
        }
    }
}

template <class Container>
std::string vec_to_string(Container& elems) {
    if (elems.empty()) {
        return "<empty>";
    }

    size_t idx = 0;
    std::ostringstream ss;
    for (auto elem : elems) {
        ss << elem;
        idx++;
        if (idx < elems.size()) {
            ss << " ";
        }
    }
    return ss.str();
}

template <typename T>
void dump_vec(const std::vector<T>& vec, const std::string& name) {
    std::cout << name << ": [";
    for (const auto& item : vec) {
        std::cout << item << ", ";
    }
    std::cout << "]\n";
}

} // namespace utils
