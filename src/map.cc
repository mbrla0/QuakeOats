/* map.cc - Everything related to map and asset handling. */
module;
#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>

export module map;

import <iostream>;	/* For stream functionality.	*/
import <vector>;	/* For vectors.					*/
import <sstream>;	/* AAAAAAAAAAAAAAAAAAAAAAAAAAA.	*/
import <concepts>;	/* For standard concepts.		*/
import gfx;			/* For Plane and Sampler.		*/
import str;			/* For UTF-8 strings.			*/

export namespace map
{
	/* Load a texture from a stream object.
	 * The data in the texture is expected to be laid out in the following way:
	 *     |--------|---------------|-----------------------------------|
	 *     | Offset | Type          | Description                       |
	 *     |--------|---------------|-----------------------------------|
	 *     | 0      | uint32_t      | Width of the texture, in pixels.  |
	 *     | 4      | uint32_t      | Heihgt of the texture, in pixels. |
	 *     | 8      | PixelRgba32[] | [width * height] Packed pixels.   |
	 *     |--------|---------------|-----------------------------------|
	 * The data from the stream will be copied and put into a new Plane object,
	 * with the exact same parameters as the ones given in the data stream.
	 */
	gfx::Plane<gfx::PixelRgba32> load_texture_rgba32(std::istream& data)
	{
		uint32_t width;
		uint32_t height;

		auto fail = []()
		{
			std::string what = u8"unexpected end of stream while reading \
				texture data into an rgba32 plane"_fb;
			throw std::runtime_error(what);
		};

		if(!(data >> width))  fail();
		if(!(data >> height)) fail();

		gfx::Plane<gfx::PixelRgba32> plane(width, height);
		for(uint32_t i = 0; i < height; ++i)
			for(uint32_t j = 0; j < width; ++j)
			{
				gfx::PixelRgba32 pixel;
				data.read((char*) &pixel.red,   1);
				data.read((char*) &pixel.green, 1);
				data.read((char*) &pixel.blue,  1);
				data.read((char*) &pixel.alpha, 1);
				if(!data) fail();
				plane.at(j, i) = pixel;
			}

		return plane;
	}	
	
	/* Points that can be loaded from an input stream. */
	template<typename T>
	concept LoadablePoint = requires(std::istream& stream)
	{
		{ T::next_from_stream(stream) } -> std::same_as<T>;
	};

	/* A model comprised of points and indices, along with a primitive assembly
	 * mode. Its functionality is very much almost the same as gfx::Mesh, with
	 * the only difference being that a model owns its data, whereas a Mesh is
	 * simply a set of references to that data. */
	template<LoadablePoint P>
	class Model
	{
	protected:
		uint32_t _mode;

		std::vector<P> _points;
		std::vector<size_t> _indices;

		glm::mat4 _transform;
	public:
		/* This type can be treated as a Mesh with no loss of information. */
		operator gfx::Mesh<P>() const
		{
			switch(_mode)
			{
			case 0:
				return gfx::Mesh(_points, _indices, gfx::Primitive::TriangleList);
			case 1:
				return gfx::Mesh(_points, _indices, gfx::Primitive::TriangleStrip);
			}

			std::string what = u8"invalid mesh mode"_fb;
			throw std::runtime_error(what);
		}

		/* Transformation matrix from model space to world space. */
		glm::mat4 transformation() const
		{
			return _transform;
		}
	public:

		/* Loads a model from a stream object.
		 * The data in the model is expected to be laid out in the following way:
		 *     |--------|---------------|-------------------------------------|
		 *     | Offset | Type          | Description                         |
		 *     |--------|---------------|-------------------------------------|
		 *     | 0      | uint32_t      | Primitive assembly mode:            |
		 *     |        |               | 0 = TriangleList 1 = TriangleStrip  | 
		 *     | 4      | uint32_t      | Number of points in the model.      |
		 *     | 8      | uint32_t      | Number of indices in the model.     |
		 *     | 12     | float         | X value for the world translation.  |
		 *     | 16     | float         | Y value for the world translation.  |
		 *     | 20     | float         | Z value for the world translation.  |
		 *     | 24     | float         | X value for the world scaling.      |
		 *     | 28     | float         | Y value for the world scaling.      |
		 *     | 32     | float         | Z value for the world scaling.      |
		 *     | 36     | float         | Pitch value for the world rotation. |
		 *     | 40     | float         | Yaw   value for the world rotation. |
		 *     | 44     | float         | Roll  value for the world rotation. |
		 *     | 48     | P[]           | Packed template-loaded points.      |
		 *     | ..     | uint32_t[]    | Packed indices.                     |
		 *     |--------|---------------|-------------------------------------|
		 * The data from the stream will be copied and put into a new Model object,
		 * with the exact same parameters as the ones given in the data stream. */
		static Model<P> load(std::istream& data)
		{
			Model<P> model;
			uint32_t points, indices;
			float x, y, z, sx, sy, sz, pitch, yaw, roll;
			
			auto fail = []()
			{
				std::string what = u8"unexpected end of stream while reading \
					model data"_fb;
				throw std::runtime_error(what);
			};
			
			if(!(data >> model._mode)) fail();
			if(!(data >> points))  fail();
			if(!(data >> indices)) fail();

			if(!(data >> x))     fail();
			if(!(data >> y))     fail();
			if(!(data >> z))     fail();
			if(!(data >> sx))    fail();
			if(!(data >> sy))    fail();
			if(!(data >> sz))    fail();
			if(!(data >> pitch)) fail();
			if(!(data >> yaw))   fail();
			if(!(data >> roll))  fail();

			model._transform = glm::translate(glm::vec3(x, y, z));
			model._transform = glm::scale(model._transform, glm::vec3(sx, sy, sz));
			model._transform = glm::rotate(pitch, glm::vec3(1.0, 0.0, 0.0));
			model._transform = glm::rotate(yaw,   glm::vec3(0.0, 1.0, 0.0));
			model._transform = glm::rotate(roll,  glm::vec3(0.0, 0.0, 1.0));

			model._points.reserve(points);
			for(uint32_t i = 0; i < points; ++i)
				model._points.push_back(P::next_from_stream(data));

			model._indices.reserve(indices);
			for(uint32_t i = 0; i < indices; ++i)
			{
				uint32_t index;
				if(!(data >> index)) fail();
				model._indices.push_back(index);
			}

			return model;
		}
	};

