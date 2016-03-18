///////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2015 Microsoft Corporation. All rights reserved.
//
// This code is licensed under the MIT License (MIT).
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//
///////////////////////////////////////////////////////////////////////////////

#pragma once

#ifndef GSL_SPAN_H
#define GSL_SPAN_H

#include "gsl_assert.h"
#include "gsl_util.h"
#include <array>
#include <limits>
#include <iterator>
#include <stdexcept>
#include <type_traits>
#include <utility>

#ifdef _MSC_VER

// turn off some warnings that are noisy about our Expects statements
#pragma warning(push)
#pragma warning(disable : 4127) // conditional expression is constant

// No MSVC does constexpr fully yet
#pragma push_macro("constexpr")
#define constexpr

// VS 2013 workarounds
#if _MSC_VER <= 1800

#define GSL_MSVC_HAS_VARIADIC_CTOR_BUG
#define GSL_MSVC_NO_SUPPORT_FOR_MOVE_CTOR_DEFAULT

// noexcept is not understood 
#ifndef GSL_THROW_ON_CONTRACT_VIOLATION
#pragma push_macro("noexcept")
#define noexcept /* nothing */
#endif

// turn off some misguided warnings
#pragma warning(push)
#pragma warning(disable : 4351) // warns about newly introduced aggregate initializer behavior
#pragma warning(disable : 4512) // warns that assignment op could not be generated

#endif // _MSC_VER <= 1800

#endif // _MSC_VER

#ifdef GSL_THROW_ON_CONTRACT_VIOLATION

#ifdef _MSC_VER
#pragma push_macro("noexcept")
#endif

#define noexcept /* nothing */

#endif // GSL_THROW_ON_CONTRACT_VIOLATION

namespace gsl
{

template <class ElementType, std::ptrdiff_t Extent = dynamic_extent>
class span;


namespace details
{
template <class T>
struct is_span_oracle : std::false_type
{
};

template <class ElementType, std::ptrdiff_t Extent>
struct is_span_oracle<gsl::span<ElementType, Extent>> : std::true_type
{
};

template <class T>
struct is_span : is_span_oracle<std::remove_cv_t<T>>
{
};

template <class From, class To>
struct is_allowed_pointer_conversion
    : std::integral_constant<bool,
        std::is_pointer<From>::value &&
        std::is_pointer<To>::value &&
        std::is_convertible<From, To>::value
    >
{
};

template <class From, class To>
struct is_allowed_integral_conversion
    : std::integral_constant<bool,
        std::is_integral<From>::value &&
        std::is_integral<To>::value &&
        sizeof(From) == sizeof(To) && 
        alignof(From) == alignof(To) &&
        std::is_convertible<From, To>::value
    >
{
};

template <class From, class To>
struct is_allowed_element_type_conversion
    : std::integral_constant<bool,        
        std::is_same<From, std::remove_cv_t<To>>::value ||
        is_allowed_pointer_conversion<From, To>::value ||
        is_allowed_integral_conversion<From, To>::value
    >
{
};

template <class From>
struct is_allowed_element_type_conversion<From, char>
    : std::integral_constant<bool, !std::is_const<From>::value>
{
};

template <class From>
struct is_allowed_element_type_conversion<From, const char>
    : std::integral_constant<bool, true>
{
};



} // namespace details


// [views.constants], constants  
constexpr const std::ptrdiff_t dynamic_extent = -1;


// [span], class template span 
template <class ElementType, std::ptrdiff_t Extent>
class span {
public:
    // constants and types 
    using element_type = ElementType;
    using index_type = std::ptrdiff_t;
    using pointer = element_type*;
    using reference = element_type&;
#if 0  // TODO
    using iterator = /*implementation-defined */;
    using const_iterator = /* implementation-defined */;
    using reverse_iterator = std::reverse_iterator<iterator>;
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;
#endif 
    constexpr static const index_type extent = Extent;

    // [span.cons], span constructors, copy, assignment, and destructor 
    constexpr span() noexcept : storage_(nullptr, extent_type<0>())
    {}

    constexpr span(nullptr_t) noexcept : span()
    {}

    constexpr span(pointer ptr, index_type count) : storage_(ptr, count)
    { Expects(((!ptr && count == 0) || (ptr && count >= 0))); }

    constexpr span(pointer firstElem, pointer lastElem)
        : storage_(firstElem, std::distance(firstElem, lastElem))
    {}
    
    template <size_t N>
    constexpr span(element_type(&arr)[N])
        : storage_(&arr[0], extent_type<N>())
    {}

