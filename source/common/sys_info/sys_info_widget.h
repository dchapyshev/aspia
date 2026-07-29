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

#ifndef COMMON_SYS_INFO_SYS_INFO_WIDGET_H
#define COMMON_SYS_INFO_SYS_INFO_WIDGET_H

#include <QWidget>

#include <string>
#include <vector>

namespace proto::system_info {
class SystemInfo;
class SystemInfoRequest;
} // namespace proto::system_info

class QTreeWidget;
class QTreeWidgetItem;
class SysInfoReport;

class SysInfoWidget : public QWidget
{
    Q_OBJECT

public:
    static const char kBoxImportantIcon[];
    static const char kCancelIcon[];
    static const char kCertificateIcon[];
    static const char kCheckIcon[];
    static const char kComputerIcon[];
    static const char kComputerCaseIcon[];
    static const char kConnectedIcon[];
    static const char kDayViewIcon[];
    static const char kElectricalIcon[];
    static const char kElectricityIcon[];
    static const char kFanIcon[];
    static const char kFeatureIcon[];
    static const char kFileDocumentIcon[];
    static const char kFlowChartIcon[];
    static const char kFolderIcon[];
    static const char kFrequencyIcon[];
    static const char kGearIcon[];
    static const char kHddIcon[];
    static const char kHeartMonitorIcon[];
    static const char kHighImportanceIcon[];
    static const char kImacIcon[];
    static const char kInfoIcon[];
    static const char kIntegratedCircuitIcon[];
    static const char kLockIcon[];
    static const char kLockedUserIcon[];
    static const char kLogIcon[];
    static const char kMemoryIcon[];
    static const char kMemorySlotIcon[];
    static const char kMicrochipIcon[];
    static const char kMotherboardIcon[];
    static const char kNasIcon[];
    static const char kNetworkCardIcon[];
    static const char kOperatingSystemIcon[];
    static const char kPrinterIcon[];
    static const char kProcessorIcon[];
    static const char kPs2MaleIcon[];
    static const char kRestartIcon[];
    static const char kSoftwareIcon[];
    static const char kTemperatureIcon[];
    static const char kUserIcon[];
    static const char kUserAccountIcon[];
    static const char kVideoCardIcon[];
    static const char kVirtualMachineIcon[];

    ~SysInfoWidget() override = default;

    virtual std::string category() const = 0;

    virtual std::vector<proto::system_info::SystemInfoRequest> requests() const;
    virtual void setSystemInfo(const proto::system_info::SystemInfo& system_info) = 0;
    virtual QTreeWidget* treeWidget() = 0;
    virtual void buildReport(SysInfoReport* report);

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

private:
    bool state_restored_ = false;
};

#endif // COMMON_SYS_INFO_SYS_INFO_WIDGET_H
