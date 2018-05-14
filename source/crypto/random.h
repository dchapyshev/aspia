//
// PROJECT:         Aspia
// FILE:            crypto/random.h
// LICENSE:         GNU General Public License 3
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_CRYPTO__RANDOM_H
#define _ASPIA_CRYPTO__RANDOM_H

#include <QByteArray>

namespace aspia {

class Random
{
public:
    static QByteArray generateBuffer(int size);
    static quint32 generateNumber();

private:
    Q_DISABLE_COPY(Random)
};

} // namespace aspia

#endif // _ASPIA_CRYPTO__RANDOM_H