	/* Point type used by the models loded in from maps. */
	struct Point
	{
		/* Index to the texture in the texture bank of the current map. */
		uint32_t texture_index;
	
		/* Texture coordinates in sampler space. */
		glm::vec2 sampler;

		/* Components of the normal vector. */
		glm::vec3 normal;

		/* Position of this point in model space. */
		glm::vec4 position;

		/* Loads a point from an input stream. */
		static Point next_from_stream(std::istream& data)
		{
			uint32_t t;
			float i, j, nx, ny, nz, x, y, z, w;

			auto fail = []()
			{
				std::string what = u8"unexpected end of stream while reading \
					model point data"_fb;
				throw std::runtime_error(what);
			};

			if(!(data >> t))  fail();
			if(!(data >> i))  fail();
			if(!(data >> j))  fail();
			if(!(data >> nx)) fail();
			if(!(data >> ny)) fail();
			if(!(data >> nz)) fail();
			if(!(data >> x))  fail();
			if(!(data >> y))  fail();
			if(!(data >> z))  fail();
			if(!(data >> w))  fail();

			Point p;
			p.texture_index = t;
			p.sampler  = glm::vec2(i, j);
			p.normal   = glm::vec3(nx, ny, nz);
			p.position = glm::vec4(x, y, z, w);

			return p;
		}
	};

	/* Slope between two points. */
	class PointSlope
	{
	protected:
		Point _a;
		Point _b;
	public:
		PointSlope(Point a, Point b)
			: _a(a), _b(b)
		{ }

		Point at(double x)
		{
			Point t;
			t.texture_index = _a.texture_index;
			t.sampler  = glm::mix(_a.sampler,  _b.sampler,  x);
			t.normal   = glm::mix(_a.normal,   _b.normal,   x);
			t.position = glm::mix(_a.position, _b.position, x);
			return t;
		}
		Point at(float x) { return at((double) x); }
	};

	/* A map is a container for textures and models. */
	class Map
	{
	protected:
		/* Bank of all the textures used by the map. This list is prepended by
		 * a null texture, whose index is always zero, which is done to 
		 * accommodate materials with no associated texture data. */
		std::vector<gfx::Plane<gfx::PixelRgba32>> _textures;

		/* Bank of all the model slices used by the map. */
		std::vector<Model<Point>> _models;
	public:
		const gfx::Plane<gfx::PixelRgba32>& texture(uint32_t index) const
		{
			return _textures[index];
		}
	public:
		/* Loads a map a stream object.
		 * The data in the model is expected to be laid out in the following way:
		 *     |--------|---------------|-------------------------------------|
		 *     | Offset | Type          | Description                         |
		 *     |--------|---------------|-------------------------------------|
		 *     | 0      | uint32_t      | Number of textures in the map.      |
		 *     | 4      | uint32_t      | Number of models in the map.        |
		 *     | 48     | Texture[]     | Packed textures.                    |
		 *     | ..     | Model[]       | Packed models.                      |
		 *     |--------|---------------|-------------------------------------|
		 * The data from the stream will be copied and put into a new Map object,
		 * with the exact same parameters as the ones given in the data stream. */
		static Map load(std::istream &data)
		{
			Map map;
			uint32_t textures, models;

			auto fail = []()
			{
				std::string what = u8"unexpected end of stream while reading \
					map data"_fb;
				throw std::runtime_error(what);
			};

			if(!(data >> textures)) fail();
			if(!(data >> models))   fail();

			map._textures.reserve(textures + 1);
			map._models.reserve(models);

			/* Initialize the null texture. */
			map._textures.push_back(gfx::Plane<gfx::PixelRgba32>(1, 1));
			map._textures[0].at(0, 0) = gfx::PixelRgba32(0.0, 0.0, 0.0, 1.0);

			/* Initialize all of the other textures. */
			for(uint32_t i = 0; i < textures; ++i)
				map._textures.push_back(load_texture_rgba32(data));

			/* Initialize all of the modules. */
			for(uint32_t i = 0; i < models; ++i)
				map._models.push_back(Model<Point>::load(data));

			return map;
		}
	};
};
