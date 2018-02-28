//
// PROJECT:         Aspia
// FILE:            ui/mru.h
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_UI__MRU_H
#define _ASPIA_UI__MRU_H

#include "proto/mru.pb.h"

namespace aspia {

class MRU
{
public:
    MRU();
    ~MRU();

    static proto::Computer GetDefaultConfig();

    void AddItem(const proto::Computer& computer);
    int GetItemCount() const;
    const proto::Computer& GetItem(int item_index) const;
    const proto::Computer& SetLastItem(int item_index);

private:
    proto::MRU mru_;
};

} // namespace aspia

#endif // _ASPIA_UI__MRU_H
