//
// PROJECT:         Aspia
// FILE:            host/ui/host_config_dialog.h
// LICENSE:         GNU General Public License 3
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_HOST__UI__HOST_CONFIG_DIALOG_H
#define _ASPIA_HOST__UI__HOST_CONFIG_DIALOG_H

#include "host/user.h"
#include "ui_host_config_dialog.h"

namespace aspia {

class HostConfigDialog : public QDialog
{
    Q_OBJECT

public:
    explicit HostConfigDialog(QWidget* parent = nullptr);
    ~HostConfigDialog();

private slots:
    void onServiceInstallRemove();
    void onServiceStartStop();
    void onContextMenu(const QPoint& point);
    void onCurrentUserChanged(QTreeWidgetItem* current, QTreeWidgetItem* previous);
    void onAddUser();
    void onModifyUser();
    void onDeleteUser();
    void onButtonBoxClicked(QAbstractButton* button);

private:
    void installTranslators(const QString& locale);
    void removeTranslators();
    void createLanguageList(const QString& current_locale);
    void reloadUserList();
    void reloadServiceStatus();
    bool restartService();

    enum class ServiceStatus
    {
        Unknown,
        NotInstalled,
        Stopped,
        Started
    };

    Ui::HostConfigDialog ui;

    QHash<QString, QStringList> locale_list_;
    QList<QTranslator*> translator_list_;

    QList<User> user_list_;
    ServiceStatus service_status_;

    Q_DISABLE_COPY(HostConfigDialog)
};

} // namespace aspia

#endif // _ASPIA_HOST__UI__HOST_CONFIG_DIALOG_H
