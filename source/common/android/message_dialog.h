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

#ifndef COMMON_ANDROID_MESSAGE_DIALOG_H
#define COMMON_ANDROID_MESSAGE_DIALOG_H

#include <QObject>
#include <QString>

#include "base/time_types.h"

class QWidget;

// Helpers for the common modal prompts built on Dialog.
class MessageDialog
{
    Q_GADGET

public:
    static bool confirm(QWidget* parent, const QString& title, const QString& text,
                        const QString& accept_text);

    static bool importantConfirm(QWidget* parent, const QString& title, const QString& text,
                                 const QString& accept_text, Seconds delay);

    static void info(QWidget* parent, const QString& title, const QString& text);

private:
    Q_DISABLE_COPY_MOVE(MessageDialog)
};

#endif // COMMON_ANDROID_MESSAGE_DIALOG_H
