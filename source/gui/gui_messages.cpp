#include <danikk_space_engine/gui/gui.h>

namespace danikk_space_engine
{
	/** @copydoc GUI::addMessage */
	void GUI::addMessage(const String& text)
	{
		if(text.empty()) return;

		message_log.messages.pushCtor();
		for(size_t index = message_log.messages.size() - 1; index > 0; --index)
			message_log.messages[index] = static_cast<gui_detail::MessageLog::Message&&>(
				message_log.messages[index - 1]);
		message_log.messages[0].text = text;
		message_log.messages[0].remaining_seconds = 10.0f;
	}

	/** @copydoc GUI::updateMessages */
	void GUI::updateMessages(float dt)
	{
		for(size_t index = 0; index < message_log.messages.size();)
		{
			gui_detail::MessageLog::Message& message = message_log.messages[index];
			message.remaining_seconds -= dt;
			if(message.remaining_seconds > 0.0f)
			{
				++index;
				continue;
			}

			for(size_t shift = index + 1; shift < message_log.messages.size(); ++shift)
				message_log.messages[shift - 1] =
					static_cast<gui_detail::MessageLog::Message&&>(message_log.messages[shift]);
			message_log.messages.resize(message_log.messages.size() - 1);
		}

		String text;
		for(const gui_detail::MessageLog::Message& message : message_log.messages)
		{
			text << message.text << '\n';
		}
		message_log.label.text = static_cast<String&&>(text);
		message_log.label.flags.set(GUIElementFlags::visible, message_log.messages.size() > 0);
	}
}
