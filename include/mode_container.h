#pragma once

#include <default.h>
#include <keybind.h>

namespace danikk_space_engine
{
	class ModeContainer
	{
	private:
		int m_enum;
		const char* sys_name;
		const char** strings;
		int max_enum;
	public:
		ModeContainer() = default;

		ModeContainer(int base_enum, int max_enum, const char* sys_name, const char** strings) : m_enum(base_enum), sys_name(sys_name), strings(strings), max_enum(max_enum)
		{
		}

		void operator ++(int)
		{
			m_enum++;
			if(m_enum == max_enum)
			{
				m_enum = 0;
			}
		}

		static void event(void* container_ptr)
		{
			ModeContainer& container = *(ModeContainer*)container_ptr;
			container++;
		}

		void bind(const KeyBindKeys& keys)
		{
			key_binds.bind(keys, event, (void*)this);
		}

		bool is(int value)
		{
			return m_enum == value;
		}
	};

#define define_mode(name) ModeContainer name##_mode(name::BASE, name::MAX, #name, name##_strings)
#define declare_mode(name) extern ModeContainer name##_mode
#define current_mode_is(name, mode) name##_mode.is(name::mode)
}
