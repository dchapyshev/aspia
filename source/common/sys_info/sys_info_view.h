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

#ifndef COMMON_SYS_INFO_SYS_INFO_VIEW_H
#define COMMON_SYS_INFO_SYS_INFO_VIEW_H

#include <QWidget>

#include <memory>

namespace Ui {
class SysInfoView;
} // namespace Ui

namespace proto::system_info {
class SystemInfo;
class SystemInfoRequest;
} // namespace proto::system_info

class QAction;
class QHBoxLayout;
class QTreeWidgetItem;
class QVersionNumber;
class SysInfoWidget;
class SysInfoWidgetSummary;

// Shows a system information report: the category tree, the page of the selected category and the
// actions to refresh, save and print it. Where the report comes from is up to the owner: the view
// asks for a category with sig_systemInfoRequest and takes the answer in onSystemInfo().
class SysInfoView final : public QWidget
{
    Q_OBJECT

public:
    explicit SysInfoView(QWidget* parent = nullptr);
    ~SysInfoView() final;

    // Actions of the built-in tool bar. The owner may add them to a tool bar or a menu of its own;
    // they stay owned by the view.
    QList<QAction*> fileActions() const;
    QList<QAction*> viewActions() const;

    // Hides the built-in tool bar for an owner that shows the actions somewhere else.
    void setToolBarVisible(bool visible);

    // Versions shown on the summary page. Only a remote session knows them all; the page leaves out
    // the ones left unset.
    void setVersions(const QVersionNumber& client_version,
                     const QVersionNumber& host_version,
                     const QVersionNumber& router_version);

    QByteArray saveState() const;
    void restoreState(const QByteArray& state);

    // QWidget implementation.
    QSize sizeHint() const final;

signals:
    void sig_systemInfoRequest(const proto::system_info::SystemInfoRequest& request);

public slots:
    void onSystemInfo(const proto::system_info::SystemInfo& system_info);

    // Asks for every category again.
    void onRefresh();

private slots:
    void onCategoryItemClicked(QTreeWidgetItem* item, int column);
    void onSave();
    void onPrint();

private:
    void buildCategoryTree();

    // Saving and printing an empty page makes no sense.
    void updateExportActions();

    std::unique_ptr<Ui::SysInfoView> ui;
    QHBoxLayout* layout_ = nullptr;
    QList<SysInfoWidget*> sys_info_widgets_;
    SysInfoWidgetSummary* summary_widget_ = nullptr;
    int current_widget_ = 0;

    Q_DISABLE_COPY_MOVE(SysInfoView)
};

#endif // COMMON_SYS_INFO_SYS_INFO_VIEW_H
