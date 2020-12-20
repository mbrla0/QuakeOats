module;
#include <glm/glm.hpp>	/* For mathematics. */
#include <glm/gtx/transform.hpp>
#include "thread_utils.hpp"

export module game;

import <concepts>;	/* For standard concepts.		*/
import <sstream>;	/* AAAAAAAAAAAAAAAAAAAAAAAAAAA.	*/
import gfx;			/* For planes and rasterizers.	*/
import map;			/* For asset loading.			*/

export namespace game
{
	/* Use 32-bit RGBA as our pixel format. */
	using Pixel = gfx::PixelRgba32;	

	/* A linerly interpolating slope compatible with gfx::Slope. */
	template<typename T>
		requires
		requires(T a, T b, float x) { { glm::mix(a, b, x) } -> std::same_as<T>; }
	struct Slope
	{
	protected:
		T a;
		T b;

	public:
		Slope(T a, T b)
			: a(a), b(b)
		{ }

		T at(float  x) const { return glm::mix(a, b, x); }
		T at(double x) const { return glm::mix(a, b, x); }
	};

	/* Vertex coordinates and attributes. */
	struct Vertex
	{
		glm::vec4 position;
		glm::vec4 color;

		bool operator ==(const Vertex& vert) const
		{
			return this->position == vert.position 
				&& this->color == vert.color;
		}
	};

	/* A linearly interpolating slope for all the parameter in a vertex. */
	struct VertexSlope
	{
		Slope<glm::vec4> position;
		Slope<glm::vec4> color;

		VertexSlope(Vertex a, Vertex b)
			: position(a.position, b.position),
			  color(a.color, b.color)
		{ }

		Vertex at(double x) const
		{
			Vertex vert;
			vert.position = position.at(x);
			vert.color    = color.at(x);

			return vert;
		}
		Vertex at(float  x) const { return this->at((double) x); } 
	};

	class Player
	{
		
	};

	/* Input mapper read by the game. */
	class Controller
	{
	protected:
		/* Mouse X and Y axis movement. */
		int32_t mx, my;

		/* Forward, backward, left and right. */
		bool fwd, bkw, lft, rht;

		/* Fire and chrouch. */
		bool fre, cch;
	public:
		int32_t mouse_x() const noexcept { return mx; }
		int32_t mouse_y() const noexcept { return my; }
		
		bool forward()  const noexcept { return fwd; }
		bool backward() const noexcept { return bkw; }
		bool left()     const noexcept { return lft; }
		bool right()    const noexcept { return rht; }
		bool fire()     const noexcept { return fre; }
		bool crouch()   const noexcept { return cch; }
		
		int32_t mouse_x_nudge(int32_t x) noexcept { return (mx += x); } 
		int32_t mouse_y_nudge(int32_t y) noexcept { return (my += y); } 

		int32_t mouse_x(int32_t x) noexcept { return (mx = x); }
		int32_t mouse_y(int32_t y) noexcept { return (my = y); }

		bool forward(bool v)  noexcept { return (fwd = v); }
		bool backward(bool v) noexcept { return (bkw = v); }
		bool left(bool v)     noexcept { return (lft = v); }
		bool right(bool v)    noexcept { return (rht = v); }
		bool fire(bool v)     noexcept { return (fre = v); }
		bool crouch(bool v)   noexcept { return (cch = v); }

	};

	class Game
	{
	protected:
		/* World space to screen space transformation matrix. */
		glm::mat4 projection;

		/* Ouput color screen buffer.
		 * 
		 * The backing storage of this buffer is aliased to the actual output 
		 * object, so any writes performed to this will propagate to the display
		 * once they're done. */
		gfx::Plane<Pixel> screen;

		/* Screen space depth buffer. */
		gfx::Plane<float> depth;
		
		/* Screen space fragment lock. 
		 * God is dead and we have killed him. */
		gfx::Plane<std::mutex> lock;

		/* World rasterizer.
		 *
		 * Use this rasterizer for objects that are placed in and that should be 
		 * affected by world transformations and effects.
		 */
		gfx::Raster<Vertex, VertexSlope> world;

