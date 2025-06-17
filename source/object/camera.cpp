#include <object/camera.h>
#include <default.h>
#include <danikk_engine/texture_methods.h>
#include <danikk_engine/danikk_engine.h>

namespace game
{
	void Camera::setViewMatrix()
	{
		mat4 view = glm::lookAt(pos, pos + getFront(), vec3(0, 1, 0));
		mat4 projection = glm::perspective(90.0f, screen_ratio_gz, 0.0001f, 10000.0f);
		danikk_engine::setViewMatrix(view);
		setProjectionMatrix(projection);
		Object::frame();
	}
}
