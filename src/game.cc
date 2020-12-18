module;
#include <glm/glm.hpp>	/* For mathematics. */
#include "thread_utils.hpp"

export module game;

import <concepts>;	/* For standard concepts.		*/
import <sstream>;	/* AAAAAAAAAAAAAAAAAAAAAAAAAAA.	*/
import gfx;			/* For planes and rasterizers.	*/

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

	class Player
	{
		
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

		/* World rasterizer.
		 *
		 * Use this rasterizer for objects that are placed in and that should be 
		 * affected by world transformations and effects.
		 */
		gfx::Raster<glm::vec4, Slope<glm::vec4>> world;
	public:
		Game(gfx::Plane<Pixel> screen)
			: screen(screen)
		{
			/* Write our shaders. */
			world.transform = [](glm::vec4 vert)
			{
				return vert;
			};
			world.screen = [&](glm::vec4 point)
			{ 
				return std::make_tuple(
					(point.x + 1.0) * screen.width()  / 2.0,
					(point.y + 1.0) * screen.height() / 2.0);
			};
			world.slope = [](glm::vec4 a, glm::vec4 b)
			{
				return Slope(a, b);
			};
			world.painter = [&](uint32_t x, uint32_t y, glm::vec4 p)
			{
				this->screen.at(x, y).red   = (uint8_t) (p.w * 256);
				this->screen.at(x, y).green = (uint8_t) (p.w * 256);
				this->screen.at(x, y).blue  = (uint8_t) (p.w * 256);
				this->screen.at(x, y).alpha = 0xff;
			};
		}

		/* Perform one iteration of the game loop. */
		void iterate()
		{
			Pixel white;
			white.red   = 0xff;
			white.green = 0xff;
			white.blue  = 0xff;
			white.alpha = 0xff;

			screen.clear(white);

			glm::vec4 a(-1.0,  1.0, 1.0, 0.35);
			glm::vec4 b( 1.0,  1.0, 1.0, 0.5);
			glm::vec4 c( 0.0, -1.0, 1.0, 1.0);

			world.dispatch(a, b, c).wait();
		}

		constexpr bool exit() const
		{
			return false;
		}
	};
}
