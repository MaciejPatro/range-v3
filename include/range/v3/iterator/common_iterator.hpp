/// \file
// Range v3 library
//
//  Copyright Eric Niebler 2014-present
//  Copyright Casey Carter 2016
//
//  Use, modification and distribution is subject to the
//  Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//
// Project home: https://github.com/ericniebler/range-v3
//
#ifndef RANGES_V3_ITERATOR_COMMON_ITERATOR_HPP
#define RANGES_V3_ITERATOR_COMMON_ITERATOR_HPP

#include <type_traits>
#include <meta/meta.hpp>
#include <range/v3/range_fwd.hpp>
#include <range/v3/iterator/basic_iterator.hpp>
#include <range/v3/utility/common_tuple.hpp>
#include <concepts/concepts.hpp>
#include <range/v3/iterator/concepts.hpp>
#include <range/v3/detail/variant.hpp>

namespace ranges
{
    /// \addtogroup group-iterator
    /// @{

    /// \cond
    namespace detail
    {
        template<typename I, typename S>
        variant<I, S> &cidata(common_iterator<I, S> &that)
        {
            return that.data_;
        }

        template<typename I, typename S>
        variant<I, S> const &cidata(common_iterator<I, S> const &that)
        {
            return that.data_;
        }
    }

#if RANGES_BROKEN_CPO_LOOKUP
    namespace _common_iterator_
    {
        struct adl_hook
        {};
    }
#endif
    /// \endcond

