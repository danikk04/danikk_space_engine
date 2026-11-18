#include <danikk_space_engine/gui.h>

namespace danikk_space_engine
{
	/** @copydoc GUI::addMessage */
	void GUI::addMessage(const String& text)
	{
		if(text.empty()) return;

		messages.pushCtor();
		for(size_t index = messages.size() - 1; index > 0; --index)
			messages[index] = static_cast<Message&&>(messages[index - 1]);
		messages[0].text = text;
		messages[0].remaining_seconds = 10.0f;
	}

	/** @copydoc GUI::updateMessages */
	void GUI::updateMessages(float dt)
	{
		for(size_t index = 0; index < messages.size();)
		{
			Message& message = messages[index];
			message.remaining_seconds -= dt;
			if(message.remaining_seconds > 0.0f)
			{
				++index;
				continue;
			}

			for(size_t shift = index + 1; shift < messages.size(); ++shift)
				messages[shift - 1] = static_cast<Message&&>(messages[shift]);
			messages.resize(messages.size() - 1);
		}

		String text;
		for(const Message& message : messages)
		{
			text << message.text << '\n';
		}
		messages_label.text = static_cast<String&&>(text);
		messages_label.flags.set(GUIElementFlags::visible, messages.size() > 0);
	}
}
