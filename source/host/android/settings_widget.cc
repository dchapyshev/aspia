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

#include "host/android/settings_widget.h"

#include <QVBoxLayout>

#include "base/gui_application.h"
#include "common/android/combo_box.h"
#include "common/android/label.h"
#include "host/user_settings.h"

namespace {

constexpr int kContentMargin = 16;
constexpr int kRowSpacing = 8;
constexpr int kSectionSpacing = 24;

} // namespace

//--------------------------------------------------------------------------------------------------
SettingsWidget::SettingsWidget(QWidget* parent)
    : ScrollArea(parent)
{
    buildContent();
}

//--------------------------------------------------------------------------------------------------
SettingsWidget::~SettingsWidget() = default;

//--------------------------------------------------------------------------------------------------
void SettingsWidget::retranslate()
{
    // The controls are built with translated strings, so the content is rebuilt. The current values
    // come from Settings, so nothing is lost.
    buildContent();
}

//--------------------------------------------------------------------------------------------------
void SettingsWidget::buildContent()
{
    QWidget* content = new QWidget(this);

    QVBoxLayout* layout = new QVBoxLayout(content);
    layout->setContentsMargins(kContentMargin, kContentMargin, kContentMargin, kContentMargin);
    layout->setSpacing(kRowSpacing);

    buildInterfaceSection(layout);
    layout->addStretch();

    // setWidget() deletes the previously set content widget.
    setWidget(content);
}

//--------------------------------------------------------------------------------------------------
void SettingsWidget::addSectionHeader(QVBoxLayout* layout, const QString& text)
{
    if (layout->count() > 0)
        layout->addSpacing(kSectionSpacing);

    layout->addWidget(new Label(text, Label::Role::CAPTION));
}

//--------------------------------------------------------------------------------------------------
void SettingsWidget::buildInterfaceSection(QVBoxLayout* layout)
{
    addSectionHeader(layout, tr("Interface"));

    UserSettings settings;

    ComboBox* language = new ComboBox();
    language->setLabel(tr("Language"));
    for (const auto& locale : GuiApplication::instance()->localeList())
        language->addItem(locale.second, locale.first);
    language->setCurrentIndex(qMax(0, language->findData(settings.locale())));
    connect(language, &QComboBox::currentIndexChanged, this, [language](int /* index */)
    {
        const QString locale = language->currentData().toString();
        UserSettings().setLocale(locale);
        GuiApplication::instance()->setLocale(locale);
    });
    layout->addWidget(language);

    ComboBox* theme = new ComboBox();
    theme->setLabel(tr("Theme"));
    for (const QString& id : GuiApplication::instance()->availableThemes())
        theme->addItem(GuiApplication::themeName(id), id);
    theme->setCurrentIndex(qMax(0, theme->findData(settings.theme())));
    connect(theme, &QComboBox::currentIndexChanged, this, [theme](int /* index */)
    {
        const QString id = theme->currentData().toString();
        UserSettings().setTheme(id);
        GuiApplication::instance()->setTheme(id);
    });
    layout->addWidget(theme);
}
