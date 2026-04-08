//
// Aspia Project
// Copyright (C) 2016-2026 Dmitry Chapyshev <dmitry@aspia.ru>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <https://www.gnu.org/licenses/>.
//

#include "client/ui/computers_tab/router_widget.h"

#include "base/logging.h"

namespace client {

//--------------------------------------------------------------------------------------------------
RouterWidget::RouterWidget(QWidget* parent)
    : ContentWidget(Type::ROUTER, parent)
{
    LOG(INFO) << "Ctor";
    ui.setupUi(this);
}

//--------------------------------------------------------------------------------------------------
RouterWidget::~RouterWidget()
{
    LOG(INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
int RouterWidget::itemCount() const
{
    return 0;
}

//--------------------------------------------------------------------------------------------------
QByteArray RouterWidget::saveState()
{
    return QByteArray();
}

//--------------------------------------------------------------------------------------------------
void RouterWidget::restoreState(const QByteArray& /* state */)
{

}

} // namespace client
