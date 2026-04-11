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

#include "common/ui/dialog_button_box.h"

#include <QPushButton>
#include <QEvent>
#include <QShowEvent>

namespace common {

//--------------------------------------------------------------------------------------------------
DialogButtonBox::DialogButtonBox(QWidget* parent)
    : QDialogButtonBox(parent)
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
DialogButtonBox::DialogButtonBox(Qt::Orientation orientation, QWidget* parent)
    : QDialogButtonBox(orientation, parent)
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
DialogButtonBox::DialogButtonBox(StandardButtons buttons, QWidget* parent)
    : QDialogButtonBox(buttons, parent)
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
DialogButtonBox::DialogButtonBox(
    StandardButtons buttons, Qt::Orientation orientation, QWidget* parent)
    : QDialogButtonBox(buttons, orientation, parent)
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
DialogButtonBox::~DialogButtonBox() = default;

//--------------------------------------------------------------------------------------------------
void DialogButtonBox::showEvent(QShowEvent* event)
{
    translateButtons();
    QDialogButtonBox::showEvent(event);
}

//--------------------------------------------------------------------------------------------------
void DialogButtonBox::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::LanguageChange)
        translateButtons();

    QDialogButtonBox::changeEvent(event);
}

//--------------------------------------------------------------------------------------------------
void DialogButtonBox::translateButtons()
{
    static const struct {
        QDialogButtonBox::StandardButton button;
        const char* text;
    } kButtons[] = {
        { QDialogButtonBox::Ok,              QT_TR_NOOP("OK")               },
        { QDialogButtonBox::Cancel,          QT_TR_NOOP("Cancel")           },
        { QDialogButtonBox::Yes,             QT_TR_NOOP("Yes")              },
        { QDialogButtonBox::No,              QT_TR_NOOP("No")               },
        { QDialogButtonBox::Apply,           QT_TR_NOOP("Apply")            },
        { QDialogButtonBox::Close,           QT_TR_NOOP("Close")            },
        { QDialogButtonBox::Save,            QT_TR_NOOP("Save")             },
        { QDialogButtonBox::Discard,         QT_TR_NOOP("Discard")          },
        { QDialogButtonBox::Reset,           QT_TR_NOOP("Reset")            },
        { QDialogButtonBox::Help,            QT_TR_NOOP("Help")             },
        { QDialogButtonBox::Abort,           QT_TR_NOOP("Abort")            },
        { QDialogButtonBox::Retry,           QT_TR_NOOP("Retry")            },
        { QDialogButtonBox::Ignore,          QT_TR_NOOP("Ignore")           },
        { QDialogButtonBox::RestoreDefaults, QT_TR_NOOP("Restore Defaults") },
        { QDialogButtonBox::SaveAll,         QT_TR_NOOP("Save All")         },
        { QDialogButtonBox::Open,            QT_TR_NOOP("Open")             },
        { QDialogButtonBox::YesToAll,        QT_TR_NOOP("Yes to All")       },
        { QDialogButtonBox::NoToAll,         QT_TR_NOOP("No to All")        },
    };

    for (const auto& entry : kButtons)
    {
        QPushButton* button = QDialogButtonBox::button(entry.button);
        if (button)
            button->setText(tr(entry.text));
    }
}

} // namespace common
