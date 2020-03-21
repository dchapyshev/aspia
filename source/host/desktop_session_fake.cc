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

#include "host/desktop_session_fake.h"

#include "base/logging.h"
#include "base/task_runner.h"
#include "desktop/desktop_frame_simple.h"

namespace host {

namespace {

const int kFrameWidth = 800;
const int kFrameHeight = 600;

} // namespace

class DesktopSessionFake::FrameGenerator : public std::enable_shared_from_this<FrameGenerator>
{
public:
    explicit FrameGenerator(std::shared_ptr<base::TaskRunner> task_runner);
    ~FrameGenerator() = default;

    void start(Delegate* delegate);
    void stop();

private:
    void generateFrame();

    Delegate* delegate_ = nullptr;

    std::shared_ptr<base::TaskRunner> task_runner_;
    std::unique_ptr<desktop::Frame> frame_;

    DISALLOW_COPY_AND_ASSIGN(FrameGenerator);
};

DesktopSessionFake::FrameGenerator::FrameGenerator(std::shared_ptr<base::TaskRunner> task_runner)
    : task_runner_(std::move(task_runner))
{
    DCHECK(task_runner_);
}

void DesktopSessionFake::FrameGenerator::start(Delegate* delegate)
{
    delegate_ = delegate;
    DCHECK(delegate);

    frame_ = desktop::FrameSimple::create(
        desktop::Size(kFrameWidth, kFrameHeight), desktop::PixelFormat::ARGB());

    memset(frame_->frameData(), 0, frame_->stride() * frame_->size().height());

    generateFrame();
}

void DesktopSessionFake::FrameGenerator::stop()
{
    delegate_ = nullptr;
}

void DesktopSessionFake::FrameGenerator::generateFrame()
{
    desktop::Region* updated_region = frame_->updatedRegion();
    updated_region->clear();
    updated_region->addRect(desktop::Rect::makeWH(kFrameWidth, kFrameHeight));

    if (delegate_)
    {
        delegate_->onScreenCaptured(*frame_);

        task_runner_->postDelayedTask(
            std::bind(&FrameGenerator::generateFrame, shared_from_this()),
            std::chrono::milliseconds(1000));
    }
    else
    {
        frame_.reset();
    }
}

DesktopSessionFake::DesktopSessionFake(
    std::shared_ptr<base::TaskRunner> task_runner, Delegate* delegate)
    : frame_generator_(std::make_shared<FrameGenerator>(std::move(task_runner))),
      delegate_(delegate)
{
    DCHECK(delegate_);
}

DesktopSessionFake::~DesktopSessionFake()
{
    frame_generator_->stop();
}

void DesktopSessionFake::start()
{
    if (delegate_)
        delegate_->onDesktopSessionStarted();
}

void DesktopSessionFake::stop()
{
    delegate_ = nullptr;
    frame_generator_->stop();
}

void DesktopSessionFake::enableSession(bool enable)
{
    if (enable)
        frame_generator_->start(delegate_);
    else
        frame_generator_->stop();
}

void DesktopSessionFake::selectScreen(const proto::Screen& /* screen */)
{
    // Nothing
}

void DesktopSessionFake::enableFeatures(const proto::internal::EnableFeatures& /* features */)
{
    // Nothing
}

void DesktopSessionFake::injectKeyEvent(const proto::KeyEvent& /* event */)
{
    // Nothing
}

void DesktopSessionFake::injectPointerEvent(const proto::PointerEvent& /* event */)
{
    // Nothing
}

void DesktopSessionFake::injectClipboardEvent(const proto::ClipboardEvent& /* event */)
{
    // Nothing
}

void DesktopSessionFake::userSessionControl(
    proto::internal::UserSessionControl::Action /* action */)
{
    // Nothing
}

} // namespace host
