//
// PROJECT:         Aspia Remote Desktop
// FILE:            base/object_watcher.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_BASE__OBJECT_WATCHER_H
#define _ASPIA_BASE__OBJECT_WATCHER_H

#include <chrono>
#include <functional>

namespace aspia {

// A class that provides a means to asynchronously wait for a Windows object to
// become signaled.  It is an abstraction around RegisterWaitForSingleObject
// that provides a notification callback, OnObjectSignaled, that runs back on
// the origin sequence (i.e., the sequence that called StartWatching).
//
// This class acts like a smart pointer such that when it goes out-of-scope,
// UnregisterWaitEx is automatically called, and any in-flight notification is
// suppressed.
//
// Typical usage:
//
//   class MyClass : public base::win::ObjectWatcher::Delegate {
//    public:
//     void DoStuffWhenSignaled(HANDLE object) {
//       watcher_.StartWatchingOnce(object, this);
//     }
//     void OnObjectSignaled(HANDLE object) override {
//       // OK, time to do stuff!
//     }
//    private:
//     base::win::ObjectWatcher watcher_;
//   };
//
// In the above example, MyClass wants to "do stuff" when object becomes
// signaled.  ObjectWatcher makes this task easy.  When MyClass goes out of
// scope, the watcher_ will be destroyed, and there is no need to worry about
// OnObjectSignaled being called on a deleted MyClass pointer.  Easy!
// If the object is already signaled before being watched, OnObjectSignaled is
// still called after (but not necessarily immediately after) watch is started.
//
class ObjectWatcher
{
public:
    ObjectWatcher();
    ~ObjectWatcher();

    class Delegate
    {
    public:
        virtual ~Delegate() {}

        // Called from the sequence that started the watch when a signaled object is
        // detected. To continue watching the object, StartWatching must be called
        // again.
        virtual void OnObjectSignaled(HANDLE object) = 0;

        virtual void OnObjectTimeout(HANDLE object) { }
    };

    // When the object is signaled, the given delegate is notified on the sequence
    // where StartWatching is called. The ObjectWatcher is not responsible for
    // deleting the delegate.
    // Returns whether watching was successfully initiated.
    bool StartWatching(HANDLE object, Delegate* delegate);

    bool StartTimedWatching(HANDLE object,
                            const std::chrono::milliseconds& timeout,
                            Delegate* delegate);

    // Stops watching.  Does nothing if the watch has already completed.  If the
    // watch is still active, then it is canceled, and the associated delegate is
    // not notified.
    //
    // Returns true if the watch was canceled.  Otherwise, false is returned.
    bool StopWatching();

    // Returns true if currently watching an object.
    bool IsWatching() const;

    // Returns the handle of the object being watched.
    HANDLE GetWatchedObject() const;

private:
    bool StartWatchingInternal(HANDLE object, DWORD timeout, Delegate* delegate);

    // Called on a background thread when done waiting.
    static void NTAPI DoneWaiting(PVOID context, BOOLEAN timed_out);

    Delegate* delegate_;
    HANDLE wait_object_;
    HANDLE object_;
};

} // namespace aspia

#endif // _ASPIA_BASE__OBJECT_WATCHER_H