    template <size_t N>
    constexpr span(std::array<std::remove_const_t<element_type>, N>& arr)
        : storage_(&arr[0], extent_type<N>())
    {}

    template <size_t N, class = std::enable_if_t<is_const<element_type>::value>>
    constexpr span(const std::array<std::remove_const_t<element_type>, N>& arr)
        : storage_(&arr[0], extent_type<N>())
    {}

    // NB: the SFINAE here uses .data() as a incomplete/imperfect proxy for the requirement
    // on Container to be a contiguous sequence container.
    template <class Container,
        class = std::enable_if_t<!details::is_span<Container>::value &&
        std::is_convertible<Container::pointer, pointer>::value &&
        std::is_convertible<Container::pointer, decltype(std::declval<Container>().data())>::value>
    >
    constexpr span(Container& cont) : span(cont.data(), cont.size()) {}

    template <class Container,
    class = std::enable_if_t<std::is_const<element_type>::value &&
        !details::is_span<Container>::value &&
        std::is_convertible<Container::pointer, pointer>::value &&
        std::is_convertible<Container::pointer, decltype(std::declval<Container>().data())>::value>
    >
    constexpr span(const Container& cont) : span(cont.data(), cont.size()) {}

    constexpr span(const span& other) noexcept = default;
    constexpr span(span&& other) noexcept = default;

    template <class OtherElementType, std::ptrdiff_t OtherExtent,
        class = std::enable_if_t<!std::is_same<element_type, OtherElementType>::value &&
            details::is_allowed_element_type_conversion<OtherElementType, element_type>::value
        >
    >
    constexpr span(const span<OtherElementType, OtherExtent>& other)
        : storage_(reinterpret_cast<pointer>(other.data()), other.length())
    {}

    template <class OtherElementType, std::ptrdiff_t OtherExtent,
        class = std::enable_if_t<!std::is_same<element_type, OtherElementType>::value &&
            details::is_allowed_element_type_conversion<OtherElementType, element_type>::value
        >
    >
    constexpr span(span<OtherElementType, OtherExtent>&& other)
        : storage_(reinterpret_cast<pointer>(other.data()), other.length())
    {
    }

    ~span() noexcept = default;
    constexpr span& operator=(const span& other) noexcept = default;
    constexpr span& operator=(span&& other) noexcept = default;

    // [span.sub], span subviews  
    template <ptrdiff_t Count>
    constexpr span<element_type, Count> first() const
    {
        Expects(Count >= 0 && Count <= size());
        return { data(), Count };
    }

    template <ptrdiff_t Count>
    constexpr span<element_type, Count> last() const
    {
        Expects(Count >= 0 && Count <= size());
        return{ Count == 0 ? data() : data() + (size() - Count), Count };
    }

    template <ptrdiff_t Offset, ptrdiff_t Count = dynamic_extent>
    constexpr span<element_type, Count> subspan() const
    {
        Expects((Offset == 0 || Offset > 0 && Offset <= size()) &&
            (Count == dynamic_extent || Count >= 0 && Offset + Count <= size()));
        return { data() + Offset, Count == dynamic_extent ? size() - Offset : Count };
    }

    constexpr span<element_type, dynamic_extent> first(index_type count) const
    {
        Expects(count >= 0 && count <= size());
        return { data(), count };
    }

    constexpr span<element_type, dynamic_extent> last(index_type count) const
    {
        Expects(count >= 0 && count <= size());
        return { count == 0 ? data() : data() + (size() - count), count };
    }

    constexpr span<element_type, dynamic_extent> subspan(index_type offset,
        index_type count = dynamic_extent) const
    {
        Expects((offset == 0 || offset > 0 && offset <= size()) &&
            (count == dynamic_extent || count >= 0 && offset + count <= size()));
        return { data() + offset, count == dynamic_extent ? size() - offset : count };
    }

    // [span.obs], span observers 
    constexpr index_type length() const noexcept { return size(); }
    constexpr index_type size() const noexcept { return storage_.size();  }
    constexpr index_type length_bytes() const noexcept { return size_bytes(); }
    constexpr index_type size_bytes() const noexcept { return size() * sizeof(element_type); }
    constexpr bool empty() const noexcept { return size() == 0; }

    // [span.elem], span element access 
    constexpr reference operator[](index_type idx) const
    {
        Expects(idx >= 0 && idx < storage_.size());
        return storage_.data()[idx];
    }
    constexpr reference operator()(index_type idx) const { return this->operator[](idx); }
    constexpr pointer data() const noexcept { return storage_.data(); }
#if 0 // TODO
    // [span.iter], span iterator support 
    iterator begin() const noexcept;
    iterator end() const noexcept;
 
