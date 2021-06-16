#include <utility>
#ifndef YGG_UTIL_HPP
#define YGG_UTIL_HPP

#include <iterator>
#include <type_traits>

namespace ygg {

namespace utilities {

template <bool use_arith>
struct choose_ptr_impl
{
	template <class T>
	[[gnu::always_inline, gnu::const]] static inline T *
	choose_ptr(bool condition, T * yes_ptr, T * no_ptr);
};

template <>
struct choose_ptr_impl<true>
{
	template <class T>
	[[gnu::always_inline, gnu::const]] static inline T *
	choose_ptr(bool condition, T * yes_ptr, T * no_ptr)
	{
		return reinterpret_cast<T *>(
		    // TODO verify
		    static_cast<size_t>(condition) * reinterpret_cast<size_t>(yes_ptr) +
		    (size_t{1} - static_cast<size_t>(condition)) *
		        reinterpret_cast<size_t>(no_ptr));
	}
};

template <>
struct choose_ptr_impl<false>
{
	template <class T>
	[[gnu::always_inline, gnu::const]] static inline T *&
	choose_ptr(bool condition, T *& yes_ptr, T *& no_ptr)
	{
		if (condition) {
			return yes_ptr;
		} else {
			return no_ptr;
		}
	}
};

template <class Options, class T>
[[gnu::always_inline, gnu::const]] static inline T *
choose_ptr(bool condition, T * yes_ptr, T * no_ptr);

template <class Options, class T>
[[gnu::always_inline, gnu::const]] static inline T *
choose_ptr(bool condition, T * yes_ptr, T * no_ptr)
{
	return choose_ptr_impl<Options::micro_avoid_conditionals>::choose_ptr(
	    condition, yes_ptr, no_ptr);
}

template <class Node>
[[gnu::always_inline, gnu::pure]] static inline Node *
go_right_if(bool cond, Node * parent)
{
	return parent->_bst_children[cond];
}

template <class Node>
[[gnu::always_inline, gnu::pure]] static inline Node *
go_left_if(bool cond, Node * parent)
{
	return parent->_bst_children[(1 - cond)];
}

template <class T>
class TypeHolder {
public:
	using type = T;
};

// From
// https://stackoverflow.com/questions/31762958/check-if-class-is-a-template-specialization#31763111
template <class T, template <class...> class Template>
struct is_specialization : std::false_type
{
};

template <template <class...> class Template, class... Args>
struct is_specialization<Template<Args...>, Template> : std::true_type
{
};

template <class T, template <std::size_t> class Template>
struct is_numeric_specialization : std::false_type
{
};

template <template <std::size_t> class Template, std::size_t N>
struct is_numeric_specialization<Template<N>, Template> : std::true_type
{
};

class NotFoundMarker {
public:
	static constexpr std::size_t value = 0;
};

/* @brief A class providing an iterator over the integers 1, 2, … <n>
 */
// TODO this does not uphold the multipass guarantee (point 1). Check how boost
// does this.
template <class T>
class IntegerRange {
public:
	IntegerRange(T start_in, T stop_in) : start(start_in), stop(stop_in) {}

	class iterator {
	public:
		typedef T difference_type;
		typedef T value_type;
		typedef const T & reference;
		typedef T * pointer;
		typedef std::random_access_iterator_tag iterator_category;

		iterator(T start) : val(start) {}
		iterator(const iterator & other) : val(other.val) {}
		iterator() : val() {}

		iterator &
		operator=(const iterator & other)
		{
			val = other.val;
		};

		bool
		operator==(const iterator & other) const
		{
			return this->val == other.val;
		}
		bool
		operator!=(const iterator & other) const
		{
			return this->val != other.val;
		}

		iterator
		operator++(int)
		{
			iterator cpy = *this;
			this->val++;
			return cpy;
		}
		iterator &
		operator++()
		{
			this->val++;
			return *this;
		}
		iterator &
		operator+=(size_t steps)
		{
			this->val += steps;
			return *this;
		}
		iterator
		operator+(size_t steps) const
		{
			iterator cpy = *this;
			cpy += steps;
			return cpy;
		}

		iterator
		operator--(int)
		{
			iterator cpy = *this;
			this->val--;
			return cpy;
		}
		iterator &
		operator--()
		{
			this->val--;
			return *this;
		}
		iterator &
		operator-=(size_t steps)
		{
			this->val -= steps;
			return *this;
		}
		iterator
		operator-(size_t steps) const
		{
			iterator cpy = *this;
			cpy -= steps;
			return cpy;
		}

		difference_type
		operator-(const iterator & other) const
		{
			return this->val - other.val;
		}
		bool
		operator<(const iterator & other) const
		{
			return this->val < other.val;
		}
		bool
		operator<=(const iterator & other) const
		{
			return this->val <= other.val;
		}
		bool
		operator>(const iterator & other) const
		{
			return this->val > other.val;
		}
		bool
		operator>=(const iterator & other) const
		{
			return this->val >= other.val;
		}

		reference
		operator[](size_t n)
		{
			return this->val + n;
		}

		reference
		operator*() const
		{
			return this->val;
		}

		pointer
		operator->() const
		{
			return &(this->val);
		}

	private:
		T val;
	};

	iterator
	begin() const
	{
		return iterator(start);
	}

	iterator
	end() const
	{
		return iterator(stop);
	}

private:
	const T start;
	const T stop;
};

/**
 * @brief A more flexible version of std::less
 *
 * This is a more flexible version of std::less, which allows to compare two
 * objects of different types T1 and T2, as long as operator<(T1, T2) and
 * operator<(T2, T1) is defined.
 */
class flexible_less {
public:
	template <class T1, class T2>
	[[gnu::always_inline, gnu::const]] inline constexpr bool
	operator()(const T1 & lhs, const T2 & rhs) const noexcept
	{
		return lhs < rhs;
	}
};

/**
 * @brief A hasher for std::pair's of hashable types
 */
struct pair_hash
{
	template <class T1, class T2>
	std::size_t
	operator()(const std::pair<T1, T2> & p) const
	{
		auto h1 = std::hash<T1>{}(p.first);
		auto h2 = std::hash<T2>{}(p.second);

		// TODO do something more useful?
		return h1 ^ h2;
	}
};

/*
 * Allow selecting between two types.
 */
template <class TypeWhenTrue, class TypeWhenFalse, bool b>
struct select_type;

template <class TypeWhenTrue, class TypeWhenFalse>
struct select_type<TypeWhenTrue, TypeWhenFalse, false>
{
	using type = TypeWhenFalse;
};

template <class TypeWhenTrue, class TypeWhenFalse>
struct select_type<TypeWhenTrue, TypeWhenFalse, true>
{
	using type = TypeWhenTrue;
};

template <class TypeWhenTrue, class TypeWhenFalse, bool b>
using select_type_t =
    typename select_type<TypeWhenTrue, TypeWhenFalse, b>::type;

} // namespace utilities
} // namespace ygg

// All operations involving the comparison of a tree node and something else
// are only noexcept if that comparison is noexcept!
//
// Sequence storage involves memory allocation and thus is never noexcept
// While some of the methods using this macro might not be affected by this,
// we use the same macro for simpler code.
#ifndef YGG_STORE_SEQUENCE
#define CMP_NOEXCEPT(OBJ)                                                      \
	noexcept(noexcept((Compare{})(std::declval<Node>(), OBJ)))
#else
#define CMP_NOEXCEPT(OBJ)
#endif

#endif // YGG_UTIL_HPP
