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

#include "common/sys_info/sys_info_widget.h"

#include "common/sys_info/sys_info_report.h"
#include "proto/system_info.h"

#include <QApplication>
#include <QClipboard>
#include <QDesktopServices>
#include <QEvent>
#include <QHeaderView>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QUrl>

namespace {

//--------------------------------------------------------------------------------------------------
void copyTextToClipboard(const QString& text)
{
    if (text.isEmpty())
        return;

    QClipboard* clipboard = QApplication::clipboard();
    if (!clipboard)
        return;

    clipboard->setText(text);
}

} // namespace

//--------------------------------------------------------------------------------------------------
const char SysInfoWidget::kBoxImportantIcon[] = ":/img/box-important.svg";
const char SysInfoWidget::kCancelIcon[] = ":/img/cancel.svg";
const char SysInfoWidget::kCertificateIcon[] = ":/img/certificate.svg";
const char SysInfoWidget::kCheckIcon[] = ":/img/check.svg";
const char SysInfoWidget::kComputerIcon[] = ":/img/computer.svg";
const char SysInfoWidget::kComputerCaseIcon[] = ":/img/computer-case.svg";
const char SysInfoWidget::kConnectedIcon[] = ":/img/connected.svg";
const char SysInfoWidget::kDayViewIcon[] = ":/img/day-view.svg";
const char SysInfoWidget::kElectricalIcon[] = ":/img/electrical.svg";
const char SysInfoWidget::kElectricityIcon[] = ":/img/electricity.svg";
const char SysInfoWidget::kFanIcon[] = ":/img/fan.svg";
const char SysInfoWidget::kFeatureIcon[] = ":/img/feature.svg";
const char SysInfoWidget::kFileDocumentIcon[] = ":/img/file-document.svg";
const char SysInfoWidget::kFlowChartIcon[] = ":/img/flow-chart.svg";
const char SysInfoWidget::kFolderIcon[] = ":/img/folder.svg";
const char SysInfoWidget::kFrequencyIcon[] = ":/img/frequency.svg";
const char SysInfoWidget::kGearIcon[] = ":/img/gear.svg";
const char SysInfoWidget::kHddIcon[] = ":/img/hdd.svg";
const char SysInfoWidget::kHeartMonitorIcon[] = ":/img/heart-monitor.svg";
const char SysInfoWidget::kHighImportanceIcon[] = ":/img/high-importance.svg";
const char SysInfoWidget::kImacIcon[] = ":/img/imac.svg";
const char SysInfoWidget::kInfoIcon[] = ":/img/info.svg";
const char SysInfoWidget::kIntegratedCircuitIcon[] = ":/img/integrated-circuit.svg";
const char SysInfoWidget::kLockIcon[] = ":/img/lock.svg";
const char SysInfoWidget::kLockedUserIcon[] = ":/img/locked-user.svg";
const char SysInfoWidget::kLogIcon[] = ":/img/log.svg";
const char SysInfoWidget::kMemoryIcon[] = ":/img/memory.svg";
const char SysInfoWidget::kMemorySlotIcon[] = ":/img/memory-slot.svg";
const char SysInfoWidget::kMicrochipIcon[] = ":/img/microchip.svg";
const char SysInfoWidget::kMotherboardIcon[] = ":/img/motherboard.svg";
const char SysInfoWidget::kNasIcon[] = ":/img/nas.svg";
const char SysInfoWidget::kNetworkCardIcon[] = ":/img/network-card.svg";
const char SysInfoWidget::kOperatingSystemIcon[] = ":/img/operating-system.svg";
const char SysInfoWidget::kPrinterIcon[] = ":/img/printer.svg";
const char SysInfoWidget::kProcessorIcon[] = ":/img/processor.svg";
const char SysInfoWidget::kPs2MaleIcon[] = ":/img/ps-2-male.svg";
const char SysInfoWidget::kRestartIcon[] = ":/img/restart.svg";
const char SysInfoWidget::kSoftwareIcon[] = ":/img/software.svg";
const char SysInfoWidget::kTemperatureIcon[] = ":/img/temperature.svg";
const char SysInfoWidget::kUserIcon[] = ":/img/user.svg";
const char SysInfoWidget::kUserAccountIcon[] = ":/img/user-account.svg";
const char SysInfoWidget::kVideoCardIcon[] = ":/img/video-card.svg";
const char SysInfoWidget::kVirtualMachineIcon[] = ":/img/virtual-machine.svg";

//--------------------------------------------------------------------------------------------------
SysInfoWidget::SysInfoWidget(QWidget* parent)
    : QWidget(parent)
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
std::vector<proto::system_info::SystemInfoRequest> SysInfoWidget::requests() const
{
    std::string page_category = category();
    if (page_category.empty())
        return {};

    std::vector<proto::system_info::SystemInfoRequest> result;
    result.emplace_back().set_category(std::move(page_category));
    return result;
}

//--------------------------------------------------------------------------------------------------
void SysInfoWidget::buildReport(SysInfoReport* report)
{
    report->addTree(QString(), treeWidget());
}

//--------------------------------------------------------------------------------------------------
QByteArray SysInfoWidget::saveState() const
{
    QTreeWidget* tree = const_cast<SysInfoWidget*>(this)->treeWidget();
    if (!tree)
        return QByteArray();

    return tree->header()->saveState();
}

//--------------------------------------------------------------------------------------------------
void SysInfoWidget::restoreState(const QByteArray& state)
{
    if (state.isEmpty())
        return;

    QTreeWidget* tree = treeWidget();
    if (!tree)
        return;

    tree->header()->restoreState(state);
    state_restored_ = true;
}

//--------------------------------------------------------------------------------------------------
void SysInfoWidget::copyRow(QTreeWidgetItem* item)
{
    if (!item)
        return;

    QString result;

    int column_count = item->columnCount();
    if (column_count > 2)
    {
        for (int i = 0; i < column_count; ++i)
        {
            QString text = item->text(i);

            if (!text.isEmpty())
                result += text + ' ';
        }

        result.chop(1);
    }
    else
    {
        result = item->text(0) + ": " + item->text(1);
    }

    copyTextToClipboard(result);
}

//--------------------------------------------------------------------------------------------------
// static
void SysInfoWidget::searchInGoogle(const QString& request)
{
    QUrl find_url("https://www.google.com/search?q=" +
                  QString::fromLatin1(QUrl::toPercentEncoding(request)));
    QDesktopServices::openUrl(find_url);
}

//--------------------------------------------------------------------------------------------------
void SysInfoWidget::copyColumn(QTreeWidgetItem* item, int column)
{
    if (!item)
        return;

    copyTextToClipboard(item->text(column));
}