    const_iterator cbegin() const noexcept;
    const_iterator cend() const noexcept;

    reverse_iterator rbegin() const noexcept;
    reverse_iterator rend() const noexcept;

    const_reverse_iterator crbegin() const noexcept;
    const_reverse_iterator crend() const noexcept;
#endif
private:
    template <index_type Extent>
    class extent_type;
    
    template <index_type Extent>
    class extent_type
    {
    public:
        static_assert(Extent >= 0, "A fixed-size span must be >= 0 in size.");

        constexpr extent_type() noexcept {}

        template <index_type Other>
        constexpr extent_type(extent_type<Other>) noexcept
        {
            static_assert(Other == Extent,
                "Mismatch between fixed-size extent and size of initializing data.");
        }

        constexpr extent_type(index_type size)
        { Expects(size == Extent); }

        constexpr inline index_type size() const noexcept { return Extent; }
    };

    template <>
    class extent_type<dynamic_extent>
    {
    public:
        template <index_type Other>
        explicit constexpr extent_type(extent_type<Other> ext) : size_(ext.size())
        {}

        explicit constexpr extent_type(index_type size) : size_(size)
        { Expects(size >= 0); } 

        constexpr inline index_type size() const noexcept
        { return size_; }

    private:
        index_type size_;
    };

    // this implementation detail class lets us take advantage of the 
    // empty base class optimization to pay for only storage of a single
    // pointer in the case of fixed-size spans
    template <class ExtentType>
    class storage_type : public ExtentType
    {
    public:
        template <class OtherExtentType>
        storage_type(pointer data, OtherExtentType ext)
            : ExtentType(ext), data_(data) {}

        constexpr inline pointer data() const noexcept
        { return data_; }

    private:
        pointer data_;
    };

    storage_type<extent_type<Extent>> storage_;
};


#if 0 // TODO
// [span.comparison], span comparison operators 
template <class ElementType, ptrdiff_t Extent>
constexpr bool operator==(const span<ElementType, Extent>& l, const span<ElementType, Extent>& r) const noexcept;

template <class ElementType, ptrdiff_t Extent>
constexpr bool operator!=(const span<ElementType, Extent>& l, const span<ElementType, Extent>& r) const noexcept;

template <class ElementType, ptrdiff_t Extent>
constexpr bool operator<(const span<ElementType, Extent>& l, const span<ElementType, Extent>& r) const noexcept;

template <class ElementType, ptrdiff_t Extent>
constexpr bool operator<=(const span<ElementType, Extent>& l, const span<ElementType, Extent>& r) const noexcept;

template <class ElementType, ptrdiff_t Extent>
constexpr bool operator>(const span<ElementType, Extent>& l, const span<ElementType, Extent>& r) const noexcept;

template <class ElementType, ptrdiff_t Extent>
constexpr bool operator>=(const span<ElementType, Extent>& l, const span<ElementType, Extent>& r) const noexcept;
#endif


#if 0 // TODO
// [span.objectrep], views of object representation 
template <class ElementType, ptrdiff_t Extent>
constexpr span<const char, ((Extent == dynamic_extent) ? dynamic_extent : (sizeof(ElementType) * Extent))> as_bytes(span<ElementType, Extent> s) noexcept;

template <class ElementType, ptrdiff_t Extent>
constexpr span<char, ((Extent == dynamic_extent) ? dynamic_extent : (sizeof(ElementType) * Extent))> as_writeable_bytes(span<ElementType, Extent>) noexcept;
#endif

} // namespace gsl

#ifdef _MSC_VER

#undef constexpr
#pragma pop_macro("constexpr")

#if _MSC_VER <= 1800
#pragma warning(pop)

#ifndef GSL_THROW_ON_CONTRACT_VIOLATION
#undef noexcept
#pragma pop_macro("noexcept")
#endif // GSL_THROW_ON_CONTRACT_VIOLATION

#undef GSL_MSVC_HAS_VARIADIC_CTOR_BUG

#endif // _MSC_VER <= 1800

#endif // _MSC_VER

#if defined(GSL_THROW_ON_CONTRACT_VIOLATION)

#undef noexcept

#ifdef _MSC_VER
#pragma warning(pop)
#pragma pop_macro("noexcept")
#endif

#endif // GSL_THROW_ON_CONTRACT_VIOLATION

#endif // GSL_SPAN_H
