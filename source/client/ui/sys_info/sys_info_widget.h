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

#ifndef CLIENT_UI_SYS_INFO_SYS_INFO_WIDGET_H
#define CLIENT_UI_SYS_INFO_SYS_INFO_WIDGET_H

#include <QWidget>

namespace proto::system_info {
class SystemInfo;
class SystemInfoRequest;
} // namespace proto::system_info

class QTreeWidget;
class QTreeWidgetItem;

class SysInfoWidget : public QWidget
{
    Q_OBJECT

public:
    ~SysInfoWidget() override = default;

    virtual std::string category() const = 0;
    virtual proto::system_info::SystemInfoRequest request() const;
    virtual void setSystemInfo(const proto::system_info::SystemInfo& system_info) = 0;
    virtual QTreeWidget* treeWidget() = 0;
    virtual QByteArray saveState() const;
    virtual void restoreState(const QByteArray& state);

    static void searchInGoogle(const QString& request);

signals:
    void sig_systemInfoRequest(const proto::system_info::SystemInfoRequest& request);

protected:
    explicit SysInfoWidget(QWidget* parent = nullptr);

    bool isStateRestored() const { return state_restored_; }

    void copyRow(QTreeWidgetItem* item);
    void copyColumn(QTreeWidgetItem* item, int column);

    // QWidget implementation.
    void changeEvent(QEvent* event) override;

    // Called on QEvent::LanguageChange. Subclasses override to re-apply translated text on
    // their own UI elements (typically by invoking ui.retranslateUi(this) and then calling
    // SysInfoWidget::retranslate() which re-requests the data from the host so the tree
    // items are rebuilt by setSystemInfo() with the new translations).
    virtual void retranslate();

private:
    bool state_restored_ = false;
};

#endif // CLIENT_UI_SYS_INFO_SYS_INFO_WIDGET_H
