#include "./shader.h"
#include "../core/sample.h"

static vec3 cal_normal(vec3 &normal, vec3 *world_coords,const vec2 *uvs,const vec2 &uv, TGAImage *normal_map)
{
	//calculate the difference in UV coordinate
	float x1 = uvs[1][0] - uvs[0][0];
	float y1 = uvs[1][1] - uvs[0][1];
	float x2 = uvs[2][0] - uvs[0][0];
	float y2 = uvs[2][1] - uvs[0][1];
	float det = (x1 * y2 - x2 * y1);

	//calculate the difference in world pos
	vec3 e1 = world_coords[1] - world_coords[0];
	vec3 e2 = world_coords[2] - world_coords[0];

	vec3 t = e1 * y2 + e2 * (-y1);
	vec3 b = e1 * (-x2) + e2 * x1;
	t /= det;
	b /= det;

	//Schmidt orthogonalization
	normal = unit_vector(normal);
	t = unit_vector(t - dot(t, normal)*normal);
	b = unit_vector(b - dot(b, normal)*normal - dot(b, t)*t);

	vec3 sample = texture_sample(uv, normal_map);
	//modify the range 0 ~ 1 to -1 ~ +1
	sample = vec3(sample[0] * 2 - 1, sample[1] * 2 - 1, sample[2] * 2 - 1);

	vec3 normal_new = t * sample[0] + b * sample[1] + normal * sample[2];
	return normal_new;
}

void PhongShader::vertex_shader(int nfaces, int nvertex)
{
	int i = 0;
	vec4 temp_vert = to_vec4(payload.model->vert(nfaces, nvertex), 1.0f);
	vec4 temp_normal = to_vec4(payload.model->normal(nfaces, nvertex), 1.0f);

	payload.uv_attri[nvertex] = payload.model->uv(nfaces, nvertex);
	payload.in_uv[nvertex] = payload.uv_attri[nvertex];
	payload.clipcoord_attri[nvertex] = payload.mvp_matrix * temp_vert;
	payload.in_clipcoord[nvertex] = payload.clipcoord_attri[nvertex];

	//only model matrix can change normal vector
	for (i = 0; i < 3; i++)
	{
		payload.worldcoord_attri[nvertex][i] = temp_vert[i];
		payload.in_worldcoord[nvertex][i] = temp_vert[i];
		payload.normal_attri[nvertex][i] = temp_normal[i];
		payload.in_normal[nvertex][i] = temp_normal[i];
	}
}

vec3 PhongShader::fragment_shader(float alpha, float beta, float gamma)
{

	//set light information
	float p = 150.0;
	vec3 amb_light_intensity(5, 5, 5);
	light light1{ {2, 4, 2},{50, 50, 50} };
	light light2{ {-1, 0.2, 5},{10, 10,10} };
	std::vector<light> lights = { light1 };

	//for reading easily
	vec4 *clip_coords = payload.clipcoord_attri;
	vec3 *world_coords = payload.worldcoord_attri;
	vec3 *normals = payload.normal_attri;
	vec2 *uvs = payload.uv_attri;

	//interpolate attribute
	float Z = 1.0 / (alpha / clip_coords[0].w() + beta / clip_coords[1].w() + gamma / clip_coords[2].w());
	vec3 normal = (alpha*normals[0] / clip_coords[0].w() + beta * normals[1] / clip_coords[1].w() +
		gamma * normals[2] / clip_coords[2].w()) * Z;
	vec2 uv = (alpha*uvs[0] / clip_coords[0].w() + beta * uvs[1] / clip_coords[1].w() +
		gamma * uvs[2] / clip_coords[2].w()) * Z;
	vec3 worldpos = (alpha*world_coords[0] / clip_coords[0].w() + beta * world_coords[1] / clip_coords[1].w() +
		gamma * world_coords[2] / clip_coords[2].w()) * Z;



	if (payload.model->normalmap_)
	{
		//printf("here\n");
		normal = cal_normal(normal, world_coords, uvs, uv, payload.model->normalmap_);
	}

	//get ka,ks,kd
	vec3 ka(0.0005, 0.0005, 0.0005);
	vec3 ks(0.6937, 0.6937, 0.6937);
	vec3 kd = payload.model->diffuse(uv);

	//calculate shading color
	vec3 result_color(0, 0, 0);
	for (auto& light : lights)
	{
		//must calculate in world space, be careful! I make a mistake here
		vec3 ambient, diffuse, specular;
		normal = unit_vector(normal);
		vec3 l = unit_vector(vec3(1, 3, 1));
		vec3 v = unit_vector(payload.camera->eye - worldpos);
		vec3 h = unit_vector(l + v);
		float r = (light.pos - worldpos).norm_squared();

		//the calculation here is not abided by original blinn-phong strictly.
		//just for better visual feelings, u can modify any u want.
		ambient = cwise_product(ka, amb_light_intensity);
		diffuse = cwise_product(kd, light.intensity) * float_max(0, dot(l, normal)) / r;
		diffuse = kd * float_max(0, dot(l, normal));
		specular = cwise_product(ks, light.intensity) * float_max(0, pow(dot(normal, h), p)) / r;

		result_color += (0.9*kd + diffuse * 0.2);
		//result_color += diffuse;
	}
	return result_color * 255.f;
}