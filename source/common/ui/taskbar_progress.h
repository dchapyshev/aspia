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

#ifndef COMMON_UI_TASKBAR_PROGRESS_H
#define COMMON_UI_TASKBAR_PROGRESS_H

#include <QObject>

namespace common {

class TaskbarProgress final : public QObject
{
    Q_OBJECT

public:
    explicit TaskbarProgress(QObject* parent = nullptr);
    ~TaskbarProgress();

    int value() const { return value_; }
    int minimum() const { return minimum_; }
    int maximum() const { return maximum_; }
    bool isVisible() const { return visible_; }
    bool isPaused() const { return paused_; }
    bool isStopped() const { return stopped_; }

public slots:
    void setValue(int value);
    void setMinimum(int minimum);
    void setMaximum(int maximum);
    void setRange(int minimum, int maximum);
    void reset();
    void show();
    void hide();
    void setVisible(bool visible);
    void pause();
    void resume();
    void setPaused(bool paused);
    void stop();

signals:
    void sig_valueChanged(int value);
    void sig_minimumChanged(int minimum);
    void sig_maximumChanged(int maximum);
    void sig_visibilityChanged(bool visible);
    void sig_pausedChanged(bool paused);
    void sig_stoppedChanged(bool stopped);

private:
    int value_ = 0;
    int minimum_ = 0;
    int maximum_ = 100;
    bool visible_ = false;
    bool paused_ = false;
    bool stopped_ = false;

    Q_DISABLE_COPY(TaskbarProgress)
};

} // namespace common

#endif // COMMON_UI_TASKBAR_PROGRESS_H
