#pragma once
#include <object.h>
#include <component/component.h>

namespace sotiria_project
{
	class ObjectClassTemplate : private Array<ComponentClassContainer*, 8>
	{
	public:
		void addClass(uint32 id)
		{
			push(component_class_storage.getClassById(id));
		}

		void createObject(Object& ref)
		{
			index_t i = 0;

			size_t mem_size = 0;

			for(ComponentClassContainer* class_container : *this)
			{
				mem_size += class_container->size + sizeof(ComponentContainer);
			}
			for(ComponentClassContainer* class_container : *this)
			{
				mem_size += class_container->size + sizeof(ComponentContainer);
			}

			ref.components_memory = malloc(mem_size);
			ref.components.m_data = (ComponentContainer*)components_memory;
			ref.components.m_size = component_list.size();
			byte* current_memory = components_memory + sizeof(ComponentContainer) * component_list.size();

			ComponentContainer* current_container = components.m_data;
			for(ComponentClassContainer& class_container : component_list)
			{
				current_container->class_container = &class_container;
				current_container->component_memory = current_memory;
				current_container->class_container->init(current_container->component_memory, *this);
				current_memory += class_container.size;
				current_container++;
			}
		}
	};

	class ObjectClassTemplateCollection
	{
		struct ObjectTemplateContainer
		{
			ObjectClassTemplate data;
			uint32 id;
			const char* name;
		};
		Array<ObjectTemplateContainer, 64> array;
	};

	extern ObjectClassTemplateCollection object_templates;
}