    template<typename I, typename S>
    struct common_iterator
#if RANGES_BROKEN_CPO_LOOKUP
      : private _common_iterator_::adl_hook
#endif
    {
    private:
        CPP_assert(Iterator<I>);
        CPP_assert(Sentinel<S, I>);
        CPP_assert(!Same<I, S>);
        variant<I, S> data_;

        friend variant<I, S> &detail::cidata<>(common_iterator<I, S> &);
        friend variant<I, S> const &detail::cidata<>(common_iterator<I, S> const &);
        struct emplace_fn
        {
            variant<I, S> *data_;
            template<typename T, std::size_t N>
            void operator()(indexed_element<T, N> t) const
            {
                ranges::emplace<N>(*data_, t.get());
            }
        };
        struct arrow_proxy_
        {
        private:
            friend common_iterator;
            iter_value_t<I> keep_;
            arrow_proxy_(iter_reference_t<I>&& x)
              : keep_(std::move(x))
            {}
        public:
            const iter_value_t<I>* operator->() const noexcept
            {
                return std::addressof(keep_);
            }
        };
        template<typename T>
        static T *operator_arrow_(T *p, int) noexcept
        {
            return p;
        }
        template<typename J, typename = detail::iter_arrow_t<J const>>
        static J operator_arrow_(J const &j, int) noexcept(noexcept(J(j)))
        {
            return j;
        }
        CPP_template(typename J)(
            requires std::is_reference<iter_reference_t<J>>::value)
        static meta::_t<std::add_pointer<iter_reference_t<J>>> operator_arrow_(J const &j, long) noexcept
        {
            auto &&r = *j;
            return std::addressof(r);
        }
        CPP_template(typename J)(
            requires Constructible<iter_value_t<J>, iter_reference_t<J>>)
        static arrow_proxy_ operator_arrow_(J const &j, ...)
            noexcept(noexcept(iter_value_t<J>(iter_value_t<J>(*j))))
        {
            return arrow_proxy_(*j);
        }
    public:
        using difference_type = iter_difference_t<I>;

        common_iterator() = default;
        common_iterator(I i)
          : data_(emplaced_index<0>, std::move(i))
        {}
        common_iterator(S s)
          : data_(emplaced_index<1>, std::move(s))
        {}
        CPP_template(typename I2, typename S2)(
            requires ConvertibleTo<I2, I> && ConvertibleTo<S2, S>)
        common_iterator(common_iterator<I2, S2> const &that)
          : data_(detail::variant_core_access::make_empty<I, S>())
        {
            detail::cidata(that).visit_i(emplace_fn{&data_});
        }
        CPP_template(typename I2, typename S2)(
            requires ConvertibleTo<I2, I> && ConvertibleTo<S2, S>)
        common_iterator & operator=(common_iterator<I2, S2> const &that)
        {
            detail::cidata(that).visit_i(emplace_fn{&data_});
            return *this;
        }
        iter_reference_t<I> operator*() noexcept(noexcept(iter_reference_t<I>(*std::declval<I &>())))
        {
            return *ranges::get<0>(data_);
        }
        CPP_member
        auto operator*() const noexcept(noexcept(iter_reference_t<I>(*std::declval<I const &>()))) ->
            CPP_ret(iter_reference_t<I>)(
                requires Readable<I const>)
        {
            return *ranges::get<0>(data_);
        }
        CPP_template(typename J = I)(
            requires Readable<J>)
        auto operator->() const
            noexcept(noexcept(common_iterator::operator_arrow_(std::declval<I const &>(), 42))) ->
            decltype(common_iterator::operator_arrow_(std::declval<J const &>(), 42))
        {
            return common_iterator::operator_arrow_(ranges::get<0>(data_), 42);
        }
        common_iterator& operator++()
        {
            ++ranges::get<0>(data_);
            return *this;
        }
#ifdef RANGES_WORKAROUND_MSVC_677925
        CPP_template(typename I2 = I)(
            requires not ForwardIterator<I2>)
        auto operator++(int) ->
            decltype(std::declval<I2 &>()++)
        {
            return ranges::get<0>(data_)++;
        }
#else // ^^^ workaround ^^^ / vvv no workaround vvv
        CPP_member
        auto operator++(int) ->
            CPP_ret(decltype(std::declval<I &>()++))(
                requires not ForwardIterator<I>)
        {
            return ranges::get<0>(data_)++;
        }
#endif // RANGES_WORKAROUND_MSVC_677925
        CPP_member
        auto operator++(int) -> CPP_ret(common_iterator)(
            requires ForwardIterator<I>)
        {
            return common_iterator(ranges::get<0>(data_)++);
        }

#if !RANGES_BROKEN_CPO_LOOKUP
        template<typename I_ = I>
        friend constexpr /*c++14*/
        auto iter_move(common_iterator const &i)
            noexcept(detail::has_nothrow_iter_move_v<I>) ->
            CPP_broken_friend_ret(iter_rvalue_reference_t<I>)(
                requires InputIterator<I_>)
        {
            return ranges::iter_move(ranges::get<0>(detail::cidata(i)));
        }
        template<typename I2, typename S2>
        friend auto iter_swap(
            common_iterator const &x, common_iterator<I2, S2> const &y)
            noexcept(is_nothrow_indirectly_swappable<I, I2>::value) ->
            CPP_broken_friend_ret(void)(
                requires IndirectlySwappable<I2, I>)
        {
            return ranges::iter_swap(
                ranges::get<0>(detail::cidata(x)),
                ranges::get<0>(detail::cidata(y)));
        }
#endif
    };

    /// \cond
#if RANGES_BROKEN_CPO_LOOKUP
    namespace _common_iterator_
    {
        template<typename I, typename S>
        constexpr /*c++14*/
        auto iter_move(common_iterator<I, S> const &i)
            noexcept(detail::has_nothrow_iter_move_v<I>) ->
            CPP_broken_friend_ret(iter_rvalue_reference_t<I>)(
                requires InputIterator<I>)
        {
            return ranges::iter_move(ranges::get<0>(detail::cidata(i)));
        }
        template<typename I1, typename S1, typename I2, typename S2>
        auto iter_swap(common_iterator<I1, S1> const &x,
                       common_iterator<I2, S2> const &y)
            noexcept(is_nothrow_indirectly_swappable<I1, I2>::value) ->
            CPP_broken_friend_ret(void)(
                requires IndirectlySwappable<I1, I2>)
        {
            return ranges::iter_swap(
                ranges::get<0>(detail::cidata(x)),
                ranges::get<0>(detail::cidata(y)));
        }
    }
#endif
    /// \endcond

