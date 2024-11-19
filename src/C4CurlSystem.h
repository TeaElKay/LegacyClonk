/*
 * LegacyClonk
 *
 * Copyright (c) 2023, The LegacyClonk Team and contributors
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

#include "C4Attributes.h"
#include "C4Coroutine.h"
#include "C4NetIO.h"
#include "StdSync.h"

#include <expected>
#include <memory>
#include <variant>

#include <curl/curlver.h>

#if CURL_AT_LEAST_VERSION(8, 11, 0)
using CURLM = void;
using CURL = void;
#else
using CURLM = struct Curl_multi;
using CURL = struct Curl_easy;
#endif

using curl_socket_t = SOCKET;
using CURLU = struct Curl_URL;

template<>
struct std::hash<std::pair<CURL *, SOCKET>>
{
	std::size_t operator()(const std::pair<CURL *, SOCKET> pair) const
	{
		return std::hash<CURL *>{}(pair.first) ^ std::hash<SOCKET>{}(pair.second);
	}
};

class C4CurlSystem
{
private:
	struct CURLMultiDeleter
	{
		void operator()(CURLM *multi);
	};

	struct CURLEasyDeleter
	{
		void operator()(CURL *easy);
	};

	using TaskType = C4Task::Task<void, C4Task::TaskTraitsHotWaitOnDestruction, C4Task::PromiseTraitsTerminateOnException>;

#ifdef _WIN32
	using WaitReturnType = bool;
#else
	using WaitReturnType = std::vector<pollfd>;
#endif

public:
	using MultiHandle = std::unique_ptr<CURLM, CURLMultiDeleter>;
	using EasyHandle = std::unique_ptr<CURL, CURLEasyDeleter>;

	class AddedEasyHandle
	{
	public:
		AddedEasyHandle(C4CurlSystem &system, EasyHandle &&easyHandle);
		~AddedEasyHandle();

		AddedEasyHandle(const AddedEasyHandle &) = delete;
		AddedEasyHandle &operator=(const AddedEasyHandle &) = delete;

		AddedEasyHandle(AddedEasyHandle &&) = default;
		AddedEasyHandle &operator=(AddedEasyHandle &&) = default;

	public:
		auto get() const { return easyHandle.get(); }

	private:
		std::reference_wrapper<C4CurlSystem> system;
		EasyHandle easyHandle;
	};

	class Exception : public std::runtime_error
	{
	public:
		using runtime_error::runtime_error;
	};

private:
	class GlobalInit
	{
	public:
		GlobalInit();
		~GlobalInit();
	};

	class MultiHandleWithCallbacks
	{
	public:
		using SocketFunction = int(CURL *, curl_socket_t, int, void *) noexcept;
		using TimerFunction = int(CURLM *, long, void *) noexcept;

	public:
		MultiHandleWithCallbacks(MultiHandle multiHandle, C4CurlSystem &system, SocketFunction *socketFunction, TimerFunction *timerFunction);
		~MultiHandleWithCallbacks();

	public:
		auto get() const { return multiHandle.get(); }
		explicit operator bool() const noexcept { return !!multiHandle; }

	private:
		MultiHandle multiHandle;
	};

	class Awaiter : public C4Task::CancellableAwaiter<Awaiter>
	{
	public:
		Awaiter(C4CurlSystem &system, EasyHandle &&easyHandle);
		~Awaiter() = default;

	public:
		void SetResult(C4NetIO::addr_t &&result);
		void SetErrorMessage(const char *message);

		constexpr bool await_ready() const noexcept { return false; }

		template<typename T>
		void await_suspend(const std::coroutine_handle<T> handle)
		{
			coroutineHandle.store(handle, std::memory_order_release);

			SetCancellablePromise(handle);

			easyHandle.emplace<1>(system.AddHandle(std::move(std::get<0>(easyHandle))));
		}

		C4NetIO::addr_t await_resume();

		void SetupCancellation(C4Task::CancellablePromise *promise);
		void Resume();

	private:
		C4CurlSystem &system;
		std::variant<EasyHandle, AddedEasyHandle> easyHandle;
		std::atomic<std::coroutine_handle<>> coroutineHandle;
		std::mutex resultMutex;
		std::expected<C4NetIO::addr_t, std::string> result;
		std::atomic_bool cancelled{false};
	};

public:
	C4CurlSystem();
	~C4CurlSystem() = default;

	C4CurlSystem(const C4CurlSystem &) = delete;
	C4CurlSystem &operator=(const C4CurlSystem &) = delete;

	C4CurlSystem(C4CurlSystem &&) = delete;
	C4CurlSystem &operator=(C4CurlSystem &&) = delete;

public:
	Awaiter WaitForEasyAsync(EasyHandle &&easyHandle)
	{
		return {*this, std::move(easyHandle)};
	}

	AddedEasyHandle AddHandle(EasyHandle &&easyHandle);
	void RemoveHandle(CURL *const handle);

private:
	TaskType Execute();
	C4Task::Cold<WaitReturnType> Wait();
	void ProcessMessages();
	void CancelWait();

	std::unordered_map<std::pair<CURL *, SOCKET>, int> GetSocketMapCopy()
	{
		const std::lock_guard lock{socketMapMutex};
		return sockets;
	}

	static int SocketFunction(CURL *curl, curl_socket_t s, int what, void *userData) noexcept;
	static int TimerFunction(CURLM *, long timeout, void *userData) noexcept;

private:
	std::atomic_uint32_t timeout{StdSync::Infinite};
	[[NO_UNIQUE_ADDRESS]] GlobalInit globalInit;

	MultiHandleWithCallbacks multiHandle;

#ifdef _WIN32
	CStdEvent event;
#endif

	TaskType multiTask;

	std::atomic<C4Task::CancellablePromise *> wait{nullptr};

	std::mutex socketMapMutex;
	std::unordered_map<std::pair<CURL *, SOCKET>, int> sockets;
};
