#pragma once

#include <default.h>
#include <component/component.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace sotiria_project
{
	class Object
	{
		friend class ComponentContainer;
		friend class ComponentInserter;
		friend class ObjectClassTemplate;

		void AllocateComponentsMemory(size_t mem_size, size_t com_count)
		{

		}
	public:
		byte* components_memory;
		ComponentStorage components;
		DynamicArray<Object> childs;

		Object()
		{

		}

		void tick(Object& parent)
		{
			components.tick(*this, parent);
			for(Object& el : childs)
			{
				el.tick(*this);
			}
		}

		void frame(Object& parent)
		{
			components.frame(*this, parent);
			for(Object& el : childs)
			{
				el.frame(*this);
			}
		}
	};


}
