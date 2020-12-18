
#include <SFML/Window.hpp>		/* For Window functionality.	*/
#include <SFML/Graphics.hpp>	/* For showing what we draw.	*/

//import <iostream>;
//import gfx;		/* For graphics functions.	*/
//import game;	/* For the game.			*/
//import str;		/* Haha UTF-8 go brr.		*/
#include <iostream>
#include "gfx.hpp"
#include "game.hpp"

/* Use 32-bit RGBA as our pixel format. */
using Pixel = gfx::PixelRgba32;

#define WIDTH  (640)	/* Frame buffer width in pixels.	*/
#define HEIGHT (480)	/* Frame buffer height in pixels.	*/

int main(void)
{
	sf::VideoMode video(WIDTH, HEIGHT);
	sf::String title((sf::Uint32*) U"QuakeOats");

	sf::RenderWindow window(video, title);
	window.setActive();
	window.setView(sf::View(sf::FloatRect(0.0, 0.0, WIDTH, HEIGHT)));

	/* Create a graphics plane for the actual graphics operations and alias its
	 * memory to an SFML image for upload to the window system. This is safe 
	 * because the image will only ever be read from when the plane has already
	 * been completed. */
	game::Game game(WIDTH, HEIGHT);


	/* Initialize the game. */
	while(!game.exit())
	{
		sf::Event event;
		while(window.pollEvent(event))
		{
			if(event.type == sf::Event::Closed)
				goto end;
		}
		game.iterate();

		sf::Image image;
		image.create(WIDTH, HEIGHT, (sf::Uint8*) game.get_screen().data());

		sf::Texture texture;
		texture.loadFromImage(image);
		
		sf::Sprite sprite(texture);
		sprite.setOrigin(0, 0);

		window.clear();
		window.draw(sprite);
		window.display();
	}
end:

	return 0;
}

