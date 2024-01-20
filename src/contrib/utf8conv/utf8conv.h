////////////////////////////////////////////////////////////////////////////////
//
// utf8conv.h -- Copyright (C) by Giovanni Dicanio
//
// This header file defines a couple of C++ functions to convert between
// UTF-8 and UTF-16 Unicode encodings.
//
// Function implementations are inline, so this module can be simply reused in
// C++ projects just by adding to your project this header file and the associated 
// "utf8except.h" header defining the UTF-8 conversion exception class.
// Note that #including this header automatically #includes "utf8except.h".
//
// std::string is used to store UTF-8-encoded strings.
// std::wstring is used to store UTF-16-encoded strings.
//
// In case of conversion errors, instances of the Utf8ConversionException
// class are thrown.
//
// If the input string is too big and its size can't be safely converted
// to a value of type int, a std::overflow_error exception is thrown.
//
// Note that std::wstring is non-portable, as wchar_t's size is non-portable
// (e.g. 16 bits on Win32/Visual C++, 32 bits on Linux/GCC).
// But since this code directly interacts with Win32 APIs (MultiByteToWideChar
// and WideCharToMultiByte in particular), this portion of code is already
// platform-specific.
//
// This code is based on my MSDN Magazine article published 
// on the 2016 September issue:
//
// "C++ - Unicode Encoding Conversions with STL Strings and Win32 APIs"
// https://msdn.microsoft.com/magazine/mt763237
//
//
////////////////////////////////////////////////////////////////////////////////

#ifndef GIOVANNI_DICANIO_WIN32_UTF8CONV_H_
#define GIOVANNI_DICANIO_WIN32_UTF8CONV_H_


//
// Includes
//

#include <crtdbg.h>      // For _ASSERTE()
#include <Windows.h>     // Win32 Platform SDK main header        

#include <limits>        // For std::numeric_limits
#include <stdexcept>     // For std::overflow_error
#include <string>        // For std::string and std::wstring

#include "utf8except.h"  // Custom exception class for UTF-8 conversion errors


