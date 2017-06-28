//
// PROJECT:         Aspia Remote Desktop
// FILE:            base/message_loop/message_loop_thread.cc
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "base/message_loop/message_loop_thread.h"
#include "base/scoped_com_initializer.h"
#include "base/logging.h"

namespace aspia {

MessageLoopThread::MessageLoopThread() :
    start_event_(WaitableEvent::ResetPolicy::MANUAL,
                 WaitableEvent::InitialState::NOT_SIGNALED)
{
    // Nothing
}

MessageLoopThread::~MessageLoopThread()
{
    Stop();
}

void MessageLoopThread::Start(MessageLoop::Type message_loop_type,
                              Delegate* delegate)
{
    std::lock_guard<std::mutex> lock(thread_lock_);

    DCHECK(!message_loop_);

    delegate_ = delegate;

    state_ = State::Starting;

    start_event_.Reset();
    thread_ = std::thread(&MessageLoopThread::ThreadMain,
                          this,
                          message_loop_type));
    start_event_.Wait();

    state_ = State::Started;

    DCHECK(message_loop_);
}

void MessageLoopThread::StopSoon()
{
    if (state_ == State::Stopping || !message_loop_)
        return;

    state_ = State::Stopping;

    message_loop_->PostTask(message_loop_->QuitClosure());
}

void MessageLoopThread::Stop()
{
    std::lock_guard<std::mutex> lock(thread_lock_);

    if (state_ == State::Stopped)
        return;

    StopSoon();

    if (message_loop_ && message_loop_->type() == MessageLoop::Type::TYPE_UI)
        PostThreadMessageW(thread_id_, WM_QUIT, 0, 0);

    // Wait for the thread to exit.
    if (thread_.joinable())
        thread_.join();

    // The thread should NULL message_loop_ on exit.
    DCHECK(!message_loop_);

    // The thread no longer needs to be joined.
    state_ = State::Stopped;

    delegate_ = nullptr;
}

void MessageLoopThread::ThreadMain(MessageLoop::Type message_loop_type)
{
    // The message loop for this thread.
    MessageLoop message_loop(message_loop_type);

    // Complete the initialization of our Thread object.
    message_loop_ = &message_loop;

    ScopedCOMInitializer com_initializer;
    CHECK(com_initializer.IsSucceeded());

    thread_id_ = GetCurrentThreadId();

    // Let the thread do extra initialization.
    // Let's do this before signaling we are started.
    if (delegate_)
        delegate_->OnBeforeThreadRunning();

    running_ = true;
    start_event_.Signal();

    message_loop_->Run();

    running_ = false;

    // Let the thread do extra cleanup.
    if (delegate_)
        delegate_->OnAfterThreadRunning();

    // We can't receive messages anymore.
    message_loop_ = nullptr;
}

} // namespace aspia
