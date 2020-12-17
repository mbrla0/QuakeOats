
#include <SFML/Window.hpp>		/* For Window functionality.	*/
#include <SFML/Graphics.hpp>	/* For showing what we draw.	*/

import gfx;		/* For graphics functions.	*/
import game;	/* For the game.			*/

/* Use 32-bit RGBA as our pixel format. */
using Pixel = gfx::PixelRgba32;

#define WIDTH  (640)	/* Frame buffer width in pixels.	*/
#define HEIGHT (480)	/* Frame buffer height in pixels.	*/

int main(void)
{
	sf::VideoMode video(WIDTH, HEIGHT);
	sf::RenderWindow window(video, u8"Quake Oats");

	/* Create a graphics plane for the actual graphics operations and alias its
	 * memory to an SFML image for upload to the window system. This is safe 
	 * because the image will only ever be read from when the plane has already
	 * been completed. */
	gfx::Plane<Pixel> color(WIDTH, HEIGHT);
	sf::Image image(WIDTH, HEIGHT, color.data());

	/* Initialize the game. */
	game::Game game(color);
	while(!game.exit())
	{
		game.iterate();
	}

	return 0;
}

