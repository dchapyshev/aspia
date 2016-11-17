/*
* PROJECT:         Aspia Remote Desktop
* FILE:            base/exception.h
* LICENSE:         See top-level directory
* PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
*/

#ifndef _ASPIA_BASE__EXCEPTION_H
#define _ASPIA_BASE__EXCEPTION_H

#include <string>

class Exception
{
public:
    //
    // Конструктор по умолчанию (не имеет возможности передачи
    // тестового описания исключения).
    //
    Exception() {}

    //
    // Конструктор с возможностью передачи текстового описания
    // исключения. Параметр message не должен быть равен nullptr.
    //
    Exception(const char *description)
    {
        description_ = description;
    }

    virtual ~Exception() {}

    //
    // Метод для получения текстового описания исключения.
    //
    const char* What() const
    {
        return description_.c_str();
    }

private:
    std::string description_; // Хранит текстовое описание исключения.
};

#endif // _ASPIA_BASE__EXCEPTION_H
