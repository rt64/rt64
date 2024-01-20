////////////////////////////////////////////////////////////////////////////////
//
// utf8except.h -- Copyright (C) by Giovanni Dicanio
//
// This header file defines a C++ exception class that represents
// UTF-8 encoding conversion errors.
// This class is written using portable cross-platform C++ code,
// so this exception can be caught in cross-platform C++ portions of
// code (even if the throwing point is Windows-specific).
//
////////////////////////////////////////////////////////////////////////////////

#ifndef GIOVANNI_DICANIO_WIN32_UTF8EXCEPT_H_
#define GIOVANNI_DICANIO_WIN32_UTF8EXCEPT_H_


//
// Includes
//

#include <stdint.h>   // for uint32_t
#include <stdexcept>  // for std::runtime_error
#include <string>     // for std::string


namespace win32 
{


//------------------------------------------------------------------------------
// Error occurred during UTF-8 encoding conversions
//------------------------------------------------------------------------------
class Utf8ConversionException
    : public std::runtime_error
{
public:

    // Possible conversion "directions"
    enum class ConversionType
    {
        FromUtf8ToUtf16 = 0,
        FromUtf16ToUtf8
    };


    // Initialize with error message raw C-string, last Win32 error code and conversion direction
    Utf8ConversionException(const char* message, uint32_t errorCode, ConversionType type);

    // Initialize with error message string, last Win32 error code and conversion direction
    Utf8ConversionException(const std::string& message, uint32_t errorCode, ConversionType type);

    // Retrieve error code associated to the failed conversion
    uint32_t ErrorCode() const;

    // Direction of the conversion (e.g. from UTF-8 to UTF-16)
    ConversionType Direction() const;


private:
    // Error code from GetLastError()
    uint32_t _errorCode;

    // Direction of the conversion
    ConversionType _conversionType;
};


//
// Inline Method Implementations
//

inline Utf8ConversionException::Utf8ConversionException(
    const char* const message, 
    const uint32_t errorCode,
    const ConversionType type)

    : std::runtime_error(message)
    , _errorCode(errorCode)
    , _conversionType(type)
{
}


inline Utf8ConversionException::Utf8ConversionException(
    const std::string& message,
    const uint32_t errorCode,
    const ConversionType type)

    : std::runtime_error(message)
    , _errorCode(errorCode)
    , _conversionType(type)
{
}


inline uint32_t Utf8ConversionException::ErrorCode() const
{
    return _errorCode;
}


inline Utf8ConversionException::ConversionType Utf8ConversionException::Direction() const
{
    return _conversionType;
}


} // namespace win32


#endif // GIOVANNI_DICANIO_WIN32_UTF8EXCEPT_H_

