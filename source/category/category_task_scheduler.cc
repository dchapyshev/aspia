//
// PROJECT:         Aspia
// FILE:            category/category_task_scheduler.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "base/strings/string_printf.h"
#include "base/strings/unicode.h"
#include "base/datetime.h"
#include "base/logging.h"
#include "base/scoped_bstr.h"
#include "base/scoped_comptr.h"
#include "base/version_helpers.h"
#include "category/category_task_scheduler.h"
#include "category/category_task_scheduler.pb.h"
#include "ui/resource.h"

#include <comdef.h>
#include <mstask.h>
#include <taskschd.h>

namespace aspia {

namespace {

void AddTaskActionV2(proto::TaskScheduler::Task* task, IAction* action)
{
    TASK_ACTION_TYPE action_type;

    HRESULT hr = action->get_Type(&action_type);
    if (FAILED(hr))
    {
        DLOG(LS_WARNING) << "action->get_Type failed: " << SystemErrorCodeToString(hr);
        return;
    }

    switch (action_type)
    {
        case TASK_ACTION_EXEC:
        {
            ScopedComPtr<IExecAction> exec_action;

            hr = action->QueryInterface(IID_IExecAction,
                                        reinterpret_cast<void**>(exec_action.Receive()));
            if (FAILED(hr))
            {
                DLOG(LS_WARNING) << "action->QueryInterface failed: "
                                 << SystemErrorCodeToString(hr);
                return;
            }

            ScopedBstr path;

            hr = exec_action->get_Path(path.Receive());
            if (FAILED(hr))
            {
                DLOG(LS_WARNING) << "exec_action->get_Path failed: "
                                 << SystemErrorCodeToString(hr);
                return;
            }

            ScopedBstr arguments;

            hr = exec_action->get_Arguments(arguments.Receive());
            if (FAILED(hr))
            {
                DLOG(LS_WARNING) << "exec_action->get_Arguments failed: "
                                 << SystemErrorCodeToString(hr);
                return;
            }

            ScopedBstr working_directory;

            hr = exec_action->get_WorkingDirectory(working_directory.Receive());
            if (FAILED(hr))
            {
                DLOG(LS_WARNING) << "exec_action->get_WorkingDirectory failed: "
                                 << SystemErrorCodeToString(hr);
                return;
            }

            proto::TaskScheduler::Action* item = task->add_action();

            item->set_path(UTF8fromUNICODE(path));
            item->set_arguments(UTF8fromUNICODE(arguments));
            item->set_working_directory(UTF8fromUNICODE(working_directory));
        }
        break;

        case TASK_ACTION_COM_HANDLER:
        case TASK_ACTION_SEND_EMAIL:
        case TASK_ACTION_SHOW_MESSAGE:
            break;

        default:
            break;
    }
}

bool AddTaskTriggerV2ForEvent(ITrigger* trigger, proto::TaskScheduler::Trigger* item)
{
    item->set_type(proto::TaskScheduler::Trigger::TYPE_EVENT);

    ScopedComPtr<IEventTrigger> event_trigger;

    HRESULT hr = trigger->QueryInterface(IID_IEventTrigger,
                                         reinterpret_cast<void**>(event_trigger.Receive()));
    if (FAILED(hr))
    {
        DLOG(LS_WARNING) << "trigger->QueryInterface failed: " << SystemErrorCodeToString(hr);
        return false;
    }

    ScopedBstr delay;

    hr = event_trigger->get_Delay(delay.Receive());
    if (FAILED(hr))
    {
        DLOG(LS_WARNING) << "event_trigger->get_Delay failed: " << SystemErrorCodeToString(hr);
        return false;
    }

    item->mutable_event()->set_delay(UTF8fromUNICODE(delay));

    ScopedComPtr<ITaskNamedValueCollection> named_value_collection;

    hr = event_trigger->get_ValueQueries(named_value_collection.Receive());
    if (FAILED(hr))
    {
        DLOG(LS_WARNING) << "event_trigger->get_ValueQueries failed: "
                         << SystemErrorCodeToString(hr);
        return false;
    }

    LONG count = 0;

    hr = named_value_collection->get_Count(&count);
    if (FAILED(hr))
    {
        DLOG(LS_WARNING) << "named_value_collection->get_Count failed: "
                         << SystemErrorCodeToString(hr);
        return false;
    }

    for (LONG i = 0; i < count; ++i)
    {
        ScopedComPtr<ITaskNamedValuePair> named_value_pair;

        hr = named_value_collection->get_Item(i + 1, named_value_pair.Receive());
        if (FAILED(hr))
        {
            DLOG(LS_WARNING) << "named_value_collection->get_Item failed: "
                             << SystemErrorCodeToString(hr);
            break;
        }

        ScopedBstr name;

        hr = named_value_pair->get_Name(name.Receive());
        if (FAILED(hr))
        {
            DLOG(LS_WARNING) << "named_value_pair->get_Name failed: "
                             << SystemErrorCodeToString(hr);
            break;
        }

        ScopedBstr value;

        hr = named_value_pair->get_Value(value.Receive());
        if (FAILED(hr))
        {
            DLOG(LS_WARNING) << "named_value_pair->get_Value failed: "
                             << SystemErrorCodeToString(hr);
            break;
        }

        proto::TaskScheduler::Trigger::Event::NamedValue* named_value =
            item->mutable_event()->add_named_value();

        named_value->set_name(UTF8fromUNICODE(name));
        named_value->set_value(UTF8fromUNICODE(value));
    }

    return true;
}

bool AddTaskTriggerV2ForDaily(ITrigger* trigger, proto::TaskScheduler::Trigger* item)
{
    item->set_type(proto::TaskScheduler::Trigger::TYPE_DAILY);

    ScopedComPtr<IDailyTrigger> daily_trigger;

    HRESULT hr = trigger->QueryInterface(IID_IDailyTrigger,
                                         reinterpret_cast<void**>(daily_trigger.Receive()));
    if (FAILED(hr))
    {
        DLOG(LS_WARNING) << "trigger->QueryInterface failed: " << SystemErrorCodeToString(hr);
        return false;
    }

    SHORT days_interval;

    hr = daily_trigger->get_DaysInterval(&days_interval);
    if (FAILED(hr))
    {
        DLOG(LS_WARNING) << "daily_trigger->get_DaysInterval failed: "
                         << SystemErrorCodeToString(hr);
        return false;
    }

    item->mutable_daily()->set_days_interval(days_interval);

    return true;
}

bool AddTaskTriggerV2ForWeekly(ITrigger* trigger, proto::TaskScheduler::Trigger* item)
{
    item->set_type(proto::TaskScheduler::Trigger::TYPE_WEEKLY);

    ScopedComPtr<IWeeklyTrigger> weekly_trigger;

    HRESULT hr = trigger->QueryInterface(IID_IWeeklyTrigger,
                                         reinterpret_cast<void**>(weekly_trigger.Receive()));
    if (FAILED(hr))
    {
        DLOG(LS_WARNING) << "trigger->QueryInterface failed: " << SystemErrorCodeToString(hr);
        return false;
    }

    SHORT days_of_week;

    hr = weekly_trigger->get_DaysOfWeek(&days_of_week);
    if (FAILED(hr))
    {
        DLOG(LS_WARNING) << "weekly_trigger->get_DaysOfWeek failed: "
                         << SystemErrorCodeToString(hr);
        return false;
    }

    SHORT weeks_interval;

    hr = weekly_trigger->get_WeeksInterval(&weeks_interval);
    if (FAILED(hr))
    {
        DLOG(LS_WARNING) << "weekly_trigger->get_DaysOfWeek failed: "
                         << SystemErrorCodeToString(hr);
        return false;
    }

    item->mutable_weekly()->set_days_of_week(days_of_week);
    item->mutable_weekly()->set_weeks_interval(weeks_interval);

    return true;
}

bool AddTaskTriggerV2ForMonthly(ITrigger* trigger, proto::TaskScheduler::Trigger* item)
{
    item->set_type(proto::TaskScheduler::Trigger::TYPE_MONTHLY);

    ScopedComPtr<IMonthlyTrigger> monthly_trigger;

    HRESULT hr = trigger->QueryInterface(IID_IMonthlyTrigger,
                                         reinterpret_cast<void**>(monthly_trigger.Receive()));
    if (FAILED(hr))
    {
        DLOG(LS_WARNING) << "trigger->QueryInterface failed: " << SystemErrorCodeToString(hr);
        return false;
    }

    LONG days_of_month;

    hr = monthly_trigger->get_DaysOfMonth(&days_of_month);
    if (FAILED(hr))
    {
        DLOG(LS_WARNING) << "monthly_trigger->get_DaysOfMonth failed: "
                         << SystemErrorCodeToString(hr);
        return false;
    }

    SHORT months_of_year;

    hr = monthly_trigger->get_MonthsOfYear(&months_of_year);
    if (FAILED(hr))
    {
        DLOG(LS_WARNING) << "monthly_trigger->get_MonthsOfYear failed: "
                         << SystemErrorCodeToString(hr);
        return false;
    }

    VARIANT_BOOL run_on_last_day_of_month;

    hr = monthly_trigger->get_RunOnLastDayOfMonth(&run_on_last_day_of_month);
    if (FAILED(hr))
    {
        DLOG(LS_WARNING) << "monthly_trigger->get_RunOnLastDayOfMonth failed: "
                         << SystemErrorCodeToString(hr);
        return false;
    }

    proto::TaskScheduler::Trigger::Monthly* monthly = item->mutable_monthly();

    monthly->set_days_of_month(days_of_month);
    monthly->set_months_of_year(months_of_year);
    monthly->set_last_day(run_on_last_day_of_month != 0);

    return true;
}

bool AddTaskTriggerV2ForMonthlyDow(ITrigger* trigger, proto::TaskScheduler::Trigger* item)
{
    item->set_type(proto::TaskScheduler::Trigger::TYPE_MONTHLYDOW);

    ScopedComPtr<IMonthlyDOWTrigger> monthly_dow_trigger;

    HRESULT hr = trigger->QueryInterface(IID_IMonthlyDOWTrigger,
                                         reinterpret_cast<void**>(monthly_dow_trigger.Receive()));
    if (FAILED(hr))
    {
        DLOG(LS_WARNING) << "trigger->QueryInterface failed: " << SystemErrorCodeToString(hr);
        return false;
    }

    SHORT days_of_week;

    hr = monthly_dow_trigger->get_DaysOfWeek(&days_of_week);
    if (FAILED(hr))
    {
        DLOG(LS_WARNING) << "monthly_trigger->get_DaysOfMonth failed: "
                         << SystemErrorCodeToString(hr);
        return false;
    }

    SHORT months_of_year;

    hr = monthly_dow_trigger->get_MonthsOfYear(&months_of_year);
    if (FAILED(hr))
    {
        DLOG(LS_WARNING) << "monthly_trigger->get_MonthsOfYear failed: "
                         << SystemErrorCodeToString(hr);
        return false;
    }

    VARIANT_BOOL run_on_last_week_of_month;

    hr = monthly_dow_trigger->get_RunOnLastWeekOfMonth(&run_on_last_week_of_month);
    if (FAILED(hr))
    {
        DLOG(LS_WARNING) << "monthly_trigger->get_RunOnLastWeekOfMonth failed: "
                         << SystemErrorCodeToString(hr);
        return false;
    }

    SHORT weeks_of_month;

    hr = monthly_dow_trigger->get_WeeksOfMonth(&weeks_of_month);
    if (FAILED(hr))
    {
        DLOG(LS_WARNING) << "monthly_dow_trigger->get_WeeksOfMonth failed: "
                         << SystemErrorCodeToString(hr);
        return false;
    }

    proto::TaskScheduler::Trigger::MonthlyDow* monthly_dow = item->mutable_monthly_dow();

    monthly_dow->set_days_of_week(days_of_week);
    monthly_dow->set_months_of_year(months_of_year);
    monthly_dow->set_last_week(run_on_last_week_of_month != 0);
    monthly_dow->set_weeks_of_month(weeks_of_month);

    return true;
}

bool AddTaskTriggerV2ForRegistration(ITrigger* trigger, proto::TaskScheduler::Trigger* item)
{
    item->set_type(proto::TaskScheduler::Trigger::TYPE_REGISTRATION);

    ScopedComPtr<IRegistrationTrigger> registration_trigger;

    HRESULT hr = trigger->QueryInterface(IID_IRegistrationTrigger,
                                         reinterpret_cast<void**>(registration_trigger.Receive()));
    if (FAILED(hr))
    {
        DLOG(LS_WARNING) << "trigger->QueryInterface failed: " << SystemErrorCodeToString(hr);
        return false;
    }

    ScopedBstr delay;

    hr = registration_trigger->get_Delay(delay.Receive());
    if (FAILED(hr))
    {
        DLOG(LS_WARNING) << "registration_trigger->get_Delay failed: "
                         << SystemErrorCodeToString(hr);
        return false;
    }

    item->mutable_registration()->set_delay(UTF8fromUNICODE(delay));

    return true;
}

bool AddTaskTriggerV2ForBoot(ITrigger* trigger, proto::TaskScheduler::Trigger* item)
{
    item->set_type(proto::TaskScheduler::Trigger::TYPE_BOOT);

    ScopedComPtr<IBootTrigger> boot_trigger;

    HRESULT hr = trigger->QueryInterface(IID_IBootTrigger,
                                         reinterpret_cast<void**>(boot_trigger.Receive()));
    if (FAILED(hr))
    {
        DLOG(LS_WARNING) << "trigger->QueryInterface failed: " << SystemErrorCodeToString(hr);
        return false;
    }

    ScopedBstr delay;

    hr = boot_trigger->get_Delay(delay.Receive());
    if (FAILED(hr))
    {
        DLOG(LS_WARNING) << "boot_trigger->get_Delay failed: " << SystemErrorCodeToString(hr);
        return false;
    }

    item->mutable_boot()->set_delay(UTF8fromUNICODE(delay));

    return true;
}

bool AddTaskTriggerV2ForLogon(ITrigger* trigger, proto::TaskScheduler::Trigger* item)
{
    item->set_type(proto::TaskScheduler::Trigger::TYPE_LOGON);

    ScopedComPtr<ILogonTrigger> logon_trigger;

    HRESULT hr = trigger->QueryInterface(IID_ILogonTrigger,
                                         reinterpret_cast<void**>(logon_trigger.Receive()));
    if (FAILED(hr))
    {
        DLOG(LS_WARNING) << "trigger->QueryInterface failed: " << SystemErrorCodeToString(hr);
        return false;
    }

    ScopedBstr user_id;

    hr = logon_trigger->get_UserId(user_id.Receive());
    if (FAILED(hr))
    {
        DLOG(LS_WARNING) << "logon_trigger->get_UserId failed: " << SystemErrorCodeToString(hr);
        return false;
    }

    ScopedBstr delay;

    hr = logon_trigger->get_Delay(delay.Receive());
    if (FAILED(hr))
    {
        DLOG(LS_WARNING) << "logon_trigger->get_Delay failed: " << SystemErrorCodeToString(hr);
        return false;
    }

    item->mutable_logon()->set_user_id(UTF8fromUNICODE(user_id));
    item->mutable_logon()->set_delay(UTF8fromUNICODE(delay));

    return true;
}

bool AddTaskTriggerV2ForSessionStateChange(ITrigger* trigger, proto::TaskScheduler::Trigger* item)
{
    item->set_type(proto::TaskScheduler::Trigger::TYPE_SESSION_STATE_CHANGE);

    ScopedComPtr<ISessionStateChangeTrigger> session_state_change_trigger;

    HRESULT hr = trigger->QueryInterface(IID_ISessionStateChangeTrigger,
        reinterpret_cast<void**>(session_state_change_trigger.Receive()));
    if (FAILED(hr))
    {
        DLOG(LS_WARNING) << "trigger->QueryInterface failed: " << SystemErrorCodeToString(hr);
        return false;
    }

    ScopedBstr user_id;

    hr = session_state_change_trigger->get_UserId(user_id.Receive());
    if (FAILED(hr))
    {
        DLOG(LS_WARNING) << "session_state_change_trigger->get_UserId failed: "
                         << SystemErrorCodeToString(hr);
        return false;
    }

    ScopedBstr delay;

    hr = session_state_change_trigger->get_Delay(delay.Receive());
    if (FAILED(hr))
    {
        DLOG(LS_WARNING) << "session_state_change_trigger->get_Delay failed: "
                         << SystemErrorCodeToString(hr);
        return false;
    }

    TASK_SESSION_STATE_CHANGE_TYPE change_type;

    hr = session_state_change_trigger->get_StateChange(&change_type);
    if (FAILED(hr))
    {
        DLOG(LS_WARNING) << "session_state_change_trigger->get_StateChange failed: "
                         << SystemErrorCodeToString(hr);
        return false;
    }

    switch (change_type)
    {
        case TASK_CONSOLE_CONNECT:
            item->mutable_session_state_change()->set_change_type(
                proto::TaskScheduler::Trigger::SessionStateChange::CHANGE_TYPE_CONSOLE_CONNECT);
            break;

        case TASK_CONSOLE_DISCONNECT:
            item->mutable_session_state_change()->set_change_type(
                proto::TaskScheduler::Trigger::SessionStateChange::CHANGE_TYPE_CONSOLE_DISCONNECT);
            break;

        case TASK_REMOTE_CONNECT:
            item->mutable_session_state_change()->set_change_type(
                proto::TaskScheduler::Trigger::SessionStateChange::CHANGE_TYPE_REMOTE_CONNECT);
            break;

        case TASK_REMOTE_DISCONNECT:
            item->mutable_session_state_change()->set_change_type(
                proto::TaskScheduler::Trigger::SessionStateChange::CHANGE_TYPE_REMOTE_DISCONNECT);
            break;

        case TASK_SESSION_LOCK:
            item->mutable_session_state_change()->set_change_type(
                proto::TaskScheduler::Trigger::SessionStateChange::CHANGE_TYPE_SESSION_LOCK);
            break;

        case TASK_SESSION_UNLOCK:
            item->mutable_session_state_change()->set_change_type(
                proto::TaskScheduler::Trigger::SessionStateChange::CHANGE_TYPE_SESSION_UNLOCK);
            break;

        default:
            DLOG(LS_WARNING) << "Unknown session change type: " << change_type;
            return false;
    }

    item->mutable_session_state_change()->set_user_id(UTF8fromUNICODE(user_id));
    item->mutable_session_state_change()->set_delay(UTF8fromUNICODE(delay));

    return true;
}

proto::TaskScheduler::Trigger* AddTaskTriggerV2(ITrigger* trigger)
{
    TASK_TRIGGER_TYPE2 trigger_type;

    HRESULT hr = trigger->get_Type(&trigger_type);
    if (FAILED(hr))
    {
        DLOG(LS_WARNING) << "trigger->get_Type failed: " << SystemErrorCodeToString(hr);
        return nullptr;
    }

    VARIANT_BOOL enabled;

    hr = trigger->get_Enabled(&enabled);
    if (FAILED(hr))
    {
        DLOG(LS_WARNING) << "trigger->get_Enabled failed: " << SystemErrorCodeToString(hr);
        return nullptr;
    }

    ScopedBstr start_boundary;

    hr = trigger->get_StartBoundary(start_boundary.Receive());
    if (FAILED(hr))
    {
        DLOG(LS_WARNING) << "trigger->get_StartBoundary failed: " << SystemErrorCodeToString(hr);
        return nullptr;
    }

    std::unique_ptr<proto::TaskScheduler::Trigger> item =
        std::make_unique<proto::TaskScheduler::Trigger>();

    item->set_start_time(UTF8fromUNICODE(start_boundary));
    item->set_enabled(enabled != 0);

    ScopedBstr end_boundary;

    hr = trigger->get_EndBoundary(end_boundary.Receive());
    if (SUCCEEDED(hr))
    {
        item->set_end_time(UTF8fromUNICODE(end_boundary));
    }

    ScopedBstr execution_time_limit;

    hr = trigger->get_ExecutionTimeLimit(execution_time_limit.Receive());
    if (SUCCEEDED(hr))
    {
        item->set_execution_time_limit(UTF8fromUNICODE(execution_time_limit));
    }

    ScopedComPtr<IRepetitionPattern> repetition_pattern;

    hr = trigger->get_Repetition(repetition_pattern.Receive());
    if (SUCCEEDED(hr))
    {
        ScopedBstr duration;

        hr = repetition_pattern->get_Duration(duration.Receive());
        if (SUCCEEDED(hr))
            item->mutable_repetition()->set_duration(UTF8fromUNICODE(duration));

        ScopedBstr interval;

        hr = repetition_pattern->get_Interval(interval.Receive());
        if (SUCCEEDED(hr))
            item->mutable_repetition()->set_interval(UTF8fromUNICODE(interval));

        VARIANT_BOOL stop_at_duration_end;

        hr = repetition_pattern->get_StopAtDurationEnd(&stop_at_duration_end);
        if (SUCCEEDED(hr))
            item->mutable_repetition()->set_stop_at_duration_end(stop_at_duration_end != 0);
    }

    switch (trigger_type)
    {
        case TASK_TRIGGER_EVENT:
            if (!AddTaskTriggerV2ForEvent(trigger, item.get()))
                return nullptr;
            break;

        case TASK_TRIGGER_TIME:
            item->set_type(proto::TaskScheduler::Trigger::TYPE_TIME);
            break;

        case TASK_TRIGGER_DAILY:
            if (!AddTaskTriggerV2ForDaily(trigger, item.get()))
                return nullptr;
            break;

        case TASK_TRIGGER_WEEKLY:
            if (!AddTaskTriggerV2ForWeekly(trigger, item.get()))
                return nullptr;
            break;

        case TASK_TRIGGER_MONTHLY:
            if (!AddTaskTriggerV2ForMonthly(trigger, item.get()))
                return nullptr;
            break;

        case TASK_TRIGGER_MONTHLYDOW:
            if (!AddTaskTriggerV2ForMonthlyDow(trigger, item.get()))
                return nullptr;
            break;

        case TASK_TRIGGER_IDLE:
            item->set_type(proto::TaskScheduler::Trigger::TYPE_IDLE);
            break;

        case TASK_TRIGGER_REGISTRATION:
            if (!AddTaskTriggerV2ForRegistration(trigger, item.get()))
                return nullptr;
            break;

        case TASK_TRIGGER_BOOT:
            if (!AddTaskTriggerV2ForBoot(trigger, item.get()))
                return nullptr;
            break;

        case TASK_TRIGGER_LOGON:
            if (!AddTaskTriggerV2ForLogon(trigger, item.get()))
                return nullptr;
            break;

        case TASK_TRIGGER_SESSION_STATE_CHANGE:
            if (!AddTaskTriggerV2ForSessionStateChange(trigger, item.get()))
                return nullptr;
            break;

        default:
            DLOG(LS_WARNING) << "Unknown trigger type: " << trigger_type;
            return nullptr;
    }

    return item.release();
}

void AddTaskV2(proto::TaskScheduler& message, IRegisteredTask* registered_task)
{
    ScopedBstr name;

    HRESULT hr = registered_task->get_Name(name.Receive());
    if (FAILED(hr))
    {
        DLOG(LS_WARNING) << "registered_task->get_Name failed: " << SystemErrorCodeToString(hr);
        return;
    }

    TASK_STATE task_state;

    hr = registered_task->get_State(&task_state);
    if (FAILED(hr))
    {
        DLOG(LS_WARNING) << "registered_task->get_State failed: " << SystemErrorCodeToString(hr);
        return;
    }

    DATE last_run_time;

    hr = registered_task->get_LastRunTime(&last_run_time);
    if (FAILED(hr))
    {
        DLOG(LS_WARNING) << "registered_task->get_LastRunTime failed: "
                         << SystemErrorCodeToString(hr);
        return;
    }

    DATE next_run_time;

    hr = registered_task->get_NextRunTime(&next_run_time);
    if (FAILED(hr))
    {
        DLOG(LS_WARNING) << "registered_task->get_NextRunTime failed: "
                         << SystemErrorCodeToString(hr);
        return;
    }

    LONG number_of_missed_runs;

    hr = registered_task->get_NumberOfMissedRuns(&number_of_missed_runs);
    if (FAILED(hr))
    {
        DLOG(LS_WARNING) << "registered_task->get_NumberOfMissedRuns failed: "
                         << SystemErrorCodeToString(hr);
        return;
    }

    LONG last_task_result;

    hr = registered_task->get_LastTaskResult(&last_task_result);
    if (FAILED(hr))
    {
        DLOG(LS_WARNING) << "registered_task->get_LastTaskResult failed: "
                         << SystemErrorCodeToString(hr);
        return;
    }

    proto::TaskScheduler::Task* item = message.add_task();

    item->set_task_name(UTF8fromUNICODE(name));
    item->set_last_run(VariantTimeToUnixTime(last_run_time));
    item->set_next_run(VariantTimeToUnixTime(next_run_time));
    item->set_number_of_missed_runs(number_of_missed_runs);
    item->set_last_task_result(last_task_result);

    switch (task_state)
    {
        case TASK_STATE_DISABLED:
            item->set_status(proto::TaskScheduler::Task::STATUS_DISABLED);
            break;

        case TASK_STATE_QUEUED:
            item->set_status(proto::TaskScheduler::Task::STATUS_QUEUED);
            break;

        case TASK_STATE_READY:
            item->set_status(proto::TaskScheduler::Task::STATUS_READY);
            break;

        case TASK_STATE_RUNNING:
            item->set_status(proto::TaskScheduler::Task::STATUS_RUNNING);
            break;

        default:
            break;
    }

    ScopedComPtr<ITaskDefinition> task_definition;

    hr = registered_task->get_Definition(task_definition.Receive());
    if (FAILED(hr))
    {
        DLOG(LS_WARNING) << "registered_task->get_Definition failed: "
                         << SystemErrorCodeToString(hr);
        return;
    }

    ScopedComPtr<IPrincipal> principal;

    hr = task_definition->get_Principal(principal.Receive());
    if (FAILED(hr))
    {
        DLOG(LS_WARNING) << "task_definition->get_Principal failed: "
                         << SystemErrorCodeToString(hr);
        return;
    }

    TASK_LOGON_TYPE logon_type;

    hr = principal->get_LogonType(&logon_type);
    if (FAILED(hr))
    {
        DLOG(LS_WARNING) << "principal->get_LogonType failed: " << SystemErrorCodeToString(hr);
        return;
    }

    switch (logon_type)
    {
        case TASK_LOGON_INTERACTIVE_TOKEN_OR_PASSWORD:
        case TASK_LOGON_SERVICE_ACCOUNT:
        case TASK_LOGON_PASSWORD:
        case TASK_LOGON_INTERACTIVE_TOKEN:
        {
            ScopedBstr user_id;

            hr = principal->get_UserId(user_id.Receive());
            if (FAILED(hr))
            {
                DLOG(LS_WARNING) << "principal->get_UserId failed: "
                                 << SystemErrorCodeToString(hr);
                return;
            }

            item->set_account_name(UTF8fromUNICODE(user_id));
        }
        break;

        case TASK_LOGON_GROUP:
        {
            ScopedBstr group_id;

            hr = principal->get_GroupId(group_id.Receive());
            if (FAILED(hr))
            {
                DLOG(LS_WARNING) << "principal->get_GroupId failed: "
                                 << SystemErrorCodeToString(hr);
                return;
            }

            item->set_account_name(UTF8fromUNICODE(group_id));
        }
        break;

        case TASK_LOGON_NONE:
        case TASK_LOGON_S4U:
        default:
            break;
    }

    ScopedComPtr<IRegistrationInfo> registration_info;

    hr = task_definition->get_RegistrationInfo(registration_info.Receive());
    if (SUCCEEDED(hr))
    {
        ScopedBstr author;

        hr = registration_info->get_Author(author.Receive());
        if (SUCCEEDED(hr))
        {
            item->set_creator(UTF8fromUNICODE(author));
        }
        else
        {
            DLOG(LS_WARNING) << "registration_info->get_Author failed: "
                             << SystemErrorCodeToString(hr);
        }

        ScopedBstr description;

        hr = registration_info->get_Description(description.Receive());
        if (SUCCEEDED(hr))
        {
            item->set_comment(UTF8fromUNICODE(description));
        }
        else
        {
            DLOG(LS_WARNING) << "registration_info->get_Description failed: "
                             << SystemErrorCodeToString(hr);
        }
    }
    else
    {
        DLOG(LS_WARNING) << "task_definition->get_RegistrationInfo failed: "
                         << SystemErrorCodeToString(hr);
    }

    ScopedComPtr<IActionCollection> action_collection;

    hr = task_definition->get_Actions(action_collection.Receive());
    if (SUCCEEDED(hr))
    {
        LONG action_count = 0;

        hr = action_collection->get_Count(&action_count);
        if (SUCCEEDED(hr))
        {
            for (LONG i = 0; i < action_count; ++i)
            {
                ScopedComPtr<IAction> action;

                hr = action_collection->get_Item(i + 1, action.Receive());
                if (SUCCEEDED(hr))
                {
                    AddTaskActionV2(item, action.get());
                }
                else
                {
                    DLOG(LS_WARNING) << "action_collection->get_Item failed: "
                                     << SystemErrorCodeToString(hr);
                }
            }
        }
        else
        {
            DLOG(LS_WARNING) << "action_collection->get_Count failed: "
                             << SystemErrorCodeToString(hr);
        }
    }
    else
    {
        DLOG(LS_WARNING) << "task_definition->get_Actions failed: "
                         << SystemErrorCodeToString(hr);
    }

    ScopedComPtr<ITriggerCollection> trigger_collection;

    hr = task_definition->get_Triggers(trigger_collection.Receive());
    if (SUCCEEDED(hr))
    {
        LONG trigger_count = 0;

        hr = trigger_collection->get_Count(&trigger_count);
        if (SUCCEEDED(hr))
        {
            for (LONG i = 0; i < trigger_count; ++i)
            {
                ScopedComPtr<ITrigger> trigger;

                hr = trigger_collection->get_Item(i + 1, trigger.Receive());
                if (SUCCEEDED(hr))
                {
                    std::unique_ptr<proto::TaskScheduler::Trigger> trigger_item(
                        AddTaskTriggerV2(trigger.get()));

                    if (trigger_item)
                        item->add_trigger()->CopyFrom(*trigger_item);
                }
                else
                {
                    DLOG(LS_WARNING) << "trigger_collection->get_Item failed: "
                                     << SystemErrorCodeToString(hr);
                }
            }
        }
        else
        {
            DLOG(LS_WARNING) << "trigger_collection->get_Count failed: "
                             << SystemErrorCodeToString(hr);
        }
    }
    else
    {
        DLOG(LS_WARNING) << "task_definition->get_Triggers failed: "
                         << SystemErrorCodeToString(hr);
    }
}

void AddTaskListV2(proto::TaskScheduler& message)
{
    ScopedComPtr<ITaskService> task_service;

    HRESULT hr = task_service.CreateInstance(CLSID_TaskScheduler);
    if (FAILED(hr))
    {
        DLOG(LS_WARNING) << "task_service.CreateInstance failed: "
                         << SystemErrorCodeToString(hr);
        return;
    }

    hr = task_service->Connect(_variant_t(), _variant_t(), _variant_t(), _variant_t());
    if (FAILED(hr))
    {
        DLOG(LS_WARNING) << "task_service->Connect failed: " << SystemErrorCodeToString(hr);
        return;
    }

    ScopedComPtr<ITaskFolder> task_folder;

    hr = task_service->GetFolder(_bstr_t(L"\\"), task_folder.Receive());
    if (FAILED(hr))
    {
        DLOG(LS_WARNING) << "task_service->GetFolder failed: " << SystemErrorCodeToString(hr);
        return;
    }

    ScopedComPtr<IRegisteredTaskCollection> task_collection;

    hr = task_folder->GetTasks(0, task_collection.Receive());
    if (FAILED(hr))
    {
        DLOG(LS_WARNING) << "task_folder->GetTasks failed: " << SystemErrorCodeToString(hr);
        return;
    }

    LONG task_count = 0;

    hr = task_collection->get_Count(&task_count);
    if (FAILED(hr))
    {
        DLOG(LS_WARNING) << "task_collection->get_Count failed: " << SystemErrorCodeToString(hr);
        return;
    }

    for (LONG i = 0; i < task_count; ++i)
    {
        ScopedComPtr<IRegisteredTask> registered_task;

        hr = task_collection->get_Item(_variant_t(i + 1), registered_task.Receive());
        if (FAILED(hr))
        {
            DLOG(LS_WARNING) << "task_collection->get_Item failed: "
                             << SystemErrorCodeToString(hr);
            continue;
        }

        AddTaskV2(message, registered_task.get());
    }
}

proto::TaskScheduler::Trigger* AddTaskTriggerV1(const TASK_TRIGGER& trigger)
{
    std::unique_ptr<proto::TaskScheduler::Trigger> item =
        std::make_unique<proto::TaskScheduler::Trigger>();

    item->set_start_time(StringPrintf("%u-%u-%uT%u:%u:00",
                                      trigger.wBeginYear,
                                      trigger.wBeginMonth,
                                      trigger.wBeginDay,
                                      trigger.wStartHour,
                                      trigger.wStartMinute));

    if (trigger.rgFlags & TASK_TRIGGER_FLAG_HAS_END_DATE)
    {
        item->set_end_time(StringPrintf("%u-%u-%uT%u:%u:00",
                                        trigger.wEndYear,
                                        trigger.wEndMonth,
                                        trigger.wEndDay,
                                        trigger.wStartHour,
                                        trigger.wStartMinute));
    }

    item->set_enabled((trigger.rgFlags & TASK_TRIGGER_FLAG_DISABLED) ? false : true);

    item->mutable_repetition()->set_duration(StringPrintf("PT%uM", trigger.MinutesDuration));
    item->mutable_repetition()->set_interval(StringPrintf("PT%uM", trigger.MinutesInterval));
    item->mutable_repetition()->set_stop_at_duration_end(
        (trigger.rgFlags & TASK_TRIGGER_FLAG_KILL_AT_DURATION_END) ? true : false);

    switch (trigger.TriggerType)
    {
        case TASK_TIME_TRIGGER_ONCE:
            item->set_type(proto::TaskScheduler::Trigger::TYPE_TIME);
            break;

        case TASK_TIME_TRIGGER_DAILY:
        {
            item->set_type(proto::TaskScheduler::Trigger::TYPE_DAILY);
            item->mutable_daily()->set_days_interval(trigger.Type.Daily.DaysInterval);
        }
        break;

        case TASK_TIME_TRIGGER_WEEKLY:
        {
            item->set_type(proto::TaskScheduler::Trigger::TYPE_WEEKLY);

            proto::TaskScheduler::Trigger::Weekly* weekly = item->mutable_weekly();

            weekly->set_days_of_week(trigger.Type.Weekly.rgfDaysOfTheWeek);
            weekly->set_weeks_interval(trigger.Type.Weekly.WeeksInterval);
        }
        break;

        case TASK_TIME_TRIGGER_MONTHLYDATE:
        {
            item->set_type(proto::TaskScheduler::Trigger::TYPE_MONTHLY);

            proto::TaskScheduler::Trigger::Monthly* monthly = item->mutable_monthly();

            monthly->set_months_of_year(trigger.Type.MonthlyDate.rgfMonths);
            monthly->set_days_of_month(trigger.Type.MonthlyDate.rgfDays);
        }
        break;

        case TASK_TIME_TRIGGER_MONTHLYDOW:
        {
            item->set_type(proto::TaskScheduler::Trigger::TYPE_MONTHLYDOW);

            proto::TaskScheduler::Trigger::MonthlyDow* monthly_dow = item->mutable_monthly_dow();

            monthly_dow->set_months_of_year(trigger.Type.MonthlyDOW.rgfMonths);
            monthly_dow->set_weeks_of_month(trigger.Type.MonthlyDOW.wWhichWeek);
            monthly_dow->set_days_of_week(trigger.Type.MonthlyDOW.rgfDaysOfTheWeek);
        }
        break;

        case TASK_EVENT_TRIGGER_ON_IDLE:
            item->set_type(proto::TaskScheduler::Trigger::TYPE_IDLE);
            break;

        case TASK_EVENT_TRIGGER_AT_SYSTEMSTART:
            item->set_type(proto::TaskScheduler::Trigger::TYPE_BOOT);
            break;

        case TASK_EVENT_TRIGGER_AT_LOGON:
            item->set_type(proto::TaskScheduler::Trigger::TYPE_LOGON);
            break;

        default:
            return nullptr;
    }

    return item.release();
}

void AddTaskListV1(proto::TaskScheduler& message)
{
    ScopedComPtr<ITaskScheduler> task_scheduler;

    HRESULT hr = task_scheduler.CreateInstance(CLSID_CTaskScheduler);
    if (FAILED(hr))
    {
        DLOG(LS_WARNING) << "task_scheduler.CreateInstance failed: "
                         << SystemErrorCodeToString(hr);
        return;
    }

    ScopedComPtr<IEnumWorkItems> enum_work_items;

    hr = task_scheduler->Enum(enum_work_items.Receive());
    if (FAILED(hr))
    {
        DLOG(LS_WARNING) << "task_scheduler->Enum failed: " << SystemErrorCodeToString(hr);
        return;
    }

    for (;;)
    {
        LPWSTR* name;

        hr = enum_work_items->Next(1, &name, nullptr);
        if (FAILED(hr))
        {
            DLOG(LS_WARNING) << "enum_work_items->Next failed: " << SystemErrorCodeToString(hr);
            break;
        }

        if (!name || !*name)
            break;

        ScopedComPtr<ITask> task;

        hr = task_scheduler->Activate(*name, IID_ITask, reinterpret_cast<IUnknown**>(task.Receive()));
        if (FAILED(hr))
        {
            DLOG(LS_WARNING) << "task_scheduler->Activate failed: " << SystemErrorCodeToString(hr);
            break;
        }

        HRESULT status;
        hr = task->GetStatus(&status);
        if (FAILED(hr))
        {
            DLOG(LS_WARNING) << "task->GetStatus failed: " << SystemErrorCodeToString(hr);
            break;
        }

        proto::TaskScheduler::Task* item = message.add_task();

        item->set_task_name(UTF8fromUNICODE(*name));

        switch (status)
        {
            case SCHED_S_TASK_READY:
                item->set_status(proto::TaskScheduler::Task::STATUS_READY);
                break;

            case SCHED_S_TASK_RUNNING:
                item->set_status(proto::TaskScheduler::Task::STATUS_RUNNING);
                break;

            case SCHED_S_TASK_DISABLED:
                item->set_status(proto::TaskScheduler::Task::STATUS_DISABLED);
                break;
        }

        LPWSTR account_name = nullptr;
        hr = task->GetAccountInformation(&account_name);
        if (SUCCEEDED(hr))
        {
            item->set_account_name(UTF8fromUNICODE(account_name));
            CoTaskMemFree(account_name);
        }

        LPWSTR creator = nullptr;
        hr = task->GetCreator(&creator);
        if (SUCCEEDED(hr))
        {
            item->set_creator(UTF8fromUNICODE(creator));
            CoTaskMemFree(creator);
        }

        LPWSTR comment = nullptr;
        hr = task->GetComment(&comment);
        if (SUCCEEDED(hr))
        {
            item->set_comment(UTF8fromUNICODE(comment));
            CoTaskMemFree(comment);
        }

        SYSTEMTIME last_run_time;
        memset(&last_run_time, 0, sizeof(last_run_time));

        hr = task->GetMostRecentRunTime(&last_run_time);
        if (SUCCEEDED(hr))
        {
            item->set_last_run(SystemTimeToUnixTime(last_run_time));
        }

        SYSTEMTIME next_run_time;
        memset(&next_run_time, 0, sizeof(next_run_time));

        hr = task->GetNextRunTime(&next_run_time);
        if (SUCCEEDED(hr))
        {
            item->set_next_run(SystemTimeToUnixTime(next_run_time));
        }

        DWORD last_task_result = 0;
        hr = task->GetExitCode(&last_task_result);
        if (SUCCEEDED(hr))
        {
            item->set_last_task_result(last_task_result);
        }

        proto::TaskScheduler::Action* action = item->add_action();

        LPWSTR path = nullptr;
        hr = task->GetApplicationName(&path);
        if (SUCCEEDED(hr))
        {
            action->set_path(UTF8fromUNICODE(path));
            CoTaskMemFree(path);
        }

        LPWSTR arguments = nullptr;
        hr = task->GetParameters(&arguments);
        if (SUCCEEDED(hr))
        {
            action->set_arguments(UTF8fromUNICODE(arguments));
            CoTaskMemFree(arguments);
        }

        LPWSTR working_directory = nullptr;
        hr = task->GetWorkingDirectory(&working_directory);
        if (SUCCEEDED(hr))
        {
            action->set_working_directory(UTF8fromUNICODE(working_directory));
            CoTaskMemFree(working_directory);
        }

        WORD trigger_count = 0;
        hr = task->GetTriggerCount(&trigger_count);
        if (SUCCEEDED(hr))
        {
            for (WORD i = 0; i < trigger_count; ++i)
            {
                ScopedComPtr<ITaskTrigger> task_trigger;

                hr = task->GetTrigger(i, task_trigger.Receive());
                if (FAILED(hr))
                    break;

                TASK_TRIGGER trigger;
                memset(&trigger, 0, sizeof(trigger));

                trigger.cbTriggerSize = sizeof(trigger);

                hr = task_trigger->GetTrigger(&trigger);
                if (FAILED(hr))
                    break;

                std::unique_ptr<proto::TaskScheduler::Trigger> trigger_item(
                    AddTaskTriggerV1(trigger));

                if (trigger_item)
                    item->add_trigger()->CopyFrom(*trigger_item);
            }
        }

        CoTaskMemFree(*name);
        CoTaskMemFree(name);
    }
}

const char* StatusToString(proto::TaskScheduler::Task::Status value)
{
    switch (value)
    {
        case proto::TaskScheduler::Task::STATUS_DISABLED:
            return "Disabled";

        case proto::TaskScheduler::Task::STATUS_QUEUED:
            return "Queued";

        case proto::TaskScheduler::Task::STATUS_READY:
            return "Ready";

        case proto::TaskScheduler::Task::STATUS_RUNNING:
            return "Running";

        default:
            return "Unknown";
    }
}

const char* TriggerTypeToString(proto::TaskScheduler::Trigger::Type value)
{
    switch (value)
    {
        case proto::TaskScheduler::Trigger::TYPE_EVENT:
            return "At Event";

        case proto::TaskScheduler::Trigger::TYPE_TIME:
            return "One Time";

        case proto::TaskScheduler::Trigger::TYPE_DAILY:
            return "Daily";

        case proto::TaskScheduler::Trigger::TYPE_WEEKLY:
            return "Weekly";

        case proto::TaskScheduler::Trigger::TYPE_MONTHLY:
            return "Monthly";

        case proto::TaskScheduler::Trigger::TYPE_MONTHLYDOW:
            return "Monthly (Day Of Week)";

        case proto::TaskScheduler::Trigger::TYPE_IDLE:
            return "Idle";

        case proto::TaskScheduler::Trigger::TYPE_REGISTRATION:
            return "Task Registration";

        case proto::TaskScheduler::Trigger::TYPE_BOOT:
            return "At Boot";

        case proto::TaskScheduler::Trigger::TYPE_LOGON:
            return "At Log On";

        case proto::TaskScheduler::Trigger::TYPE_SESSION_STATE_CHANGE:
            return "Session State Change";

        default:
            return "Unknown";
    }
}

const char* SessionChangeTypeToString(
    proto::TaskScheduler::Trigger::SessionStateChange::ChangeType value)
{
    switch (value)
    {
        case proto::TaskScheduler::Trigger::SessionStateChange::CHANGE_TYPE_CONSOLE_CONNECT:
            return "Console Connect";

        case proto::TaskScheduler::Trigger::SessionStateChange::CHANGE_TYPE_CONSOLE_DISCONNECT:
            return "Console Disconnect";

        case proto::TaskScheduler::Trigger::SessionStateChange::CHANGE_TYPE_REMOTE_CONNECT:
            return "Remote Connect";

        case proto::TaskScheduler::Trigger::SessionStateChange::CHANGE_TYPE_REMOTE_DISCONNECT:
            return "Remote Disconnect";

        case proto::TaskScheduler::Trigger::SessionStateChange::CHANGE_TYPE_SESSION_LOCK:
            return "Session Lock";

        case proto::TaskScheduler::Trigger::SessionStateChange::CHANGE_TYPE_SESSION_UNLOCK:
            return "Session Unlock";

        default:
            return "Unknown";
    }
}

void AddDaysOfMonth(Group& parent_group, uint32_t flags, bool last_day_of_month)
{
    Group group = parent_group.AddGroup("Days Of Month");

    auto add_day = [&](const char* name, proto::TaskScheduler::Trigger::DayOfMonth value)
    {
        group.AddParam(name, Value::Bool(flags & value));
    };

    add_day("1", proto::TaskScheduler::Trigger::DAY_OF_MONTH_1);
    add_day("2", proto::TaskScheduler::Trigger::DAY_OF_MONTH_2);
    add_day("3", proto::TaskScheduler::Trigger::DAY_OF_MONTH_3);
    add_day("4", proto::TaskScheduler::Trigger::DAY_OF_MONTH_4);
    add_day("5", proto::TaskScheduler::Trigger::DAY_OF_MONTH_5);
    add_day("6", proto::TaskScheduler::Trigger::DAY_OF_MONTH_6);
    add_day("7", proto::TaskScheduler::Trigger::DAY_OF_MONTH_7);
    add_day("8", proto::TaskScheduler::Trigger::DAY_OF_MONTH_8);
    add_day("9", proto::TaskScheduler::Trigger::DAY_OF_MONTH_9);
    add_day("10", proto::TaskScheduler::Trigger::DAY_OF_MONTH_10);
    add_day("11", proto::TaskScheduler::Trigger::DAY_OF_MONTH_11);
    add_day("12", proto::TaskScheduler::Trigger::DAY_OF_MONTH_12);
    add_day("13", proto::TaskScheduler::Trigger::DAY_OF_MONTH_13);
    add_day("14", proto::TaskScheduler::Trigger::DAY_OF_MONTH_14);
    add_day("15", proto::TaskScheduler::Trigger::DAY_OF_MONTH_15);
    add_day("16", proto::TaskScheduler::Trigger::DAY_OF_MONTH_16);
    add_day("17", proto::TaskScheduler::Trigger::DAY_OF_MONTH_17);
    add_day("18", proto::TaskScheduler::Trigger::DAY_OF_MONTH_18);
    add_day("19", proto::TaskScheduler::Trigger::DAY_OF_MONTH_19);
    add_day("20", proto::TaskScheduler::Trigger::DAY_OF_MONTH_20);
    add_day("21", proto::TaskScheduler::Trigger::DAY_OF_MONTH_21);
    add_day("22", proto::TaskScheduler::Trigger::DAY_OF_MONTH_22);
    add_day("23", proto::TaskScheduler::Trigger::DAY_OF_MONTH_23);
    add_day("24", proto::TaskScheduler::Trigger::DAY_OF_MONTH_24);
    add_day("25", proto::TaskScheduler::Trigger::DAY_OF_MONTH_25);
    add_day("26", proto::TaskScheduler::Trigger::DAY_OF_MONTH_26);
    add_day("27", proto::TaskScheduler::Trigger::DAY_OF_MONTH_27);
    add_day("28", proto::TaskScheduler::Trigger::DAY_OF_MONTH_28);
    add_day("29", proto::TaskScheduler::Trigger::DAY_OF_MONTH_29);
    add_day("30", proto::TaskScheduler::Trigger::DAY_OF_MONTH_30);
    add_day("31", proto::TaskScheduler::Trigger::DAY_OF_MONTH_31);

    group.AddParam("Last", Value::Bool(last_day_of_month));
}

void AddDaysOfWeek(Group& parent_group, uint32_t flags)
{
    Group group = parent_group.AddGroup("Days Of Week");

    auto add_day = [&](const char* name, proto::TaskScheduler::Trigger::DayOfWeek value)
    {
        group.AddParam(name, Value::Bool(flags & value));
    };

    add_day("Sunday", proto::TaskScheduler::Trigger::DAY_OF_WEEK_SUNDAY);
    add_day("Monday", proto::TaskScheduler::Trigger::DAY_OF_WEEK_MONDAY);
    add_day("Tuesday", proto::TaskScheduler::Trigger::DAY_OF_WEEK_TUESDAY);
    add_day("Wednesday", proto::TaskScheduler::Trigger::DAY_OF_WEEK_WEDNESDAY);
    add_day("Thursday", proto::TaskScheduler::Trigger::DAY_OF_WEEK_THURSDAY);
    add_day("Friday", proto::TaskScheduler::Trigger::DAY_OF_WEEK_FRIDAY);
    add_day("Saturday", proto::TaskScheduler::Trigger::DAY_OF_WEEK_SATURDAY);
}

void AddMonthsOfYear(Group& parent_group, uint32_t flags)
{
    Group group = parent_group.AddGroup("Month Of Year");

    auto add_month = [&](const char* name, proto::TaskScheduler::Trigger::Month value)
    {
        group.AddParam(name, Value::Bool(flags & value));
    };

    add_month("January", proto::TaskScheduler::Trigger::MONTH_JANUARY);
    add_month("February", proto::TaskScheduler::Trigger::MONTH_FEBRUARY);
    add_month("March", proto::TaskScheduler::Trigger::MONTH_MARCH);
    add_month("April", proto::TaskScheduler::Trigger::MONTH_APRIL);
    add_month("May", proto::TaskScheduler::Trigger::MONTH_MAY);
    add_month("June", proto::TaskScheduler::Trigger::MONTH_JUNE);
    add_month("July", proto::TaskScheduler::Trigger::MONTH_JULY);
    add_month("August", proto::TaskScheduler::Trigger::MONTH_AUGUST);
    add_month("September", proto::TaskScheduler::Trigger::MONTH_SEPTEMBER);
    add_month("October", proto::TaskScheduler::Trigger::MONTH_OCTOBER);
    add_month("November", proto::TaskScheduler::Trigger::MONTH_NOVEMBER);
    add_month("December", proto::TaskScheduler::Trigger::MONTH_DECEMBER);
}

void AddWeeksOfMonth(Group& parent_group, uint32_t flags, bool last_week)
{
    Group group = parent_group.AddGroup("Weeks Of Month");

    auto add_week = [&](const char* name, proto::TaskScheduler::Trigger::WeekOfMonth value)
    {
        group.AddParam(name, Value::Bool(flags & value));
    };

    add_week("First", proto::TaskScheduler::Trigger::WEEK_OF_MONTH_FIRST);
    add_week("Second", proto::TaskScheduler::Trigger::WEEK_OF_MONTH_SECOND);
    add_week("Third", proto::TaskScheduler::Trigger::WEEK_OF_MONTH_THIRD);
    add_week("Fourth", proto::TaskScheduler::Trigger::WEEK_OF_MONTH_FOURTH);

    group.AddParam("Last", Value::Bool(last_week));
}

void AddTrigger(Group& group, const proto::TaskScheduler::Trigger& trigger)
{
    group.AddParam("Type", Value::String(TriggerTypeToString(trigger.type())));
    group.AddParam("Enabled", Value::Bool(trigger.enabled()));

    {
        Group repetition_group = group.AddGroup("Repetition");
        const proto::TaskScheduler::Trigger::Repetition& repetition = trigger.repetition();

        if (repetition.duration().empty())
            repetition_group.AddParam("Duration", Value::String("<None>"));
        else
            repetition_group.AddParam("Duration", Value::String(repetition.duration()));

        if (repetition.interval().empty())
            repetition_group.AddParam("Interval", Value::String("<None>"));
        else
            repetition_group.AddParam("Interval", Value::String(repetition.interval()));

        repetition_group.AddParam("Stop at Duration End",
                                  Value::Bool(repetition.stop_at_duration_end()));
    }

    if (!trigger.start_time().empty())
        group.AddParam("Start", Value::String(trigger.start_time()));

    if (trigger.end_time().empty())
        group.AddParam("Expire", Value::String("<None>"));
    else
        group.AddParam("Expire", Value::String(trigger.end_time()));

    if (trigger.execution_time_limit().empty())
        group.AddParam("Execution Time Limit", Value::String("<None>"));
    else
        group.AddParam("Execution Time Limit", Value::String(trigger.execution_time_limit()));

    switch (trigger.type())
    {
        case proto::TaskScheduler::Trigger::TYPE_EVENT:
        {
            const proto::TaskScheduler::Trigger::Event& event = trigger.event();

            if (event.delay().empty())
                group.AddParam("Delay", Value::String("<None>"));
            else
                group.AddParam("Delay", Value::String(event.delay()));

            if (event.named_value_size())
            {
                Group named_value_group = group.AddGroup("Log / Source");

                for (int i = 0; i < event.named_value_size(); ++i)
                {
                    named_value_group.AddParam(
                        event.named_value(i).name(),
                        Value::String(event.named_value(i).value()));
                }
            }
        }
        break;

        case proto::TaskScheduler::Trigger::TYPE_TIME:
            // Nothing
            break;

        case proto::TaskScheduler::Trigger::TYPE_DAILY:
        {
            group.AddParam("Days Interval", Value::Number(trigger.daily().days_interval()));
        }
        break;

        case proto::TaskScheduler::Trigger::TYPE_WEEKLY:
        {
            const proto::TaskScheduler::Trigger::Weekly& weekly = trigger.weekly();

            group.AddParam("Weeks Interval", Value::Number(weekly.weeks_interval()));
            AddDaysOfWeek(group, weekly.days_of_week());
        }
        break;

        case proto::TaskScheduler::Trigger::TYPE_MONTHLY:
        {
            const proto::TaskScheduler::Trigger::Monthly& monthly = trigger.monthly();

            AddDaysOfMonth(group, monthly.days_of_month(), monthly.last_day());
            AddMonthsOfYear(group, monthly.months_of_year());
        }
        break;

        case proto::TaskScheduler::Trigger::TYPE_MONTHLYDOW:
        {
            const proto::TaskScheduler::Trigger::MonthlyDow& monthly_dow = trigger.monthly_dow();

            AddDaysOfWeek(group, monthly_dow.days_of_week());
            AddMonthsOfYear(group, monthly_dow.months_of_year());
            AddWeeksOfMonth(group, monthly_dow.weeks_of_month(), monthly_dow.last_week());
        }
        break;

        case proto::TaskScheduler::Trigger::TYPE_IDLE:
            // Nothing
            break;

        case proto::TaskScheduler::Trigger::TYPE_REGISTRATION:
        {
            if (trigger.registration().delay().empty())
                group.AddParam("Delay", Value::String("<None>"));
            else
                group.AddParam("Delay", Value::String(trigger.registration().delay()));
        }
        break;

        case proto::TaskScheduler::Trigger::TYPE_BOOT:
        {
            if (trigger.boot().delay().empty())
                group.AddParam("Delay", Value::String("<None>"));
            else
                group.AddParam("Delay", Value::String(trigger.boot().delay()));
        }
        break;

        case proto::TaskScheduler::Trigger::TYPE_LOGON:
        {
            const proto::TaskScheduler::Trigger::Logon& logon = trigger.logon();

            if (logon.user_id().empty())
                group.AddParam("User", Value::String("<Any>"));
            else
                group.AddParam("User", Value::String(logon.user_id()));

            if (logon.delay().empty())
                group.AddParam("Delay", Value::String("<None>"));
            else
                group.AddParam("Delay", Value::String(logon.delay()));
        }
        break;

        case proto::TaskScheduler::Trigger::TYPE_SESSION_STATE_CHANGE:
        {
            const proto::TaskScheduler::Trigger::SessionStateChange& session_state_change =
                trigger.session_state_change();

            group.AddParam("Change Type", Value::String(
                SessionChangeTypeToString(session_state_change.change_type())));

            if (session_state_change.user_id().empty())
                group.AddParam("User", Value::String("<Any>"));
            else
                group.AddParam("User", Value::String(session_state_change.user_id()));

            if (session_state_change.delay().empty())
                group.AddParam("Delay", Value::String("<None>"));
            else
                group.AddParam("Delay", Value::String(session_state_change.delay()));
        }
        break;
    }
}

} // namespace

CategoryTaskScheduler::CategoryTaskScheduler()
    : CategoryInfo(Type::INFO_PARAM_VALUE)
{
    // Nothing
}

const char* CategoryTaskScheduler::Name() const
{
    return "Task Scheduler";
}

Category::IconId CategoryTaskScheduler::Icon() const
{
    return IDI_CLOCK;
}

const char* CategoryTaskScheduler::Guid() const
{
    return "{1B27C27F-847E-47CC-92DF-6B8F5CB4827A}";
}

void CategoryTaskScheduler::Parse(Table& table, const std::string& data)
{
    proto::TaskScheduler message;

    if (!message.ParseFromString(data))
        return;

    table.AddColumns(ColumnList::Create()
                     .AddColumn("Parameter", 300)
                     .AddColumn("Value", 260));

    for (int index = 0; index < message.task_size(); ++index)
    {
        const proto::TaskScheduler::Task& task = message.task(index);

        Group group = table.AddGroup(task.task_name());

        group.AddParam("Status", Value::String(StatusToString(task.status())));
        group.AddParam("Comment", Value::String(task.comment()));
        group.AddParam("Account Name", Value::String(task.account_name()));
        group.AddParam("Creator", Value::String(task.creator()));
        group.AddParam("Last Run", Value::String(TimeToString(task.last_run())));
        group.AddParam("Next Run", Value::String(TimeToString(task.next_run())));
        group.AddParam("Number Of Missed Runs", Value::Number(task.number_of_missed_runs()));
        group.AddParam("Last Task Result", Value::String(StringPrintf("%08Xh", task.last_task_result())));

        if (task.action_size())
        {
            Group actions_group = group.AddGroup("Actions");

            for (int i = 0; i < task.action_size(); ++i)
            {
                const proto::TaskScheduler::Action& action = task.action(i);

                Group action_group = actions_group.AddGroup(StringPrintf("Action #%d", i + 1));

                action_group.AddParam("Program/Script", Value::String(action.path()));
                action_group.AddParam("Arguments", Value::String(action.arguments()));
                action_group.AddParam("Working Directory", Value::String(action.working_directory()));
            }
        }

        if (task.trigger_size())
        {
            Group triggers_group = group.AddGroup("Triggers");

            for (int i = 0; i < task.trigger_size(); ++i)
            {
                Group trigger_group = triggers_group.AddGroup(StringPrintf("Trigger #%d", i + 1));
                AddTrigger(trigger_group, task.trigger(i));
            }
        }
    }
}

std::string CategoryTaskScheduler::Serialize()
{
    proto::TaskScheduler message;

    if (IsWindowsVistaOrGreater())
    {
        // Uses Task Scheduler 2.0 API.
        AddTaskListV2(message);
    }
    else
    {
        // Uses Task Scheduler 1.0 API.
        AddTaskListV1(message);
    }

    return message.SerializeAsString();
}

} // namespace aspia
