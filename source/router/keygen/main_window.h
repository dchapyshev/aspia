//
// Aspia Project
// Copyright (C) 2020 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef ROUTER__KEYGEN__MAIN_WINDOW_H
#define ROUTER__KEYGEN__MAIN_WINDOW_H

#include "base/macros_magic.h"
#include "ui_main_window.h"

#include <QMainWindow>

namespace router {

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow();
    ~MainWindow();

private slots:
    void onCopyPrivateKey();
    void onSavePrivateKey();
    void onCopyPublicKey();
    void onSavePublicKey();
    void onGenerate();

private:
    void copyToClipboard(const QString& text);
    void saveToFile(const QString& text);

    Ui::MainWindow ui;

    DISALLOW_COPY_AND_ASSIGN(MainWindow);
};

} // namespace router

#endif // ROUTER__KEYGEN__MAIN_WINDOW_H
