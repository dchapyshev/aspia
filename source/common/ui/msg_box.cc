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

#include "common/ui/msg_box.h"

#include <QAbstractButton>

namespace common {

//--------------------------------------------------------------------------------------------------
MsgBox::MsgBox(QWidget* parent)
    : QMessageBox(parent)
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
MsgBox::MsgBox(Icon icon, const QString& title, const QString& text, StandardButtons buttons, QWidget* parent)
    : QMessageBox(icon, title, text, buttons, parent)
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
MsgBox::~MsgBox() = default;

//--------------------------------------------------------------------------------------------------
int MsgBox::exec()
{
    static const struct
    {
        QMessageBox::StandardButton button;
        const char* text;
    } kButtons[] =
    {
        { QMessageBox::Ok,              QT_TR_NOOP("OK")                },
        { QMessageBox::Cancel,          QT_TR_NOOP("Cancel")            },
        { QMessageBox::Yes,             QT_TR_NOOP("Yes")               },
        { QMessageBox::No,              QT_TR_NOOP("No")                },
        { QMessageBox::Apply,           QT_TR_NOOP("Apply")             },
        { QMessageBox::Close,           QT_TR_NOOP("Close")             },
        { QMessageBox::Save,            QT_TR_NOOP("Save")              },
        { QMessageBox::Discard,         QT_TR_NOOP("Discard")           },
        { QMessageBox::Reset,           QT_TR_NOOP("Reset")             },
        { QMessageBox::Help,            QT_TR_NOOP("Help")              },
        { QMessageBox::Abort,           QT_TR_NOOP("Abort")             },
        { QMessageBox::Retry,           QT_TR_NOOP("Retry")             },
        { QMessageBox::Ignore,          QT_TR_NOOP("Ignore")            },
        { QMessageBox::RestoreDefaults, QT_TR_NOOP("Restore Defaults")  },
        { QMessageBox::SaveAll,         QT_TR_NOOP("Save All")          },
        { QMessageBox::Open,            QT_TR_NOOP("Open")              },
        { QMessageBox::YesToAll,        QT_TR_NOOP("Yes to All")        },
        { QMessageBox::NoToAll,         QT_TR_NOOP("No to All")         },
    };

    for (const auto& entry : kButtons)
    {
        QAbstractButton* btn = button(entry.button);
        if (btn)
            btn->setText(tr(entry.text));
    }

    return QMessageBox::exec();
}

//--------------------------------------------------------------------------------------------------
// static
int MsgBox::warning(QWidget* parent, const QString& text, StandardButtons buttons)
{
    MsgBox message_box(QMessageBox::Warning, tr("Warning"), text, buttons, parent);
    return message_box.exec();
}

//--------------------------------------------------------------------------------------------------
// static
int MsgBox::information(QWidget* parent, const QString& text, StandardButtons buttons)
{
    MsgBox message_box(QMessageBox::Information, tr("Information"), text, buttons, parent);
    return message_box.exec();
}

//--------------------------------------------------------------------------------------------------
// static
int MsgBox::question(QWidget* parent, const QString& text, StandardButtons buttons)
{
    MsgBox message_box(QMessageBox::Question, tr("Confirmation"), text, buttons, parent);
    return message_box.exec();
}

} // namespace common
