/* gfx.cc - The rasterizer. This module provides access to a triangle
 * renderer with a multi-stage pipeline and other graphics utilities. */
module;
#include "thread_utils.hpp"	/* Thanks Natan. */

export module gfx;

import <string>;	/* For standard string types.		*/
import <sstream>;	/* AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA.	*/
import <cstdint>;	/* For standard integer types.		*/
import <stdexcept>;	/* For standard exception types.	*/
import <concepts>;	/* For standard concepts.			*/
import <iostream>;	/* For warning messages.			*/
import <cmath>;		/* For floor() and ceil().			*/
import str;			/* Haha UTF-8 go brr.				*/

export namespace gfx
{
	/* RGBA 32 color values. */
	struct alignas(4) PixelRgba32
	{
		uint8_t red;
		uint8_t green;
		uint8_t blue;
		uint8_t alpha;

		PixelRgba32()
		{
			this->red   = 0;
			this->green = 0;
			this->blue  = 0;
			this->alpha = 0xff;
		}

		PixelRgba32(uint8_t v)
		{
			this->red   = v;
			this->green = v;
			this->blue  = v;
			this->alpha = v;
		}

		PixelRgba32(uint8_t r, uint8_t g, uint8_t b, uint8_t a)
		{
			this->red   = r;
			this->green = g;
			this->blue  = b;
			this->alpha = a;
		}
	};
	
	/* Slope between two RGBA 32 color values. */
	class PixelRgba32Slope
	{
	protected:
		PixelRgba32 _a;
		PixelRgba32 _b;
	public:
		/* Create a new slope between the given pixels. */
		PixelRgba32Slope(PixelRgba32 a, PixelRgba32 b)
			: _a(a), _b(b)
		{ }

		PixelRgba32 at(double x)
		{
			PixelRgba32 p;
			p.red   = (1.0 - x) * _a.red   + x * _b.red;
			p.green = (1.0 - x) * _a.green + x * _b.green;
			p.blue  = (1.0 - x) * _a.blue  + x * _b.blue;
			p.alpha = (1.0 - x) * _a.alpha + x * _b.alpha;
			
			return p;
		}
		PixelRgba32 at(float x) { return at((double) x); }
	};

	/* A plane of data points. */
	template<typename T>
	class Plane
	{
	protected:
		/* Extent of the plane, in pixels. */
		uint32_t _width, _height;

		/* Color storage. */
		T *_data;

		/* Ownership of the data storage. */
		bool _owns;
	protected:
		/* Checks for bounds on input X and Y coordinates, thowing a range error
		 * in case the coordinates land somewhere outside the plane. */
		void _check_bounds(uint32_t x, uint32_t y) const
		{
			if(x >= this->_width || y >= this->_height)
			{
				std::stringstream what;
				what << u8"Could not access pixel at ";
				what << u8"(" << x << u8", " << y << u8")";
				what << u8", expected < ";
				what << u8"(" << _width << u8", " << _height << u8")";

				throw std::range_error(what.str());
			}
		}
	public:
		/* Creates a new plane with the given dimensions.
		 * 
		 * The backing storage is allocated immediately and left unitialized.
		 * Therefore, the data inside of a plane at creation is undefined, which
		 * should not be a problem, as most buffer operations are write-only, 
		 * and thus performing a buffer clear would be unneeded.
		 *
		 * If a uniform, known value is desired the user should call `clear()`
		 * in order to initialize the contents of the plane.
		 */
		Plane(uint32_t width, uint32_t height)
			: _width(width), _height(height)
		{
			this->_data = new T[width * height];
			this->_owns = true;
		}

		~Plane()
		{
			if(_owns) 
				/* We own the buffer. */
				delete[] this->_data;
		}	

		/* Copy constructor. */
		Plane(const Plane& source)
		{
			this->_width  = source._width;
			this->_height = source._height;
			this->_data   = new T[source._width * source._height];
			this->_owns   = true;

			std::memcpy(
				(void*) this->_data, 
				(const void*) source._data,
				_width * _height * sizeof(T));
		}

		/* Move constructor. */
		Plane(Plane&& source)
		{
			this->_width  = source._width;
			this->_height = source._height;
			this->_data   = source._data;
			this->_owns   = true;
			source._owns  = false;
		}

