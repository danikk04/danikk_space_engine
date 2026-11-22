#pragma once

#include <atomic>
#include <chrono>
#include <string_view>

#include <danikk_space_engine/config.h>

namespace danikk_space_engine
{
	class ProfilingSystem;

	/** @brief Measures one named CPU scope and records its elapsed duration when destroyed. */
	class Watcher
	{
		/** @brief Profiler currently active for Watcher scopes on the main thread. */
		static std::atomic<ProfilingSystem*> active_profiling_system;
		/** @brief Profiler selected by the current thread's World telemetry context. */
		ProfilingSystem* profiling_system = nullptr;
		/** @brief Telemetry node receiving this watcher's accumulated duration. */
		uint32 node_id = static_cast<uint32>(-1);
		/** @brief Monotonic timestamp captured when the watched scope begins. */
		std::chrono::steady_clock::time_point start_time;

		/** @brief Starts timing a scope and registers it beneath the current parent scope. */
		explicit Watcher(const char* name);
		friend class ProfilingSystem;

	public:
		/** @brief Creates a timing scope for the instrumentation macro. */
		[[nodiscard]] static Watcher create(const char* name) { return Watcher(name); }
		/** @brief Disabled so a timed scope cannot be ended by multiple destructors. */
		Watcher(const Watcher&) = delete;
		/** @brief Disabled so a timed scope cannot be ended by multiple destructors. */
		Watcher& operator=(const Watcher&) = delete;
		/** @brief Stops the timer and adds its elapsed duration to the global profiler. */
		~Watcher();
	};

	namespace telemetry_internal
	{
		template<size_t capacity>
		struct WatcherName
		{
			char text[capacity]{};
		};

		/** @brief Finds the function token before its parameters, including names such as operator(). */
		constexpr size_t findFunctionName(std::string_view signature, std::string_view function)
		{
			for(size_t start = signature.find(function); start != std::string_view::npos;
				start = signature.find(function, start + 1))
			{
				const size_t end = start + function.size();
				if(end == signature.size() || signature[end] != '(') continue;
				if(start == 0 || signature[start - 1] == ' '
					|| (start >= 2 && signature.substr(start - 2, 2) == "::")) return start;
			}
			return std::string_view::npos;
		}

		/** @brief Extracts the nearest named class or namespace from a decorated signature. */
		constexpr std::string_view findWatcherOwner(std::string_view prefix)
		{
			while(prefix.ends_with("::"))
			{
				prefix.remove_suffix(2);
				constexpr std::string_view gcc_anonymous = "{anonymous}";
				constexpr std::string_view clang_anonymous = "(anonymous namespace)";
				constexpr std::string_view msvc_anonymous = "`anonymous namespace'";
				if(prefix.ends_with(gcc_anonymous)) prefix.remove_suffix(gcc_anonymous.size());
				else if(prefix.ends_with(clang_anonymous)) prefix.remove_suffix(clang_anonymous.size());
				else if(prefix.ends_with(msvc_anonymous)) prefix.remove_suffix(msvc_anonymous.size());
				else
				{
					if(prefix.ends_with(">"))
					{
						size_t depth = 0;
						for(size_t index = prefix.size(); index > 0;)
							{
								const char character = prefix[--index];
								if(character == '>') ++depth;
								if(character == '<' && --depth == 0)
								{
									prefix = prefix.substr(0, index);
									break;
								}
							}
					}
					const size_t start = prefix.find_last_of(": \t*&");
					return start == std::string_view::npos ? prefix : prefix.substr(start + 1);
				}
			}
			return {};
		}

		/** @brief Builds a stable owner::function label entirely at compile time. */
		template<size_t capacity>
		consteval WatcherName<capacity> makeWatcherName(const char (&signature)[capacity],
			std::string_view function)
		{
			const std::string_view decorated(signature, capacity - 1);
			const size_t start = findFunctionName(decorated, function);
			const auto owner = start == std::string_view::npos
				? std::string_view{} : findWatcherOwner(decorated.substr(0, start));

			WatcherName<capacity> result;
			size_t length = 0;
			for(char character : owner) result.text[length++] = character;
			if(!owner.empty())
			{
				result.text[length++] = ':';
				result.text[length++] = ':';
			}
			for(char character : function) result.text[length++] = character;
			return result;
		}
	}
}

#if defined(__clang__) || defined(__GNUC__)
	#define DSE_WATCH_SIGNATURE __PRETTY_FUNCTION__
#elif defined(_MSC_VER)
	#define DSE_WATCH_SIGNATURE __FUNCSIG__
#else
	#define DSE_WATCH_SIGNATURE __func__
#endif

/** @brief Profiles the enclosing function under its short name. Declare once per scope. */
#define DSE_WATCH() \
	static constexpr auto dse_watcher_name = ::danikk_space_engine::telemetry_internal::makeWatcherName( \
		DSE_WATCH_SIGNATURE, __func__); \
	[[maybe_unused]] const auto dse_watcher = ::danikk_space_engine::Watcher::create(dse_watcher_name.text)

#define DSE_PROFILE_CONTROLLER()
