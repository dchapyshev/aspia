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
#include "base/desktop/frame_simple.h"
#include "base/desktop/screen_capturer.h"

namespace host {

namespace {

const int kFrameWidth = 800;
const int kFrameHeight = 600;

} // namespace

class DesktopSessionFake::FrameGenerator : public std::enable_shared_from_this<FrameGenerator>
{
public:
    explicit FrameGenerator(std::shared_ptr<base::TaskRunner> task_runner);
    ~FrameGenerator();

    void start(Delegate* delegate);
    void stop();
    void generateFrame();

private:

    Delegate* delegate_ = nullptr;

    std::shared_ptr<base::TaskRunner> task_runner_;
    std::unique_ptr<base::Frame> frame_;

    DISALLOW_COPY_AND_ASSIGN(FrameGenerator);
};

DesktopSessionFake::FrameGenerator::FrameGenerator(std::shared_ptr<base::TaskRunner> task_runner)
    : task_runner_(std::move(task_runner)),
      frame_(base::FrameSimple::create(base::Size(kFrameWidth, kFrameHeight)))

{
    LOG(LS_INFO) << "FrameGenerator Ctor";
    DCHECK(task_runner_);

    if (!frame_)
    {
        LOG(LS_ERROR) << "Frame not created";
        return;
    }

    memset(frame_->frameData(), 0, static_cast<size_t>(frame_->stride() * frame_->size().height()));

    frame_->setCapturerType(static_cast<uint32_t>(base::ScreenCapturer::Type::FAKE));
    frame_->setDpi(base::Point(96, 96));
}

DesktopSessionFake::FrameGenerator::~FrameGenerator()
{
    LOG(LS_INFO) << "FrameGenerator Dtor";
}

void DesktopSessionFake::FrameGenerator::start(Delegate* delegate)
{
    LOG(LS_INFO) << "Start frame generator";

    delegate_ = delegate;
    DCHECK(delegate);

    generateFrame();
}

void DesktopSessionFake::FrameGenerator::stop()
{
    LOG(LS_INFO) << "Stop frame generator";
    delegate_ = nullptr;
}

void DesktopSessionFake::FrameGenerator::generateFrame()
{
    if (!frame_)
    {
        LOG(LS_ERROR) << "No frame generated";
        return;
    }

    base::Region* updated_region = frame_->updatedRegion();
    updated_region->clear();
    updated_region->addRect(base::Rect::makeWH(kFrameWidth, kFrameHeight));

    if (delegate_)
    {
        delegate_->onScreenCaptured(frame_.get(), nullptr);

        task_runner_->postDelayedTask(
            std::bind(&FrameGenerator::generateFrame, shared_from_this()),
            std::chrono::milliseconds(1000));
    }
    else
    {
        LOG(LS_INFO) << "Reset desktop frame";
        frame_.reset();
    }
}

DesktopSessionFake::DesktopSessionFake(
    std::shared_ptr<base::TaskRunner> task_runner, Delegate* delegate)
    : frame_generator_(std::make_shared<FrameGenerator>(std::move(task_runner))),
      delegate_(delegate)
{
    LOG(LS_INFO) << "DesktopSessionFake Ctor";
    DCHECK(delegate_);
}

DesktopSessionFake::~DesktopSessionFake()
{
    LOG(LS_INFO) << "DesktopSessionFake Dtor";
    frame_generator_->stop();
}

void DesktopSessionFake::start()
{
    LOG(LS_INFO) << "Start called for fake session";

    if (delegate_)
    {
        delegate_->onDesktopSessionStarted();
    }
    else
    {
        LOG(LS_WARNING) << "Invalid delegate";
    }
}

void DesktopSessionFake::stop()
{
    LOG(LS_INFO) << "Stop called for fake session";

    delegate_ = nullptr;
    frame_generator_->stop();
}

void DesktopSessionFake::control(proto::internal::Control::Action action)
{
    switch (action)
    {
        case proto::internal::Control::ENABLE:
        {
            LOG(LS_INFO) << "Control::ENABLE for fake session";
            frame_generator_->start(delegate_);
        }
        break;

        case proto::internal::Control::DISABLE:
        {
            LOG(LS_INFO) << "Control::DISABLE for fake session";
            frame_generator_->stop();
        }
            break;

        default:
            break;
    }
}

void DesktopSessionFake::configure(const Config& /* config */)
{
    // Nothing
}

void DesktopSessionFake::selectScreen(const proto::Screen& /* screen */)
{
    // Nothing
}

void DesktopSessionFake::captureScreen()
{
    frame_generator_->generateFrame();
}

void DesktopSessionFake::injectKeyEvent(const proto::KeyEvent& /* event */)
{
    // Nothing
}

void DesktopSessionFake::injectMouseEvent(const proto::MouseEvent& /* event */)
{
    // Nothing
}

void DesktopSessionFake::injectClipboardEvent(const proto::ClipboardEvent& /* event */)
{
    // Nothing
}

} // namespace host