		/* Acquire a reference to a data point in the plane at the given x and y
		 * coordinate index, not checking for boundaries. 
		 *
		 * # Safety
		 * This does not perform any boundary checks on the  input coordinates 
		 * and is thus wildly unsafe. Prefer `at()` over this unless it is 
		 * guaranteed that you'll only pass valid coordinates. 
		 */
		constexpr const T& at_unchecked(uint32_t x, uint32_t y) const noexcept
		{
			return this->_data[y * this->_width + x];
		}
		
		/* Acquire a reference to a data point in the plane at the given x and y
		 * coordinate index, not checking for boundaries. 
		 *
		 * # Safety
		 * This does not perform any boundary checks on the  input coordinates 
		 * and is thus wildly unsafe. Prefer `at()` over this unless it is 
		 * guaranteed that you'll only pass valid coordinates. 
		 */
		constexpr T& at_unckecked(uint32_t x, uint32_t y) noexcept
		{
			return this->_data[y * this->_width + x];
		}	

		/* Acquire a reference to a data point in the plane at the given x and y
		 * coordinate index, checking for boundaries. 
		 *
		 * # Exceptions
		 * Throws an instance of `std::range_error` if the given x or y values 
		 * land outside the range of the plane. 
		 */
		const T& at(uint32_t x, uint32_t y) const
		{
			this->_check_bounds(x, y);
			return this->_data[y * this->_width + x];
		}
		
		/* Acquire a reference to a data point in the plane at the given x and y
		 * coordinate index, checking for boundaries. 
		 *
		 * # Exceptions
		 * Throws an instance of `std::range_error` if the given x or y values 
		 * land outside the range of the plane. 
		 */
		T& at(uint32_t x, uint32_t y)
		{
			this->_check_bounds(x, y);
			return this->_data[y * this->_width + x];
		}

		/* Clears the buffer with a given data value. */
		void clear(T clear)
		{
			for(uint32_t i = 0; i < this->_height; ++i)
				for(uint32_t j = 0; j < this->_width; ++j)
					this->_data[i * this->_width + j] = clear;
		}

		/* Gets the width of this plane. */
		uint32_t width() const
		{
			return this->_width;
		}

		/* Gets the height of this plane. */
		uint32_t height() const
		{
			return this->_height;
		}

		/* Returns the backing storage for this plane. The elements in this 
		 * storage are guaranteed to be laid out in row-major with no stride
		 * from one row to the next.
		 *
		 * # Safety
		 * None. I hope you know what you're doing. 
		 */
		T* data()
		{
			return this->_data;
		}
	};

	template<typename T, typename P>
	concept Slope = requires(T slope, double dPos, float fPos)
	{
		{ slope.at(dPos) } -> std::same_as<P>;
		{ slope.at(fPos) } -> std::same_as<P>;
	};

	/* Plane sampler.
	 * 
	 * This structure is responsible for sampling data from the plane and
	 * and interpolating that data using the given slope type and generation
	 * functor. This sampler behaves very much like the image samplers in OpenGL
	 * and Vulkan. */
	template<typename T, typename S>
		requires Slope<S, T>
	class Sampler
	{
	protected:
		/* The plane whose values are to be interpolated with the slope. */
		const Plane<T>& _plane;

		/* The slope generation function. */
		std::function<S(T, T)> _slope;
	public:
		/* Create a new sampler for the given plane with the given slope 
		 * generation function. */
		Sampler(
			const Plane<T> &plane,
			std::function<S(T, T)> slope)
			: _plane(plane), _slope(slope)
		{ }	

