#pragma once

#include <default.h>
#include <danikk_framework/array.h>
#include <danikk_framework/memory.h>
#include <danikk_framework/sub_array.h>


namespace sotiria_project
{
	extern uint32 last_component_id;

#define DEFINE_COMPONENT_ID static uint32 id;
#define IMPLEMENT_COMPONENT_ID(CLASS) uint32 CLASS::id = -1;

#define DEFINE_COMPONENT_INIT_FUNCTION void init(Object& container, Object& parent)
#define DEFINE_COMPONENT_TICK_FUNCTION void tick (Object& container, Object& parent)
#define DEFINE_COMPONENT_FRAME_FUNCTION void frame (Object& container, Object& parent)

	class BaseComponent
	{

	};

	inline uint32 generateNewComponentId()
	{
		return last_component_id++;
	}

	class ComponentClassContainer
	{
	public:
		typedef void (BaseComponent::*component_method)(Object&, Object&);

		component_method init;
		component_method tick;
		component_method frame;
		uint32 size;
		uint32 id;

		template<class component_t> void initClass()
		{
			init = (component_method)&component_t::init;
			tick = (component_method)&component_t::tick;
			frame = (component_method)&component_t::frame;
			size = sizeof(component_t);
			component_t::id = generateNewComponentId();
			id = component_t::id;
		}
	};

	class ComponentClassStorage : Array<ComponentClassContainer, 64>
	{
	public:
		ComponentClassStorage();

		template<class component_t> void addClass()
		{
			ComponentClassContainer* container = &pushCtor();
			container->initClass<component_t>();
			assert(size() == container->id + 1);//id должен быть индексом в массиве
		}

		inline ComponentClassContainer* getClassById(uint32 id)
		{
			return &(*this)[id];
		}
	};

	extern ComponentClassStorage component_class_storage;

	class ComponentContainer
	{
	public:
		void* component_memory;
		ComponentClassContainer* class_container;
	};

	class ComponentStorage : public SubArray<ComponentContainer>
	{
		friend class ObjectTemplate;

		void addComponent(Object& parent, uint32 class_id, index_t& current_index)
		{
			ComponentContainer& container = (*this)[current_index++];
			container.class_container = component_class_storage.getClassById(class_id);
		}
	public:
		template<class component_t> index_t getComponentIndex()
		{
			for(index_t i = 0; i < size(); i++)
			{
				if((*this)[i].class_container->id == component_t::id)
				{
					return i;
				}
			}
			fatalError("ComponentStorage !> id");
			assert(0);
		}

		template<class component_t> component_t* getComponent(index_t index)
		{
			return (component_t*)((*this)[index].component_memory);
		}

		void tick(Object& container, Object& parent)
		{
			for(ComponentContainer& el : *this)
			{
				ComponentClassContainer::component_method frame_func = el.class_container->tick;
				BaseComponent* component = (BaseComponent*)el.component_memory;
				(*component.*frame_func)(container, parent);
			}
		}

		void frame(Object& container, Object& parent)
		{
			for(ComponentContainer& el : *this)
			{
				ComponentClassContainer::component_method frame_func = el.class_container->frame;
				BaseComponent* component = (BaseComponent*)el.component_memory;
				(*component.*frame_func)(container, parent);
			}
		}
	};


}
