#pragma once

#include <glm/glm.hpp>

// Material surface properties to be used as input into a local illumination model
// (e.g. the Phong Reflection Model).
struct Material {
	Material()
			: kd(glm::vec3(0.0f)),
			  ks(glm::vec3(0.0f)),
              //id(glm::vec3(0.0f)),
			  shininess(0.0f) { }

	// Diffuse reflection coefficient
	glm::vec3 kd;

	// Specular reflection coefficient
	glm::vec3 ks;
    
    // id
    glm::vec3 id;

	// Material shininess constant.  Larger positive values model surfaces that
	// are smoother or mirror-like.  Smaller positive values model rougher surfaces.
	float shininess;
};