		/* Sample data from this plane at the given coordinates. The sampled 
		 * value resulting from this function will be 4-way interpolated using 
		 * the templated slope type and provided slope generation function.
		 * 
		 * It is important to note that these coordinates are not in plane 
		 * space, instead, they sit in an arbirray space from (0, 0) to (1, 1),
		 * with the following mapping into plane space:
		 *
		 *   (0, 1)                    (1, 1)
		 *    |------------|------------|
		 *    |            |            |
		 *    |            |            |
		 *    |            |            |
		 *    |--------(0.5,0.5)--------|
		 *    |            |            |
		 *    |            |            |
		 *    |            |            |
		 *    |------------|------------|
		 *   (0, 0)                    (1, 0)
		 *
		 * This is done to match the behaviors found in OpenGL and Vulkan, which
		 * is what most toolage for textures out there expects.
		 */
		T at(double x, double y) const
		{
			/* Map our input space into the plane. */
			x = x * (double) _plane.width();
			y = y * (double) _plane.height();
			y = (double) _plane.height() - 1;

			/* Clamp resulting coordinates into fitting neatly into the sampled
			 * plane. This guarantees this function will behave nicely at the
			 * edges of the input and not generate any undesided exceptions. */
			x = std::clamp(x, 0.0, (double) _plane.width()  - 1.0);
			y = std::clamp(x, 0.0, (double) _plane.height() - 1.0);

			/* Figure out our neighbourhood. */
			T t00 = _plane.at(std::floor(x), std::ceil(y));
			T t01 = _plane.at(std::floor(x), std::floor(y));
			T t10 = _plane.at(std::ceil(x),  std::ceil(y));
			T t11 = _plane.at(std::ceil(x),  std::floor(y));

			/* 4-way interpolate it. */
			double a = x - std::floor(x);
			double b = y - std::floor(y);

			S s0 = _slope(t01, t00);
			S s1 = _slope(t11, t10);

			T u0 = s0.at(b);
			T u1 = s1.at(b);

			S s2 = _slope(u0, u1);
			return s2.at(a);
		}
		T at(float x, float y) const { return at((double) x, (double) y); }
	};

	template<typename P, typename S>
		requires Slope<S, P>
	class Raster
	{
	public:
		/* Given a point, this function applies a transformation to it, 
		 * returning the resulting transformed point.
		 *
		 * # Synchronization
		 * This is expected to behave as a pure function, thus, no 
		 * synchronization guarantees are made about its invocations.
		 */
		std::function<P(P)> transform;

		/* Given a point, project it. */
		std::function<P(P)> project;

		/* Given a point, returns a tuple with the coordinates of this point
		 * in screen space.
		 *
		 * # Synchronization
		 * Same as transform.
		 */
		std::function<std::tuple<int32_t, int32_t>(P)> screen;

		/* Given a pair of points, create a slope that goes from one to the 
		 * first to the second point.
		 *
		 * # Synchronization
		 * Same as transform. 
		 */
		std::function<S(P, P)> slope;

		/* Scissor rectangle used to determine the bounds of the drawing scope.
		 * Setting this helps avoid useless pixel calculations that would happen
		 * outside of the screen area. 
		 *
		 * The value returned by this function is the (Left, Right, Top Bottom)
		 * tuple for the bounds of the clipping rectangle. */
		std::function<std::tuple<uint32_t, uint32_t, uint32_t, uint32_t>()> scissor;

		/* Tesselation function.
		 * This function is responsible for generating a number of output triangles 
		 * from a given triangle input. */
		std::function<void(P, P, P, std::function<void(P, P, P)>)> tesselation;

		/* This function gets called at the bottom of the raster pipeline and
		 * to it is given a position X in screen space, followed by a position 
		 * Y, also in screen space, followed by an interpollated coordinate for
		 * the current pixel in the space resulting from the previous 
		 * transformation.
		 *
		 * # Interpolation
		 * The interpolation is done by linearly interpolating each dimension of
		 * the point independently.
		 *
		 * # Synchronization
		 * It is guaranteed that no two simultaneous invocations of this 
		 * function will have the same values for X and Y. These two values can, 
		 * therefore, be used as, for example, indices to individual pixels 
		 * without the need for external synchronization.
		 */
		std::function<void(uint32_t, uint32_t, P)> painter;
	protected:	
		/* Triangle point bundle. */
		struct Triangle
		{
			P point0;
			P point1;
			P point2;
		};

		/* This is the thread pool that will be running the rasterization tasks
		 * on each and every one of our submitted triangles. */
		thread_pool pool;
	protected:
		/* Set up the rasterization by clipping the input triangle. */
		void clip_rasterize(Triangle t)
		{
			P a, b, c;

			a = t.point0;
			b = t.point1;
			c = t.point2;

			a = this->transform(a);
			b = this->transform(b);
			c = this->transform(c);

			this->tesselation(
				a, b, c,
				[&](P i, P j, P k)
				{
					Triangle t;
					t.point0 = i;
					t.point1 = j;
					t.point2 = k;
					this->rasterize(t);
				});
		}

