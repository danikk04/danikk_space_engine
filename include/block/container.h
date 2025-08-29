#pragma once

#include <default.h>
#include <block/block.h>
#include <block/material.h>

namespace danikk_space_engine
{
	class AbstractContainer : public Solid
	{
	public:
		define_block_constructor(AbstractContainer, Solid){}
	};

	class WoodenContainer : public AbstractContainer
	{
	public:
		static size_t id;

		define_block_constructor(WoodenContainer, AbstractContainer){}
	};
}
