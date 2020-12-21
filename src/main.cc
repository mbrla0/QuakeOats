
#include <SFML/Window.hpp>		/* For Window functionality.	*/
#include <SFML/Graphics.hpp>	/* For showing what we draw.	*/

import <iostream>;
import gfx;		/* For graphics functions.	*/
import game;	/* For the game.			*/
import str;		/* Haha UTF-8 go brr.		*/

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

	using Clock    = std::chrono::steady_clock;
	using Duration = std::chrono::duration<double, std::ratio<1>>;
	auto last_time = Clock::now();

	/* Run the game. */
	while(!game.exit())
	{
		static int32_t mouse_x = 0, mouse_y = 0;
		static bool valid_position = false;

		game.controller().mouse_x(0);
		game.controller().mouse_y(0);

		sf::Event event;
		while(window.pollEvent(event))
		{
			if(event.type == sf::Event::Closed)
				goto end;
			else if(event.type == sf::Event::KeyPressed)
				switch(event.key.code)
				{
				case sf::Keyboard::Key::W: game.controller().forward(true);  break;
				case sf::Keyboard::Key::A: game.controller().left(true);     break;
				case sf::Keyboard::Key::S: game.controller().backward(true); break;
				case sf::Keyboard::Key::D: game.controller().right(true);    break;
				case sf::Keyboard::Key::C: game.controller().crouch(true);   break;
				default: break;
				}
			else if(event.type == sf::Event::KeyReleased)
				switch(event.key.code)
				{
				case sf::Keyboard::Key::W: game.controller().forward(false);  break;
				case sf::Keyboard::Key::A: game.controller().left(false);     break;
				case sf::Keyboard::Key::S: game.controller().backward(false); break;
				case sf::Keyboard::Key::D: game.controller().right(false);    break;
				case sf::Keyboard::Key::C: game.controller().crouch(false);   break;
				default: break;
				}
			else if(event.type == sf::Event::MouseButtonPressed)
			{
				if(event.mouseButton.button == sf::Mouse::Button::Left)
					game.controller().fire(true);
			}
			else if(event.type == sf::Event::MouseButtonReleased)
			{
				if(event.mouseButton.button == sf::Mouse::Button::Left)
					game.controller().fire(false);
			}
			else if(event.type == sf::Event::MouseEntered)
				valid_position = false;
			else if(event.type == sf::Event::MouseMoved)
			{
				if(valid_position)
				{
					game.controller().mouse_x_nudge(event.mouseMove.x - mouse_x);
					game.controller().mouse_y_nudge(event.mouseMove.y - mouse_y);
				}

				mouse_x = event.mouseMove.x;
				mouse_y = event.mouseMove.y;
				valid_position = true;
			}
			
		}

		auto now = Clock::now();
		auto delta = std::chrono::duration_cast<Duration>(last_time - now).count();
		last_time = now;

		game.iterate(delta);

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