		/* Actually perform the raster operation using the given triangle. */
		void rasterize(Triangle t)
		{
			P a, b, c;

			a = t.point0;
			b = t.point1;
			c = t.point2;

			a = this->project(a);
			b = this->project(b);
			c = this->project(c);

			auto [x0, y0, x1, y1, x2, y2] = std::tuple_cat(
				this->screen(a),
				this->screen(b),
				this->screen(c));

			/* Sort the points primarily by increasing Y and, secondy, by increasing X. */
			if(std::tie(y0, x0) > std::tie(y1, x1)) { std::swap(a, b); std::swap(y0, y1); std::swap(x0, x1); }
			if(std::tie(y1, x1) > std::tie(y2, x2)) { std::swap(b, c); std::swap(y1, y2); std::swap(x1, x2); }
			if(std::tie(y0, x0) > std::tie(y1, x1)) { std::swap(a, b); std::swap(y0, y1); std::swap(x0, x1); }

			/* Side of the shortest slope. */
			bool shortside = (y1 - y0) * (x2 - x0) < (x1 - x0) * (y2 - y0);

			S slopes[2] = 
			{
				/* Yes, the initializer list is required here because otherwise
				 * C++ would complain it needs S() to be valid. */
				shortside == 0 ? this->slope(a, b) : this->slope(a, c),
				shortside == 1 ? this->slope(a, b) : this->slope(a, c)
			};

			/* Get the scissor. */
			auto [left, right, top, bottom] = this->scissor();

			auto ye = y1;
			auto yt = y0;
			for(int32_t y = std::max(y0, (int32_t) top); y <= (int32_t) bottom; ++y)
			{
				if(y >= ye)
				{
					if(ye >= y2) 
						/* We've reached the end of the second bend. */
						break;

					/* We're at the end of the first bend, change the slopes. */
					ye = y2;
					yt = y1;
					slopes[shortside] = this->slope(b, c);
				}

				double posY = (double) (y - y0) / (double) (y2 - y0);
				double posR = (double) (y - yt) / (double) (ye - yt);
				auto p0 = slopes[ shortside].at(posR);
				auto p1 = slopes[!shortside].at(posY);

				auto x0 = std::get<0>(this->screen(p0));
				auto x1 = std::get<0>(this->screen(p1));
				if(x0 > x1) { std::swap(x0, x1); std::swap(p0, p1); }

				S slope = this->slope(p0, p1);
				for(int32_t x = std::max(x0, (int32_t) left); x < x1 && x <= (int32_t) right; ++x)
				{
					double posX = (double) (x - x0) / (double) (x1 - x0);
					auto p = slope.at(posX);
					
					/* In order for this operation to be safe, it has to be 
					 * guaranteed that no other triangle operation is currently
					 * modifying this one pixel value. 
					 *
					 * TODO: Actually implement the synchronization here. */
					if(x < 0 || y < 0 || x > (int32_t) right || y > (int32_t) bottom )
						throw std::runtime_error("invalid pixel shader invocation coordinate");
					this->painter((uint32_t) x, (uint32_t) y, p);
				}
			}
		}

		/* Calculate an approximate double area value for the triangle. */
		uint64_t darea(const Triangle t)
		{
			auto [x0, y0, x1, y1, x2, y2] = std::tuple_cat(
				this->screen(t.point0),
				this->screen(t.point1),
				this->screen(t.point2));

			uint32_t width  = std::max(x0, std::max(x1, x2)) - std::min(x0, std::min(x1, x2));
			uint32_t height = std::max(y0, std::max(y1, y2)) - std::min(y0, std::min(y1, y2));
			
			return width * height; 
		}