		/* Input map. */
		Controller _controller;	
	public:
		Game(uint32_t width, uint32_t height)
			: screen(width, height), depth(width, height), lock(width, height)
		{
			/* Write our shaders. */
			world.transform = [](Vertex vert)
			{
				return vert;
			};
			world.screen = [&](Vertex vert)
			{ 
				glm::vec4 point = vert.position;
				return std::make_tuple(
					std::round((point.x + 1.0) * (float) screen.width()  / 2.0),
					std::round((point.y + 1.0) * (float) screen.height() / 2.0));
			};
			world.slope = [](Vertex a, Vertex b)
			{
				return VertexSlope(a, b);
			};
			world.painter = [&](uint32_t x, uint32_t y, Vertex p)
			{
				this->lock.at(x, y).lock();

				/* Clip pixels further that what we already have. */
				if(this->depth.at(x, y) < p.position.z)
				{
					this->lock.at(x, y).unlock();
					return;	
				}
				this->depth.at(x, y) = p.position.z;

				this->screen.at(x, y).red   = (uint8_t) (p.color.x * 255.0);
				this->screen.at(x, y).green = (uint8_t) (p.color.y * 255.0);
				this->screen.at(x, y).blue  = (uint8_t) (p.color.z * 255.0);
				this->screen.at(x, y).alpha = (uint8_t) (p.color.w * 255.0);

				this->lock.at(x, y).unlock();
			};
		}

		/* Reference to the controller interface for this game. */
		const Controller& controller() const noexcept { return _controller; }
		      Controller& controller()       noexcept { return _controller; }

		/* Perform one iteration of the game loop. */
		void iterate()
		{
			Pixel white;
			white.red   = 0x11;
			white.green = 0x11;
			white.blue  = 0x11;
			white.alpha = 0xff;

			screen.clear(white);
			depth.clear(+1.0 / 0.0);

			static double pos = 0.0;
			std::vector<Vertex> vertices(5);
			std::vector<size_t> indices(
			{
				0, 1, 4, 
				1, 2, 4,
				2, 3, 4, 
				3, 0, 4,
				0, 2, 3,
				0, 1, 2,
			});

			vertices[0].position = glm::vec4( 1.0,  1.0,  1.0, 1.0);
			vertices[1].position = glm::vec4(-1.0,  1.0,  1.0, 1.0);
			vertices[2].position = glm::vec4(-1.0,  1.0, -1.0, 1.0);
			vertices[3].position = glm::vec4( 1.0,  1.0, -1.0, 1.0);
			vertices[4].position = glm::vec4( 0.0, -1.0,  0.0, 1.0);

			vertices[0].color = glm::vec4(0.0, 1.0, 1.0, 1.0);
			vertices[1].color = glm::vec4(0.0, 0.0, 1.0, 1.0);
			vertices[2].color = glm::vec4(1.0, 0.0, 1.0, 1.0);
			vertices[3].color = glm::vec4(0.0, 1.0, 0.0, 1.0);
			vertices[4].color = glm::vec4(1.0, 1.0, 1.0, 1.0);

			pos += 0.04;
			double a = pos;

			gfx::Mesh mesh(vertices, indices, gfx::Primitive::TriangleList);

			this->world.transform = [a](Vertex v)
			{
				glm::mat4 t, p;
				t = glm::translate(glm::vec3(0.0, 0.0, -5.0));
				t = glm::rotate(t, glm::radians((float) a * 20), glm::vec3(0.0, 1.0, 0.0));
				p = glm::perspective(glm::radians(90.0), 4.0 / 3.0, 0.1, 100.0);

				v.position  = p * t * v.position;
				v.position /= v.position.w;
				return v;

			};
			mesh.draw(this->world);
		}

		constexpr bool exit() const
		{
			return false;
		}

		gfx::Plane<Pixel>& get_screen()
		{
			return this->screen;
		}

		const gfx::Plane<Pixel>& get_screen() const
		{
			return this->screen;
		}
	};
}