namespace win32
{


//------------------------------------------------------------------------------
// Convert form UTF-8 to UTF-16.
// Throws Utf8ConversionException on conversion errors
// (e.g. invalid UTF-8 sequence found in input string).
//------------------------------------------------------------------------------
inline std::wstring Utf8ToUtf16(const std::string& utf8)
{
    // Result of the conversion
    std::wstring utf16; 

    // First, handle the special case of empty input string
    if (utf8.empty())
    {
        _ASSERTE(utf16.empty());
        return utf16;
    }

    // Safely fail if an invalid UTF-8 character sequence is encountered
    constexpr DWORD kFlags = MB_ERR_INVALID_CHARS;

    // Safely cast the length of the source UTF-8 string (expressed in chars)
    // from size_t (returned by std::string::length()) to int 
    // for the MultiByteToWideChar API.
    // If the size_t value is too big to be stored into an int, 
    // throw an exception to prevent conversion errors like huge size_t values 
    // converted to *negative* integers.
    if (utf8.length() > static_cast<size_t>((std::numeric_limits<int>::max)())) 
    {
        throw std::overflow_error(
            "Input string too long: size_t-length doesn't fit into int.");
    }
    const int utf8Length = static_cast<int>(utf8.length());

    // Get the size of the destination UTF-16 string
    const int utf16Length = ::MultiByteToWideChar(
        CP_UTF8,       // source string is in UTF-8
        kFlags,        // conversion flags
        utf8.data(),   // source UTF-8 string pointer
        utf8Length,    // length of the source UTF-8 string, in chars
        nullptr,       // unused - no conversion done in this step
        0              // request size of destination buffer, in wchar_ts
    );
    if (utf16Length == 0)
    {
        // Conversion error: capture error code and throw
        const DWORD error = ::GetLastError();
        throw Utf8ConversionException(
            error == ERROR_NO_UNICODE_TRANSLATION ? 
            "Invalid UTF-8 sequence found in input string." 
            :
            "Cannot get result string length when converting " \
            "from UTF-8 to UTF-16 (MultiByteToWideChar failed).",
            error,
            Utf8ConversionException::ConversionType::FromUtf8ToUtf16);
    }

    // Make room in the destination string for the converted bits
    utf16.resize(utf16Length);

    // Do the actual conversion from UTF-8 to UTF-16
    int result = ::MultiByteToWideChar(
        CP_UTF8,       // source string is in UTF-8
        kFlags,        // conversion flags
        utf8.data(),   // source UTF-8 string pointer
        utf8Length,    // length of source UTF-8 string, in chars
        &utf16[0],     // pointer to destination buffer
        utf16Length    // size of destination buffer, in wchar_ts           
    );
    if (result == 0)
    {
        // Conversion error: capture error code and throw
        const DWORD error = ::GetLastError();
        throw Utf8ConversionException(
            error == ERROR_NO_UNICODE_TRANSLATION ?
            "Invalid UTF-8 sequence found in input string."
            :
            "Cannot convert from UTF-8 to UTF-16 "\
            "(MultiByteToWideChar failed).",
            error,
            Utf8ConversionException::ConversionType::FromUtf8ToUtf16);
    }

    return utf16;
}


//------------------------------------------------------------------------------
// Convert form UTF-16 to UTF-8.
// Throws Utf8ConversionException on conversion errors
// (e.g. invalid UTF-16 sequence found in input string).
//------------------------------------------------------------------------------
inline std::string Utf16ToUtf8(const std::wstring& utf16)
{
    // Result of the conversion
    std::string utf8;

    // First, handle the special case of empty input string
    if (utf16.empty())
    {
        _ASSERTE(utf8.empty());
        return utf8;
    }

    // Safely fail if an invalid UTF-16 character sequence is encountered
    constexpr DWORD kFlags = WC_ERR_INVALID_CHARS;

    // Safely cast the length of the source UTF-16 string (expressed in wchar_ts)
    // from size_t (returned by std::wstring::length()) to int 
    // for the WideCharToMultiByte API.
    // If the size_t value is too big to be stored into an int, 
    // throw an exception to prevent conversion errors like huge size_t values 
    // converted to *negative* integers.
    if (utf16.length() > static_cast<size_t>((std::numeric_limits<int>::max)()))
    {
        throw std::overflow_error(
            "Input string too long: size_t-length doesn't fit into int.");
    }
    const int utf16Length = static_cast<int>(utf16.length());

    // Get the length, in chars, of the resulting UTF-8 string
    const int utf8Length = ::WideCharToMultiByte(
        CP_UTF8,            // convert to UTF-8
        kFlags,             // conversion flags
        utf16.data(),       // source UTF-16 string
        utf16Length,        // length of source UTF-16 string, in wchar_ts
        nullptr,            // unused - no conversion required in this step
        0,                  // request size of destination buffer, in chars
        nullptr, nullptr    // unused
    );
    if (utf8Length == 0)
    {
        // Conversion error: capture error code and throw
        const DWORD error = ::GetLastError();
        throw Utf8ConversionException(
            error == ERROR_NO_UNICODE_TRANSLATION ?
            "Invalid UTF-16 sequence found in input string."
            :
            "Cannot get result string length when converting "\
            "from UTF-16 to UTF-8 (WideCharToMultiByte failed).",
            error,
            Utf8ConversionException::ConversionType::FromUtf16ToUtf8);
    }

    // Make room in the destination string for the converted bits
    utf8.resize(utf8Length);

    // Do the actual conversion from UTF-16 to UTF-8
    int result = ::WideCharToMultiByte(
        CP_UTF8,            // convert to UTF-8
        kFlags,             // conversion flags
        utf16.data(),       // source UTF-16 string
        utf16Length,        // length of source UTF-16 string, in wchar_ts
        &utf8[0],           // pointer to destination buffer
        utf8Length,         // size of destination buffer, in chars
        nullptr, nullptr    // unused
    );
    if (result == 0)
    {
        // Conversion error: capture error code and throw
        const DWORD error = ::GetLastError();
        throw Utf8ConversionException(
            error == ERROR_NO_UNICODE_TRANSLATION ?
            "Invalid UTF-16 sequence found in input string."
            :
            "Cannot convert from UTF-16 to UTF-8 "\
            "(WideCharToMultiByte failed).",
            error,
            Utf8ConversionException::ConversionType::FromUtf16ToUtf8);
    }

    return utf8;
}


} // namespace win32


#endif // GIOVANNI_DICANIO_WIN32_UTF8CONV_H_
