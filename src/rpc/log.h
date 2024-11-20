/* date = June 1st 2022 19:33 */
#pragma once
#include <iostream>
#include <sstream>
#include <time.h>
#include <algorithm>
#include <string>

#define COLOR_RED "\033[0;31m"
#define COLOR_RED_BRIGHT "\033[1;31m"
#define COLOR_GREEN "\033[0;32m"
#define COLOR_GREEN_BRIGHT "\033[1;32m"
#define COLOR_BLUE "\033[0;34m"
#define COLOR_BLUE_BRIGHT "\033[1;34m"
#define COLOR_YELLOW "\033[0;33m"
#define COLOR_YELLOW_BRIGHT "\033[1;33m"
#define COLOR_BLACK "\033[0;30m"
#define COLOR_WHITE "\033[1;37m"
#define COLOR_NONE "\033[0m"
#define COLOR_CYAN "\033[0;36m"
#define FONT_BOLD "\e[1m"
#define FONT_UNDERLINE "\e[4m"
#define FONT_NORMAL "\e[21m\e[24m"

#define FONT_RESET FONT_NORMAL COLOR_NONE

template<typename T> struct PartTrait { typedef T Type; };

#ifndef NOINLINE_ATTRIBUTE
    #ifdef __ICC
        #define NOINLINE_ATTRIBUTE __attribute__(( noinline ))
    #else
        #define NOINLINE_ATTRIBUTE
    #endif // __ICC
#endif // NOINLINE_ATTRIBUTE

#define LOG(msg) Log(__FILE__, __LINE__, Part<bool, bool>() << msg)

#if !defined(ENABLE_VERBOSE)
    #define LOG_VERBOSE(msg) do{}while(0)
#endif

#if defined(LOG_MODULE)
    #define LOG_ERR(msg) LOG(COLOR_RED << "[" LOG_MODULE "] " << COLOR_NONE << msg)
    #define LOG_INFO(msg) LOG("[" LOG_MODULE "] " << msg)
    #if !defined(LOG_VERBOSE)
        #define LOG_VERBOSE(msg) LOG(COLOR_BLUE << " - [" LOG_MODULE "] " << COLOR_NONE << msg)
    #endif
#else
    #define LOG_ERR(msg) LOG(COLOR_RED << COLOR_NONE << msg)
    #define LOG_INFO(msg) LOG(msg)
    #if !defined(LOG_VERBOSE)
        #define LOG_VERBOSE(msg) LOG(msg)
    #endif
#endif

inline std::string __GetTimeString(struct tm tmstruct, const char *fmt){
    char buffer[80];
    strftime(buffer, sizeof(buffer), fmt, &tmstruct);
    return std::string(buffer);
}

inline std::string LoggableCurrentTime(void){
    time_t now = time(0);
    struct tm tstruct;
    tstruct = *localtime(&now);
    std::string str(COLOR_RED);
    str += __GetTimeString(tstruct, "%e %b ");
    str += std::string(COLOR_GREEN);
    str += __GetTimeString(tstruct, "%T");
    str += std::string(COLOR_NONE);
    return str;
}

template<typename T> inline
void Log(const char* file, int line, const T& msg) NOINLINE_ATTRIBUTE
{
    (void)(line);
    std::string now = LoggableCurrentTime();

    std::stringstream ss;
    ss << now << " " << COLOR_YELLOW << ": " << COLOR_NONE << msg;
    ss << "\n";

    std::cout << ss.str() << std::flush;
}


template<typename TValue, typename TPreviousPart>
struct Part : public PartTrait<Part<TValue, TPreviousPart>>
{
    Part()
        : value(nullptr), prev(nullptr)
    { }

    Part(const Part<TValue, TPreviousPart>&) = default;
    Part<TValue, TPreviousPart> operator=(
        const Part<TValue, TPreviousPart>&) = delete;

    Part(const TValue& v, const TPreviousPart& p)
        : value(&v), prev(&p)
    { }

    std::ostream& output(std::ostream& os) const
    {
        if (prev)
            os << *prev;
        if (value)
            os << *value;
        return os;
    }

    const TValue* value;
    const TPreviousPart* prev;
};


typedef std::ostream& (*PfnManipulator)(std::ostream&);

template<typename TPreviousPart> struct Part<PfnManipulator, TPreviousPart>
: public PartTrait<Part<PfnManipulator, TPreviousPart>>
{
    Part()
        : pfn(nullptr), prev(nullptr)
    { }

    Part(const Part<PfnManipulator, TPreviousPart>& that) = default;
    Part<PfnManipulator, TPreviousPart> operator=(const Part<PfnManipulator,
                                                  TPreviousPart>&) = delete;

    Part(PfnManipulator pfn_, const TPreviousPart& p)
        : pfn(pfn_), prev(&p)
    { }

    std::ostream& output(std::ostream& os) const
    {
        if (prev)
            os << *prev;
        if (pfn)
            pfn(os);
        return os;
    }

    PfnManipulator pfn;
    const TPreviousPart* prev;
};


template<typename TPreviousPart, typename T>
typename std::enable_if<std::is_base_of<PartTrait<TPreviousPart>, TPreviousPart>::value,
Part<T, TPreviousPart> >::type
operator<<(const TPreviousPart& prev, const T& value)
{
    return Part<T, TPreviousPart>(value, prev);
}

template<typename TPreviousPart>
typename std::enable_if<std::is_base_of<PartTrait<TPreviousPart>, TPreviousPart>::value,
Part<PfnManipulator, TPreviousPart> >::type
operator<<(const TPreviousPart& prev, PfnManipulator value)
{
    return Part<PfnManipulator, TPreviousPart>(value, prev);
}

template<typename TPart>
typename std::enable_if<std::is_base_of<PartTrait<TPart>, TPart>::value,std::ostream&>::type
operator<<(std::ostream& os, const TPart& part)
{
    return part.output(os);
}