		/* Divides a triangle into two smaller triangles, each with half the 
		 * area of the original triangle. */
		void bissect(const Triangle source, Triangle& t0, Triangle& t1)
		{
			Triangle a0, a1, b0, b1;
			uint64_t area, max;

			auto make = [&](P p0, P p1, P p2, Triangle& t0, Triangle& t1)
			{
				auto slope  = this->slope(p0, p1);
				auto middle = slope.at(0.5);

				t0.point0 = p0;
				t0.point1 = middle;
				t0.point2 = p2;

				t1.point0 = middle;
				t1.point1 = p1;
				t1.point2 = p2;

				return std::max(darea(t0), darea(t1));
			};

			max = make(source.point0, source.point1, source.point2, a0, a1);
			area = make(source.point1, source.point2, source.point0, b0, b1);
			if(area > max)
			{
				std::swap(a0, b0);
				std::swap(a1, b1);
				max = area;
			}
			area = make(source.point2, source.point0, source.point1, b0, b1);
			if(area > max)
			{
				std::swap(a0, b0);
				std::swap(a1, b1);
				max = area;
			}

			t0 = a0;
			t1 = a1;
		}
	
	public:
		Raster()
			/* Create the thread pool. 
			 * 
			 * This will create a new unbalanced thread pool (which should not
			 * be a problem, given it's work stealing) with as many workers as
			 * there are hardware threads. */
			: pool(thread_pool::default_concurrency())
		{
			/* Since C++ gets really bloated and hard to read at the mildest of
			 * things and, no doubt, to generate a compile time error for the 
			 * uninitialized functions it would turn this code unto an 
			 * unreadable piece of satire.
			 *
			 * And strings have to be encoded in the input encodingn because
			 * C++20 saw fit to effectively destroy all interop with UTF-8
			 * strings by making `char8_t` its own special snowflake type, thus
			 * rendering UTF-8 support in C++20 a fucking joke.
			 * GOD I HATE THIS LANGUAGE SO MUCH
			 *
			 * std::function by default will throw `bad_function_call` when
			 * trying to invoke a default-initialized function. I find that to
			 * be still a bit too unclear for my taste, so I'll just do custom
			 * runtime exception messages for this. */
			this->transform = [](P) -> P
			{
				auto what = u8"raster call missing transform function"_fb;
				throw std::runtime_error(what);
			};
			this->screen = [](P) -> std::tuple<uint32_t, uint32_t> 
			{
				auto what = u8"raster call missing screen space fucntion"_fb;
				throw std::runtime_error(what);
			};
			this->slope = [](P, P) -> S
			{
				auto what = u8"raster call missing slope creator function"_fb;
				throw std::runtime_error(what);
			};
			this->scissor = []() -> std::tuple<uint32_t, uint32_t, uint32_t, uint32_t>
			{
				auto what = u8"raster call missing scissor function"_fb;
				throw std::runtime_error(what);
			};
			this->tesselation = [](P, P, P, std::function<void(P, P, P)>) -> void
			{
				auto what = u8"raster call missing tesselation function"_fb;
				throw std::runtime_error(what);
			};
			this->painter = [](auto, auto, auto)
			{
				auto what = u8"raster call missing painter function"_fb;
				throw std::runtime_error(what);
			};
		}

		/* Dispatches the rendering of a triangle, given the coordinates for its
		 * three vertices. This function returns a future that will be complete
		 * when the triangle has been completely drawn. */
		void dispatch(P p0, P p1, P p2, std::vector<std::future<void>>& futures)
		{
			/* Build the triangle structure. */
			Triangle triangle = 
			{
				.point0 = p0,
				.point1 = p1,
				.point2 = p2
			};

			/* Tesselation step.
			 * 
			 * We first perform a tesselation step on the triangle such that no
			 * CPU core ever has too much work on its hands trying to render a 
			 * single triangle that happens to cover the entire screen. */
			#define TESSEL_AREA_THRESHOLD (1024 * 64)

			uint64_t area = darea(triangle);
			if(area > TESSEL_AREA_THRESHOLD)
			{
				Triangle t0, t1;
				bissect(triangle, t0, t1);
			
				if(darea(t0) + darea(t1) <= area)
				{
					dispatch(t0.point0, t0.point1, t0.point2, futures);
					dispatch(t1.point0, t1.point1, t1.point2, futures);
				}
			}

			/* We're satistifed with the size of the fragment. So submit it. */
			auto task = make_task<void>([triangle, this]() 
			{	
				this->clip_rasterize(triangle);
			});
			futures.push_back(this->pool.submit_task(task));
		}
	};

	/* Primitive input type used by the Mesh to build triangles from index data.
	 * These variants control how the input will be used and how much imput is 
	 * needed for every new triangle. */
	enum class Primitive
	{
		/* Every triplet of vertices describes a new triangle. */
		TriangleList,
		/* Every three consecutive vertices describe a new triangle. */
		TriangleStrip
	};

