//
// Aspia Project
// Copyright (C) 2016-2025 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "common/ui/taskbar_progress.h"

namespace common {

//--------------------------------------------------------------------------------------------------
TaskbarProgress::TaskbarProgress(QObject *parent)
    : QObject(parent)
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
TaskbarProgress::~TaskbarProgress() = default;

//--------------------------------------------------------------------------------------------------
void TaskbarProgress::setValue(int value)
{
    if ((value == value_) || value < minimum_ || value > maximum_)
        return;

    value_ = value;
    emit sig_valueChanged(value_);
}

//--------------------------------------------------------------------------------------------------
void TaskbarProgress::setMinimum(int minimum)
{
    setRange(minimum, std::max(minimum, maximum_));
}

//--------------------------------------------------------------------------------------------------
void TaskbarProgress::setMaximum(int maximum)
{
    setRange(std::min(minimum_, maximum), maximum);
}

//--------------------------------------------------------------------------------------------------
void TaskbarProgress::setVisible(bool visible)
{
    if (visible == visible_)
        return;

    visible_ = visible;
    emit sig_visibilityChanged(visible_);
}

//--------------------------------------------------------------------------------------------------
void TaskbarProgress::show()
{
    setVisible(true);
}

//--------------------------------------------------------------------------------------------------
void TaskbarProgress::hide()
{
    setVisible(false);
}

//--------------------------------------------------------------------------------------------------
void TaskbarProgress::setRange(int minimum, int maximum)
{
    const bool minChanged = minimum != minimum_;
    const bool maxChanged = maximum != maximum_;

    if (minChanged || maxChanged)
    {
        minimum_ = minimum;
        maximum_ = std::max(minimum, maximum);

        if (value_ < minimum_ || value_ > maximum_)
            reset();

        if (minChanged)
            emit sig_minimumChanged(minimum_);
        if (maxChanged)
            emit sig_maximumChanged(maximum_);
    }
}

//--------------------------------------------------------------------------------------------------
void TaskbarProgress::reset()
{
    setValue(minimum());
}

//--------------------------------------------------------------------------------------------------
void TaskbarProgress::setPaused(bool paused)
{
    if (paused == paused_ || stopped_)
        return;

    paused_ = paused;
    emit sig_pausedChanged(paused_);
}

//--------------------------------------------------------------------------------------------------
void TaskbarProgress::pause()
{
    setPaused(true);
}

//--------------------------------------------------------------------------------------------------
void TaskbarProgress::resume()
{
    setPaused(false);
    if (stopped_)
    {
        stopped_ = false;
        emit sig_stoppedChanged(false);
    }
}

//--------------------------------------------------------------------------------------------------
void TaskbarProgress::stop()
{
    setPaused(false);
    if (!stopped_)
    {
        stopped_ = true;
        emit sig_stoppedChanged(stopped_);
    }
}

} // namespace common
