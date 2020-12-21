module;
#include <glm/glm.hpp>	/* For mathematics. */
#include <glm/gtx/transform.hpp>
#include "thread_utils.hpp"

export module game;

import <concepts>;	/* For standard concepts.		*/
import <sstream>;	/* AAAAAAAAAAAAAAAAAAAAAAAAAAA.	*/
import <fstream>;	/* For loading the map file.	*/
import <iostream>;	/* For debug output.			*/
import gfx;			/* For planes and rasterizers.	*/
import map;			/* For asset loading.			*/
import str;

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

	struct Player
	{
		glm::vec3 position;
		glm::vec3 velocity;
		glm::vec3 rotation;
		glm::vec3 scaling;
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
		gfx::Raster<map::Point, map::PointSlope> world;

		/* Input map. */
		Controller _controller;	

		/* World map. */
		map::Map world_map;

		/* Player variables. */
		Player player;
	public:
		Game(uint32_t width, uint32_t height)
			: screen(width, height), depth(width, height), lock(width, height)
		{ 
			/* Load the map. */
			std::ifstream map;
			map.open("assets/map0.map", std::ios_base::in | std::ios_base::binary);
			if(!map)
				throw std::runtime_error("could not open assets/map0.map");

			world_map = map::Map::load(map);


			projection = glm::perspective(glm::radians(90.0), 4.0 / 3.0, 1.0, 100.0);
			player.position = glm::vec3(0.0);
			player.velocity = glm::vec3(0.0);
			player.rotation = glm::vec3(0.0);
			player.scaling  = glm::vec3(1.0);
		}

		/* Reference to the controller interface for this game. */
		const Controller& controller() const noexcept { return _controller; }
		      Controller& controller()       noexcept { return _controller; }

		/* Perform one iteration of the game loop. */
		void iterate(double delta)
		{
			/* Update the position of the player. */
			static double angle = - 3.1415 / 2.0;
			player.position.x = 0.0;
			player.position.z = 0.0;
			player.position.y = -10.0;
			player.scaling = glm::vec3(1.0);
			/*player.rotation.x = 3.1415 / 2.0;*/
			player.rotation.y = angle;
			angle += 3.1415 / 4.0 * delta;

			/* Render the screen. */
			Pixel white;
			white.red   = 0x11;
			white.green = 0x11;
			white.blue  = 0x11;
			white.alpha = 0xff;

			screen.clear(white);
			depth.clear(+1.0 / 0.0);

			glm::mat4 view = glm::mat4(1.0);
			view = glm::rotate(view, player.rotation.x, glm::vec3(1.0, 0.0, 0.0));
			view = glm::rotate(view, player.rotation.y, glm::vec3(0.0, 1.0, 0.0));
			view = glm::rotate(view, player.rotation.z, glm::vec3(0.0, 0.0, 1.0));
			view = glm::scale(view, glm::vec3(
				1.0 / player.scaling.x,
				1.0 / player.scaling.y,
				1.0 / player.scaling.z));
			view = glm::translate(view, -player.position);

			for(auto& model : world_map.models())
			{
				auto transform = model.transformation();
				world.transform = [&](map::Point p)
				{
					glm::vec4 point = this->projection * view * transform * p.position;
					p.position = point;

					return p;
				};
				world.tesselation = [&](
					map::Point a, 
					map::Point b, 
					map::Point c, 
					std::function<void(map::Point, map::Point, map::Point)> dispatch)
				{
					glm::vec3 q(0.0, 0.0, 1.0);
					glm::vec3 n(0.0, 0.0, 1.0);
					
					auto ndot = [&](glm::vec3 p)
					{
						return glm::dot(n, p - q);
					};

					auto lncross = [&](glm::vec3 a, glm::vec3 b) -> std::optional<glm::vec3>
					{
						glm::vec3 v0 = a - q;
						glm::vec3 v1 = b - q;

						float d0 = glm::dot(n, v0);
						float d1 = glm::dot(n, v1);

						if(std::signbit(d0) == std::signbit(d1))
							/* Line segment doesn't cross the plane. */
							return {};

						float t = d0 / (d1 - d0);
						return a + t * (b - a);
					};

					auto lenrat = [](glm::vec3 a, glm::vec3 shrt, glm::vec3 lng)
					{
						float l = glm::length(lng  - a);
						float s = glm::length(shrt - a);

						return s / l;
					};
					
					uint32_t trigs = 0;
					map::Point points[4];

					auto p_lncross = [&](map::Point a, map::Point b) -> std::optional<map::Point>
					{
						std::optional<glm::vec3> ocross = lncross(
							a.position.xyz(), 
							b.position.xyz());
						if(!ocross) return {};
						
						glm::vec3 cross = ocross.value();
						float midf = lenrat(
							a.position.xyz(),
							cross,
							b.position.xyz());

						map::PointSlope slope(a, b);
						map::Point mid = slope.at(midf);

						return mid;
					};
					
					auto test = [&](map::Point p)
					{
						if(!std::signbit(ndot(p.position.xyz()))) 
						{
							if(trigs >= 4)
								throw std::runtime_error(u8"more than three points in triangle crossing"_fb);
							points[trigs++] = p;
						}
					};
				
					std::optional<map::Point> cross;
					test(a);
					if((cross = p_lncross(a, b))) test(cross.value());
					test(b);
					if((cross = p_lncross(b, c))) test(cross.value());
					test(c);
					if((cross = p_lncross(c, a))) test(cross.value());

					if(trigs == 3)
						dispatch(points[0], points[1], points[2]);
					else if(trigs == 4)
					{
						dispatch(points[0], points[1], points[2]);
						dispatch(points[1], points[2], points[3]);
					}

				};
				world.project = [&](map::Point p)
				{
					if(p.position.w <= 0.1) p.position.w = 0.1;

					p.position /= p.position.w;
					return p;
				};
				world.screen = [&](map::Point p)
				{
					int32_t x = std::round((p.position.x + 1.0) * (double) screen.width()  / 2.0);
					int32_t y = std::round((p.position.y + 1.0) * (double) screen.height() / 2.0);
					
					return std::make_tuple(x, y);
				};
				world.scissor = [&]()
				{
					/* Cut off-screen pixels. */
					return std::make_tuple(
						0, screen.width(),
						0, screen.height());
				};
				world.slope = [&](map::Point a, map::Point b)
				{
					return map::PointSlope(a, b);
				};
				world.painter = [&](uint32_t x, uint32_t y, map::Point p)
				{
					auto sampler = gfx::Sampler<gfx::PixelRgba32, gfx::PixelRgba32Slope>(
						world_map.textures().at(p.texture_index),
						[](gfx::PixelRgba32 a, gfx::PixelRgba32 b) {
							return gfx::PixelRgba32Slope(a, b);
						});

					lock.at(x, y).lock();
					if(depth.at(x, y) < p.position.z)
					{
						lock.at(x, y).unlock();
						return;
					}
					depth.at(x, y) = p.position.z;
					screen.at(x, y) = sampler.at(p.sampler.x, p.sampler.y);

					lock.at(x, y).unlock();
				};

				auto mesh = model.mesh();
				mesh.draw(world);
			}
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