	/* Geometry mesh draw command.
	 *
	 * This class holds a point buffer and an index buffer, both are used to 
	 * build the triangles that will get submitted to the dispatch function of a
	 * given renderer. */
	template<typename P>
	class Mesh
	{
	protected:
		/* The vertex point data for this mesh. */
		const std::vector<P>& _vertices;
		
		/* The indices used to assemble the vertex data into triangles. */
		const std::vector<size_t>& _indices;

		/* How the index data will be used to assemble triangle.s */
		Primitive _primitive = Primitive::TriangleStrip;
	public:
		/* Create a new mesh object with the given vertex and index data. */
		Mesh(const std::vector<P>& vertices, const std::vector<size_t>& indices)
			: _vertices(vertices), _indices(indices)
		{ }
	
		/* Create a new mesh object with the given vertex and idex data, along
		 * with the given primitive assembler type. */
		Mesh(
			const std::vector<P>& vertices, 
			const std::vector<size_t>& indices,
			Primitive primitive)
			: _vertices(vertices), _indices(indices), _primitive(primitive)
		{ }
	protected:
		/* Assembles the input in triangle list mode and actually performs all 
		 * of the dispatch operations on the triangles. */
		template<typename S>
			requires Slope<S, P>
		void dispatch_triangle_list(
			Raster<P, S>& raster, 
			std::vector<std::future<void>>& futures) const
		{	
			if(_indices.size() % 3 != 0)
			{
				std::cerr << "warning: mesh in triangle list mode will have ";
				std::cerr << "its trailing " << _indices.size() % 3 << " ";
				std::cerr << "indices ignored for not having a multiple of 3";
				std::cerr << std::endl;
			}
			if(_indices.size() / 3 == 0) 
			{
				/* No work to do. */
				std::cerr << "warning: submitted mesh with no completable work";
				std::cerr << std::endl;
				return;
			}

			for(size_t i = 0; i + 2 < _indices.size(); i += 3)
			{
				P p0, p1, p2;
				p0 = _vertices[_indices[i + 0]];
				p1 = _vertices[_indices[i + 1]];
				p2 = _vertices[_indices[i + 2]];

				raster.dispatch(p0, p1, p2, futures);
			}
		}

		/* Assembles the input in triangle strip mode and actually performs all
		 * of the dispatch operations on the triangles. */
		template<typename S>
			requires Slope<S, P>
		void dispatch_triangle_strip(
			Raster<P, S>& raster, 
			std::vector<std::future<void>>& futures) const
		{
			if(_indices.size() / 3 == 0) 
			{
				/* No work to do. */
				std::cerr << "warning: submitted mesh with no completable work";
				std::cerr << std::endl;
				return;
			}

			for(size_t i = 0; i + 2 < _indices.size(); ++i)
			{
				P p0, p1, p2;
				p0 = _vertices[_indices[i + 0]];
				p1 = _vertices[_indices[i + 1]];
				p2 = _vertices[_indices[i + 2]];

				raster.dispatch(p0, p1, p2, futures);
			}
		}

	public:
		/* Assemble the the geometry in this mesh into triangles and dispatch
		 * them to the given raster, saving the futures of the operation in the
		 * given vector. 
		 *
		 * This function does not block waiting for the render operation to
		 * complete. If that is what you want, use `draw()` instead. */
		template<typename S>
			requires Slope<S, P>
		void dispatch(
			Raster<P, S>& raster, 
			std::vector<std::future<void>>& futures) const
		{
			switch(_primitive)
			{
			case Primitive::TriangleList: 
				dispatch_triangle_list(raster, futures);
				break;
			case Primitive::TriangleStrip: 
				dispatch_triangle_strip(raster, futures);
				break;
			}
		}
		
		/* Assemble the the geometry in this mesh into triangles and dispatch
		 * them to the given raster. This function blocks waiting for the mesh
		 * to be fully drawn. */
		template<typename S>
			requires Slope<S, P>
		void draw(Raster<P, S>& raster) const
		{
			std::vector<std::future<void>> commands;
			dispatch(raster, commands);

			for(size_t i = 0; i < commands.size(); ++i)
				commands[i].wait();
		}
	};
}
