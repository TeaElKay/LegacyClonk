/*
 * LegacyClonk
 *
 * Copyright (c) RedWolf Design
 * Copyright (c) 2017-2021, The LegacyClonk Team and contributors
 *
 * Distributed under the terms of the ISC license; see accompanying file
 * "COPYING" for details.
 *
 * "Clonk" is a registered trademark of Matthes Bender, used with permission.
 * See accompanying file "TRADEMARK" for details.
 *
 * To redistribute this file separately, substitute the full license texts
 * for the above references.
 */

#pragma once

#include "StdScheduler.h"
#include "StdSync.h"

#include <any>

// Event types
enum C4InteractiveEventType
{
	Ev_None = 0,

	Ev_Log,
	Ev_LogSilent,
	Ev_LogFatal,

	Ev_FileChange,

	Ev_HTTP_Response,

	Ev_IRC_Message,

	Ev_Net_Conn,
	Ev_Net_Disconn,
	Ev_Net_Packet,

	Ev_Last = Ev_Net_Packet,
};

// Collects StdSchedulerProc objects and executes them in a separate thread
// Provides an event queue for the procs to communicate with the main thread
class C4InteractiveThread
{
public:
	C4InteractiveThread();
	~C4InteractiveThread();

	// Event callback interface
	class Callback
	{
	public:
		virtual void OnThreadEvent(C4InteractiveEventType eEvent, const std::any &eventData) = 0;
		virtual ~Callback() {}
	};

private:
	// the thread itself
	StdSchedulerThread Scheduler;

	// event queue (signals to main thread)
	struct Event
	{
		C4InteractiveEventType Type;
		std::any Data;
#ifndef NDEBUG
		int Time;
#endif
		Event *Next;
	};
	Event *pFirstEvent, *pLastEvent;
	CStdCSec EventPushCSec, EventPopCSec;

	// callback objects for events of special types
	Callback *pCallbacks[Ev_Last + 1];

public:
	// process management
	bool AddProc(StdSchedulerProc *pProc);
	void RemoveProc(StdSchedulerProc *pProc);

	// event queue
	bool PushEvent(C4InteractiveEventType eEventType, std::any data);
	void ProcessEvents(); // by main thread

	// special events
	bool ThreadLog(const char *szMessage);
	bool ThreadLogS(const char *szMessage);

	template<typename... Args>
	bool ThreadLogF(const char *szMessage, Args... args)
	{
		// send to main thread
		return PushEvent(Ev_Log, FormatString(szMessage, args...));
	}

	template<typename... Args>
	bool ThreadLogSF(const char *szMessage, Args... args)
	{
		// send to main thread
		return PushEvent(Ev_LogSilent, FormatString(szMessage, args...));
	}

	// event handlers
	void SetCallback(C4InteractiveEventType eEvent, Callback *pnNetworkCallback)
	{
		pCallbacks[eEvent] = pnNetworkCallback;
	}

	void ClearCallback(C4InteractiveEventType eEvent, Callback *pnNetworkCallback)
	{
		if (pCallbacks[eEvent] == pnNetworkCallback) pCallbacks[eEvent] = nullptr;
	}

private:
	bool PopEvent(C4InteractiveEventType *pEventType, std::any *data); // by main thread
};
#include <any>
#include <stdexcept>

// needed to support OS X < 10.14, because older libc++ does not have the necessary code for bad_any_cast
namespace std
{
	template<typename T, typename Any = any>
	T any_cast_(Any &&a)
	{
		if(auto &&result = any_cast<remove_reference_t<T>>(&a); result)
		{
			return *result;
		}

		throw runtime_error{"bad_any_cast"};
	}
}

#define any_cast any_cast_