    CPP_template(typename I1, typename I2, typename S1, typename S2)(
        requires Sentinel<S1, I2> && Sentinel<S2, I1> &&
            !EqualityComparableWith<I1, I2>)
    bool operator==(common_iterator<I1, S1> const &x, common_iterator<I2, S2> const &y)
    {
        return detail::cidata(x).index() == 1u ?
            (detail::cidata(y).index() == 1u || ranges::get<0>(detail::cidata(y)) == ranges::get<1>(detail::cidata(x))) :
            (detail::cidata(y).index() != 1u || ranges::get<0>(detail::cidata(x)) == ranges::get<1>(detail::cidata(y)));
    }

    CPP_template(typename I1, typename I2, typename S1, typename S2)(
        requires Sentinel<S1, I2> && Sentinel<S2, I1> &&
            EqualityComparableWith<I1, I2>)
    bool operator==(common_iterator<I1, S1> const &x, common_iterator<I2, S2> const &y)
    {
        return detail::cidata(x).index() == 1u ?
            (detail::cidata(y).index() == 1u || ranges::get<0>(detail::cidata(y)) == ranges::get<1>(detail::cidata(x))) :
            (detail::cidata(y).index() == 1u ?
                ranges::get<0>(detail::cidata(x)) == ranges::get<1>(detail::cidata(y)) :
                ranges::get<0>(detail::cidata(x)) == ranges::get<0>(detail::cidata(y)));
    }

    CPP_template(typename I1, typename I2, typename S1, typename S2)(
        requires Sentinel<S1, I2> && Sentinel<S2, I1>)
    bool operator!=(common_iterator<I1, S1> const &x, common_iterator<I2, S2> const &y)
    {
        return !(x == y);
    }

    CPP_template(typename I1, typename I2, typename S1, typename S2)(
        requires SizedSentinel<I1, I2> && SizedSentinel<S1, I2> &&
            SizedSentinel<S2, I1>)
    iter_difference_t<I2> operator-(
        common_iterator<I1, S1> const &x, common_iterator<I2, S2> const &y)
    {
        return detail::cidata(x).index() == 1u ?
            (detail::cidata(y).index() == 1u ? 0 : ranges::get<1>(detail::cidata(x)) - ranges::get<0>(detail::cidata(y))) :
            (detail::cidata(y).index() == 1u ?
                ranges::get<0>(detail::cidata(x)) - ranges::get<1>(detail::cidata(y)) :
                ranges::get<0>(detail::cidata(x)) - ranges::get<0>(detail::cidata(y)));
    }

    template<typename I, typename S>
    struct readable_traits<common_iterator<I, S>>
      : meta::if_c<
            (bool) Readable<I>,
            readable_traits<I>,
            meta::nil_>
    {};

    /// \cond
    namespace detail
    {
        template<typename I>
        auto demote_common_iter_cat(...) ->
            nil_;
        template<typename I>
        auto demote_common_iter_cat(long) ->
            with_iterator_category<std::input_iterator_tag>;
        CPP_template(typename I)(
            requires DerivedFrom<
                typename std::iterator_traits<I>::iterator_category,
                std::forward_iterator_tag>)
        with_iterator_category<std::forward_iterator_tag>
        demote_common_iter_cat(int);

        template<typename I, bool = (bool) InputIterator<I>>
        struct common_iterator_std_traits
          : decltype(detail::demote_common_iter_cat<I>(0))
        {
            using difference_type = iter_difference_t<I>;
            using value_type = iter_value_t<I>;
            using reference = iter_reference_t<I>;
            using pointer = detail::iter_pointer_t<I>;
            using iterator_concept =
                if_then_t<
                    (bool) ForwardIterator<I>,
                    std::forward_iterator_tag,
                    std::input_iterator_tag>;
        };

        template<typename I>
        struct common_iterator_std_traits<I, false>
        {
            using difference_type = iter_difference_t<I>;
            using value_type = void;
            using reference = void;
            using pointer = void;
            using iterator_category = std::output_iterator_tag;
        };
    }
    /// \endcond

    namespace cpp20
    {
        using ranges::common_iterator;
    }
    /// @}
}

/// \cond
namespace std
{
    template<typename I, typename S>
    struct iterator_traits< ::ranges::common_iterator<I, S>>
      : ::ranges::detail::common_iterator_std_traits<I>
    {};
}
/// \endcond

#endif