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

#ifndef HOST_UI_SECURITY_LOG_DIALOG_H
#define HOST_UI_SECURITY_LOG_DIALOG_H

#include <QDialog>

#include <memory>

class QSortFilterProxyModel;
class QStandardItemModel;

namespace Ui {
class SecurityLogDialog;
} // namespace Ui

class SecurityLogDialog final : public QDialog
{
    Q_OBJECT

public:
    explicit SecurityLogDialog(QWidget* parent = nullptr);
    ~SecurityLogDialog() final;

protected:
    // QDialog implementation.
    void closeEvent(QCloseEvent* event) final;
    void changeEvent(QEvent* event) final;

private slots:
    void onFileSelected(int index);
    void onRefresh();
    void onOpenDirectory();
    void onFilterChanged();

private:
    void retranslateUi();
    void reloadFileList();
    void switchToFile(const QString& file_path);
    void tailCurrentFile();
    void appendLine(const QString& line);
    void updateStatusBar();

    std::unique_ptr<Ui::SecurityLogDialog> ui;
    QStandardItemModel* model_ = nullptr;
    QSortFilterProxyModel* proxy_ = nullptr;

    QString log_directory_;
    QString current_file_path_;
    qint64 last_pos_ = 0;

    Q_DISABLE_COPY_MOVE(SecurityLogDialog)
};

#endif // HOST_UI_SECURITY_LOG_DIALOG_H
