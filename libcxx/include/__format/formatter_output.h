// -*- C++ -*-
//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef _LIBCPP___FORMAT_FORMATTER_OUTPUT_H
#define _LIBCPP___FORMAT_FORMATTER_OUTPUT_H

#include <__algorithm/copy.h>
#include <__algorithm/fill_n.h>
#include <__config>
#include <__format/parser_std_format_spec.h>
#include <__utility/move.h>
#include <__utility/unreachable.h>
#include <cstddef>
#include <string_view>

#if !defined(_LIBCPP_HAS_NO_PRAGMA_SYSTEM_HEADER)
#  pragma GCC system_header
#endif

_LIBCPP_BEGIN_NAMESPACE_STD

#if _LIBCPP_STD_VER > 17

namespace __formatter {

// TODO FMT remove _v2 suffix.
struct _LIBCPP_TYPE_VIS __padding_size_result_v2 {
  size_t __before_;
  size_t __after_;
};

// TODO FMT remove _v2 suffix.
_LIBCPP_HIDE_FROM_ABI constexpr __padding_size_result_v2 __padding_size_v2(size_t __size, size_t __width,
                                                                           __format_spec::__alignment __align) {
  _LIBCPP_ASSERT(__width > __size, "don't call this function when no padding is required");
  _LIBCPP_ASSERT(__align != __format_spec::__alignment::__default,
                 "the caller should adjust the default to the value required by the type");
  _LIBCPP_ASSERT(__align != __format_spec::__alignment::__zero_padding,
                 "the caller should have handled the zero-padding");

  size_t __fill = __width - __size;
  switch (__align) {
  case __format_spec::__alignment::__default:
  case __format_spec::__alignment::__zero_padding:
    __libcpp_unreachable();

  case __format_spec::__alignment::__left:
    return {0, __fill};

  case __format_spec::__alignment::__center: {
    // The extra padding is divided per [format.string.std]/3
    // __before = floor(__fill, 2);
    // __after = ceil(__fill, 2);
    size_t __before = __fill / 2;
    size_t __after = __fill - __before;
    return {__before, __after};
  }
  case __format_spec::__alignment::__right:
    return {__fill, 0};
  }
  __libcpp_unreachable();
}

/// Writes the input to the output with the required padding.
///
/// Since the output column width is specified the function can be used for
/// ASCII and Unicode output.
///
/// \pre [\a __first, \a __last) is a valid range.
/// \pre \a __size <= \a __width. Using this function when this pre-condition
///      doesn't hold incurs an unwanted overhead.
///
/// \param __first     Pointer to the first element to write.
/// \param __last      Pointer beyond the last element to write.
/// \param __out_it    The output iterator to write to.
/// \param __specs     The parsed formatting specifications.
/// \param __size      The (estimated) output column width. When the elements
///                    to be written are ASCII the following condition holds
///                    \a __size == \a __last - \a __first.
///
/// \returns           An iterator pointing beyond the last element written.
///
/// \note The type of the elements in range [\a __first, \a __last) can differ
/// from the type of \a __specs. Integer output uses \c std::to_chars for its
/// conversion, which means the [\a __first, \a __last) always contains elements
/// of the type \c char.
template <class _CharT, class _ParserCharT>
_LIBCPP_HIDE_FROM_ABI auto __write(const _CharT* __first, const _CharT* __last,
                                   output_iterator<const _CharT&> auto __out_it,
                                   __format_spec::__parsed_specifications<_ParserCharT> __specs, ptrdiff_t __size)
    -> decltype(__out_it) {
  _LIBCPP_ASSERT(__first <= __last, "Not a valid range");

  if (__size >= __specs.__width_)
    return _VSTD::copy(__first, __last, _VSTD::move(__out_it));

  __padding_size_result_v2 __padding =
      __formatter::__padding_size_v2(__size, __specs.__width_, __specs.__std_.__alignment_);
  __out_it = _VSTD::fill_n(_VSTD::move(__out_it), __padding.__before_, __specs.__fill_);
  __out_it = _VSTD::copy(__first, __last, _VSTD::move(__out_it));
  return _VSTD::fill_n(_VSTD::move(__out_it), __padding.__after_, __specs.__fill_);
}

#  ifndef _LIBCPP_HAS_NO_UNICODE
template <class _CharT>
_LIBCPP_HIDE_FROM_ABI auto __write_unicode_no_precision(basic_string_view<_CharT> __str,
                                                        output_iterator<const _CharT&> auto __out_it,
                                                        __format_spec::__parsed_specifications<_CharT> __specs)
    -> decltype(__out_it) {
  _LIBCPP_ASSERT(!__specs.__has_precision(), "use __write_unicode");
  // No padding -> copy the string
  if (!__specs.__has_width())
    return _VSTD::copy(__str.begin(), __str.end(), _VSTD::move(__out_it));

  // Non Unicode part larger than width -> copy the string
  auto __last = __format_spec::__detail::__estimate_column_width_fast(__str.begin(), __str.end());
  ptrdiff_t __size = __last - __str.begin();
  if (__size >= __specs.__width_)
    return _VSTD::copy(__str.begin(), __str.end(), _VSTD::move(__out_it));

  // Is there a non Unicode part?
  if (__last != __str.end()) {
    // Non Unicode and Unicode part larger than width -> copy the string
    __format_spec::__detail::__column_width_result __column_width =
        __format_spec::__detail::__estimate_column_width(__last, __str.end(), __specs.__width_);
    __size += __column_width.__width; // Note this new size is used when __size < __specs.__width_
    if (__size >= __specs.__width_)
      return _VSTD::copy(__str.begin(), __str.end(), _VSTD::move(__out_it));
  }

  return __formatter::__write(__str.begin(), __str.end(), _VSTD::move(__out_it), __specs, __size);
}
#  endif

template <class _CharT>
_LIBCPP_HIDE_FROM_ABI auto __write_unicode(basic_string_view<_CharT> __str,
                                           output_iterator<const _CharT&> auto __out_it,
                                           __format_spec::__parsed_specifications<_CharT> __specs)
    -> decltype(__out_it) {
#  ifndef _LIBCPP_HAS_NO_UNICODE
  if (!__specs.__has_precision())
    return __formatter::__write_unicode_no_precision(__str, _VSTD::move(__out_it), __specs);

  // Non unicode part larger than precision -> truncate the output and use the normal write operation.
  auto __last = __format_spec::__detail::__estimate_column_width_fast(__str.begin(), __str.end());
  ptrdiff_t __size = __last - __str.begin();
  if (__size >= __specs.__precision_)
    return __formatter::__write(__str.begin(), __str.begin() + __specs.__precision_, _VSTD::move(__out_it), __specs,
                                __specs.__precision_);

  // No non Unicode part, implies __size <  __specs.__precision_ -> use normal write operation
  if (__last == __str.end())
    return __formatter::__write(__str.begin(), __str.end(), _VSTD::move(__out_it), __specs, __str.size());

  __format_spec::__detail::__column_width_result __column_width =
      __format_spec::__detail::__estimate_column_width(__last, __str.end(), __specs.__precision_ - __size);
  __size += __column_width.__width;
  // Truncate the output
  if (__column_width.__ptr != __str.end())
    __str.remove_suffix(__str.end() - __column_width.__ptr);

  return __formatter::__write(__str.begin(), __str.end(), _VSTD::move(__out_it), __specs, __size);

#  else
  if (__specs.__has_precision()) {
    ptrdiff_t __size = __str.size();
    if (__size > __specs.__precision_)
      return __formatter::__write(__str.begin(), __str.begin() + __specs.__precision_, _VSTD::move(__out_it), __specs,
                                  __specs.__precision_);
  }
  return __formatter::__write(__str.begin(), __str.end(), _VSTD::move(__out_it), __specs, __str.size());

#  endif
}

} // namespace __formatter

#endif //_LIBCPP_STD_VER > 17

_LIBCPP_END_NAMESPACE_STD

#endif // _LIBCPP___FORMAT_FORMATTER_OUTPUT_H
