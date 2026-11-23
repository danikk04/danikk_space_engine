#include <danikk_space_engine/systems/telemetry/telemetry.h>
#include <danikk_space_engine/world.h>
#include <danikk_space_engine/world_body.h>
#include <danikk_space_engine/point.h>
#include <danikk_space_engine/systems/camera/camera.h>

namespace danikk_space_engine
{
	glm::dvec3 WorldBodyData::getPointWorldPosition(PointId point_id) const
	{
		DSE_WATCH();
		const Point& point = world.getPointData().getPoint(point_id);
		const WorldBody& body = getWorldBody(point.world_body_id);
		return body.position + body.rotation * point.body_local_position;
	}

	mat4 WorldBodyData::getPointWorldMatrix(PointId point_id) const
	{
		const Point& point = world.getPointData().getPoint(point_id);
		const WorldBody& body = getWorldBody(point.world_body_id);
		const glm::dvec3 relative_position = body.position + body.rotation * point.body_local_position
			- world.getCameraSystem().getWorldPosition();
		mat4 rotation(1.0f);
		rotation[0] = vec4(vec3(body.rotation * glm::dvec3(1.0, 0.0, 0.0)), 0.0f);
		rotation[1] = vec4(vec3(body.rotation * glm::dvec3(0.0, 1.0, 0.0)), 0.0f);
		rotation[2] = vec4(vec3(body.rotation * glm::dvec3(0.0, 0.0, 1.0)), 0.0f);
		return glm::translate(mat4(1.0f), vec3(relative_position)) * rotation;
	}

	void WorldBodyData::setPointBodyLocalPosition(PointId point_id,
		const glm::dvec3& body_local_position)
	{
		if(!world.getPointData().isPointActive(point_id)) return;
		Point& point = world.getPointData().getPoint(point_id);
		if(!isWorldBodyActive(point.world_body_id))
			danikk_engine::formatFatalError("Cannot move point % without an active WorldBody", point_id);
		WorldBody& body = getWorldBody(point.world_body_id);
		body.spatial_tree.remove(point_id, point.body_local_position);
		point.body_local_position = body_local_position;
		body.spatial_tree.insert(point_id, body_local_position);
	}
}
