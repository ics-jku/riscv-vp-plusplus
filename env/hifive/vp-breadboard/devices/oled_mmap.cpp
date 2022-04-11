#include "oled_mmap.hpp"

void OLED_mmap::draw(QPainter& p)
{
	p.fillRect(
			QRect(offs, QSize((ss1106::width - 2 * ss1106::padding_lr) * scale + margin.x()*2, ss1106::height * scale + margin.y()*2)),
			QBrush(Qt::black, Qt::SolidPattern)
			);

	if(state->display_on && state->changed)
	{
		state->changed = 0;	//We ignore this small race-condition
		uchar* map = image.bits();
		for(unsigned page = 0; page < ss1106::height/8; page++)
		{
			for(unsigned column = 0; column < (ss1106::width - 2 * ss1106::padding_lr); column++)
			{
				for(unsigned page_pixel = 0; page_pixel < 8; page_pixel++)
				{
					map[((page * 8 + page_pixel) * (ss1106::width - 2 * ss1106::padding_lr)) + column] = state->frame[page][column+ss1106::padding_lr] & (1 << page_pixel) ? state->contrast : 0;
				}
			}
		}
	}
	if(state->display_on)
		p.drawImage(offs + margin, image.scaled((ss1106::width - 2 * ss1106::padding_lr) * scale, ss1106::height * scale));
}
