//
// PROJECT:         Aspia
// FILE:            ui/mru.h
// LICENSE:         Mozilla Public License Version 2.0
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

    void AddItem(const proto::ClientConfig& client_config);
    int GetItemCount() const;
    const proto::ClientConfig& GetItem(int item_index) const;

private:
    proto::MRU mru_;
};

} // namespace aspia

#endif // _ASPIA_UI__MRU_H
