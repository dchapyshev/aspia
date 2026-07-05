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

#include <QStackedWidget>
#include <QVBoxLayout>

#include "base/gui_application.h"
#include "common/android/about_widget.h"
#include "common/android/combo_box.h"
#include "common/android/icon_button.h"
#include "common/android/label.h"
#include "common/android/scroll_area.h"
#include "host/user_settings.h"

namespace {

constexpr int kContentMargin = 16;
constexpr int kRowSpacing = 8;
constexpr int kSectionSpacing = 24;

} // namespace

//--------------------------------------------------------------------------------------------------
SettingsWidget::SettingsWidget(QWidget* parent)
    : QWidget(parent),
      stack_(new QStackedWidget(this)),
      settings_page_(new ScrollArea()),
      about_page_(new AboutWidget()),
      about_button_(new IconButton(":/img/material/info.svg", this))
{
    // The about action lives in the app bar; AppBar::setActions() reparents and shows it. Hidden by
    // default so it does not linger in this widget.
    about_button_->hide();
    connect(about_button_, &IconButton::clicked, this, &SettingsWidget::showAbout);

    buildSettings();

    stack_->addWidget(settings_page_);
    stack_->addWidget(about_page_);

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(stack_);
}

//--------------------------------------------------------------------------------------------------
SettingsWidget::~SettingsWidget() = default;

//--------------------------------------------------------------------------------------------------
void SettingsWidget::retranslate()
{
    // The controls are built with translated strings, so the content is rebuilt. The current values
    // come from UserSettings, so nothing is lost.
    buildSettings();
    about_page_->retranslate();
}

//--------------------------------------------------------------------------------------------------
QList<QWidget*> SettingsWidget::appBarActions() const
{
    if (isAboutPage())
        return {};
    return { about_button_ };
}

//--------------------------------------------------------------------------------------------------
void SettingsWidget::goBack()
{
    if (!isAboutPage())
        return;

    stack_->setCurrentWidget(settings_page_);
    emit sig_titleChanged(QString(), false);
    emit sig_appBarActionsChanged();
}

//--------------------------------------------------------------------------------------------------
void SettingsWidget::resetToSettings()
{
    stack_->setCurrentWidget(settings_page_);
}

//--------------------------------------------------------------------------------------------------
void SettingsWidget::showAbout()
{
    stack_->setCurrentWidget(about_page_);
    emit sig_titleChanged(tr("About"), true);
    emit sig_appBarActionsChanged();
}

//--------------------------------------------------------------------------------------------------
bool SettingsWidget::isAboutPage() const
{
    return stack_->currentWidget() == about_page_;
}

//--------------------------------------------------------------------------------------------------
void SettingsWidget::buildSettings()
{
    QWidget* content = new QWidget(settings_page_);

    QVBoxLayout* layout = new QVBoxLayout(content);
    layout->setContentsMargins(kContentMargin, kContentMargin, kContentMargin, kContentMargin);
    layout->setSpacing(kRowSpacing);

    buildInterfaceSection(layout);
    layout->addStretch();

    // setWidget() deletes the previously set content widget.
    settings_page_->setWidget(content);
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
