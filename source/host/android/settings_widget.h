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

#ifndef HOST_ANDROID_SETTINGS_WIDGET_H
#define HOST_ANDROID_SETTINGS_WIDGET_H

#include "common/android/scroll_area.h"

class QVBoxLayout;

class SettingsWidget final : public ScrollArea
{
    Q_OBJECT

public:
    explicit SettingsWidget(QWidget* parent = nullptr);
    ~SettingsWidget() final;

    void retranslate();

private:
    void buildContent();
    void addSectionHeader(QVBoxLayout* layout, const QString& text);
    void buildInterfaceSection(QVBoxLayout* layout);

    Q_DISABLE_COPY_MOVE(SettingsWidget)
};

#endif // HOST_ANDROID_SETTINGS_WIDGET_H
