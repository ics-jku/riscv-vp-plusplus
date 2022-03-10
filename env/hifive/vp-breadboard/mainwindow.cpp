#include "mainwindow.h"
#include <qpainter.h>
#include <QKeyEvent>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <cassert>
#include <iostream>
#include "ui_mainwindow.h"

#include <unistd.h>  //sleep

using namespace std;

void Sevensegment::draw(QPainter& p) {
	p.save();
	QPen segment(QColor("#f72727"), linewidth, Qt::PenStyle::SolidLine, Qt::PenCapStyle::RoundCap, Qt::RoundJoin);
	p.setPen(segment);

	//  0
	// 5   1
	//  6
	// 4   2
	//  3   7
	// printf(" %c\n", map & 1 ? '_' : ' ');
	// printf("%c %c\n", map & (1 << 5) ? '|' : ' ', map & (1 << 1) ? '|' : '
	// '); printf(" %c\n", map & (1 << 6) ? '-' : ' '); printf("%c %c\n", map &
	// (1 << 5) ? '|' : ' ', map & (1 << 1) ? '|' : ' '); printf(" %c  %c\n",
	// map & (1 << 3) ? '-' : ' ', map & (1 << 7) ? '.' : ' ');

	int xcol1 = 0;
	int xcol2 = 3 * (extent.x() / 4);
	int yrow1 = 0;
	int xrow1 = extent.x() / 4 - 2;
	int yrow2 = extent.y() / 2;
	int xrow2 = extent.x() / 8 - 1;
	int yrow3 = extent.y();
	int xrow3 = 0;

	if (map & 0b00000001)  // 0
		p.drawLine(offs + QPoint(xcol1 + xrow1, yrow1), offs + QPoint(xcol2 + xrow1, yrow1));
	if (map & 0b00000010)  // 1
		p.drawLine(offs + QPoint(xcol2 + xrow1, yrow1), offs + QPoint(xcol2 + xrow2, yrow2));
	if (map & 0b00000100)  // 2
		p.drawLine(offs + QPoint(xcol2 + xrow2, yrow2), offs + QPoint(xcol2 + xrow3, yrow3));
	if (map & 0b00001000)  // 3
		p.drawLine(offs + QPoint(xcol2 + xrow3, yrow3), offs + QPoint(xcol1 + xrow3, yrow3));
	if (map & 0b00010000)  // 4
		p.drawLine(offs + QPoint(xcol1 + xrow3, yrow3), offs + QPoint(xcol1 + xrow2, yrow2));
	if (map & 0b00100000)  // 5
		p.drawLine(offs + QPoint(xcol1 + xrow2, yrow2), offs + QPoint(xcol1 + xrow1, yrow1));
	if (map & 0b01000000)  // 6
		p.drawLine(offs + QPoint(xcol1 + xrow2, yrow2), offs + QPoint(xcol2 + xrow2, yrow2));
	if (map & 0b10000000)  // 7
		p.drawPoint(offs + extent);

	p.restore();
}

void RGBLed::draw(QPainter& p) {
	if (!map) {
		return;
	}
	p.save();
	QPen led(QColor(map & 1 ? 255 : 0, map & (1 << 1) ? 255 : 0, map & (1 << 2) ? 255 : 0, 0xC0), linewidth,
	         Qt::PenStyle::SolidLine, Qt::PenCapStyle::RoundCap, Qt::RoundJoin);
	p.setPen(led);

	p.drawPoint(offs);

	p.restore();
}

void OLED::draw(QPainter& p)
{
	p.fillRect(QRect(offs, QSize((ss1106::width - 2 * ss1106::padding_lr) * scale + margin.x()*2, ss1106::height * scale + margin.y()*2)), Qt::SolidPattern);

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

VPBreadboard::VPBreadboard(const char* configfile, const char* host, const char* port, QWidget* mparent)
    : QWidget(mparent),
      host(host),
      port(port),
      sevensegment(nullptr),
      rgbLed(nullptr),
      oled(nullptr){

	memset(buttons, 0, max_num_buttons * sizeof(Button*));

	QFile confFile(configfile);
    if (!confFile.open(QIODevice::ReadOnly)) {
        std::cerr << "Could not open config file " << configfile << std::endl;
        exit(-4);
    }

    QByteArray  raw_file = confFile.readAll();
    /*
    for(unsigned i = 0; i < raw_file.size(); i++)
    {
    	cout << raw_file.data()[i];
    }
    cout << endl;
    */
    QJsonParseError error;
    QJsonDocument json_doc = QJsonDocument::fromJson(raw_file, &error);
    if(json_doc.isNull())
    {
    	cerr << "Config seems to be invalid: ";
    	cerr << error.errorString().toStdString() << endl;
    	return;
    }
    QJsonObject config = json_doc.object();

	QPixmap bkgnd(config["background"].toString(""));
	if(bkgnd.isNull())
	{
		cerr << "invalid background " << config["background"].toString().toStdString() << endl;
		return;
	}

	QSize size(config["windowsize"].toArray().at(0).toInt(800),
			   config["windowsize"].toArray().at(1).toInt(600));


	bkgnd = bkgnd.scaled(size, Qt::IgnoreAspectRatio);
	QPalette palette;
	palette.setBrush(QPalette::Background, bkgnd);
	this->setPalette(palette);
	setFixedSize(size);

	if(config.contains("sevensegment"))
	{
		QJsonObject obj = config["sevensegment"].toObject();
		sevensegment = new Sevensegment(
			QPoint(obj["offs"].toArray().at(0).toInt(312),
			       obj["offs"].toArray().at(1).toInt(353)),
			QPoint(obj["extent"].toArray().at(0).toInt(36),
			       obj["extent"].toArray().at(1).toInt(50)),
			obj["linewidth"].toInt(7)
			);
	}
	if(config.contains("rgb"))
	{
		QJsonObject obj = config["rgb"].toObject();
		rgbLed = new RGBLed(
			QPoint(obj["offs"].toArray().at(0).toInt(89),
			       obj["offs"].toArray().at(1).toInt(161)),
			obj["linewidth"].toInt(15)
			);
	}
	if(config.contains("oled"))
	{
		QJsonObject obj = config["oled"].toObject();
		oled = new OLED(
			QPoint(obj["offs"].toArray().at(0).toInt(450),
			       obj["offs"].toArray().at(1).toInt(343)),
			obj["margin"].toInt(15),
			obj["scale"].toDouble(1.));
	}
	if(config.contains("buttons"))
	{
		QJsonArray butts = config["buttons"].toArray();
		for(unsigned i = 0; i < butts.size() && i < max_num_buttons; i++)
		{
			QJsonObject butt = butts[i].toObject();
			buttons[i] = new Button{
				QRect{
					QPoint{butt["pos"].toArray().at(0).toInt(), butt["pos"].toArray().at(1).toInt()},
					QSize{butt["dim"].toArray().at(0).toInt(), butt["dim"].toArray().at(1).toInt()}
				},
				static_cast<uint8_t>(butt["pin"].toInt()),
				butt["key"].toString(QString("")),
				butt["name"].toString(QString("undef"))
			};
		}
	}
}

VPBreadboard::~VPBreadboard()
{
	if(sevensegment != nullptr)
		delete sevensegment;
	if(rgbLed != nullptr)
		delete rgbLed;
	if(oled != nullptr)
		delete oled;
	for(unsigned i = 0; i < max_num_buttons; i++)
	{
		if(buttons[i] != nullptr)
		delete buttons[i];
	}
}

void VPBreadboard::showConnectionErrorOverlay(QPainter& p) {
	p.save();
	p.setBrush(QBrush(QColor("black")));
	QRect sign;
	if(this->size().width() > this->size().height())
	{
		sign = QRect (QPoint(this->size().width()/4, this->size().height()/4), this->size()/2);
	}
	else
	{
		sign = QRect (QPoint(this->size().width()/10, this->size().height()/4),
				QSize(4*this->size().width()/5, this->size().height()/4));
	}
	p.drawRect(sign);
	p.setFont(QFont("Arial", 25, QFont::Bold));
	QPen penHText(QColor("red"));
	p.setPen(penHText);
	p.drawText(sign, QString("No connection"), Qt::AlignHCenter | Qt::AlignVCenter);
	p.restore();
}

uint64_t VPBreadboard::translateGpioToExtPin(GpioCommon::Reg reg) {
	uint64_t ext = 0;
	for (uint64_t i = 0; i < 24; i++)  // Max Pin is 32, see SiFive HiFive1
	                                   // Getting Started Guide 1.0.2 p. 20
	{
		// cout << i << " to ";;
		if (i >= 16) {
			ext |= (reg & (1l << i)) >> 16;
			// cout << i - 16 << endl;
		} else if (i <= 5) {
			ext |= (reg & (1l << i)) << 8;
			// cout << i + 8 << endl;
		} else if (i >= 9 && i <= 13) {
			ext |= (reg & (1l << i)) << 6;  // Bitshift Ninja! (with broken
			                                // legs) cout << i + 6 << endl;
		}
		// rest is not connected.
	}
	return ext;
}

uint8_t VPBreadboard::translatePinNumberToSevensegment(uint64_t pinmap) {
	uint8_t ret = 0;
	static uint8_t pinMapping[8] =
	{
	  15, 16, 17, 18, 19, 7, 6, 5
	};
	for(unsigned i = 0; i < 8; i++)
	{
		ret |= pinmap & (1 << pinMapping[i]) ? (1 << i) : 0;
	}
	return ret;
}

uint8_t VPBreadboard::translatePinNumberToRGBLed(uint64_t pinmap) {
	uint8_t ret = 0;
	ret |= (~pinmap & (1 << 6)) >> 6;  // R
	ret |= (~pinmap & (1 << 3)) >> 2;  // G
	ret |= (~pinmap & (1 << 5)) >> 3;  // B
	return ret;
}

uint8_t VPBreadboard::translatePinToGpioOffs(uint8_t pin) {
	if (pin < 8) {
		return pin + 16;  // PIN_0_OFFSET
	}
	if(pin >= 8 && pin < 14) {
		return pin - 8;
	}
	//ignoring non-wired pin 14 <==> 8
	if(pin > 14 && pin < 20){
		return pin - 6;
	}

	return 0;
}

void printBin(char* buf, uint8_t len) {
	for (uint16_t byte = 0; byte < len; byte++) {
		for (int8_t bit = 7; bit >= 0; bit--) {
			printf("%c", buf[byte] & (1 << bit) ? '1' : '0');
		}
		printf(" ");
	}
	printf("\n");
}

void VPBreadboard::paintEvent(QPaintEvent*) {
	QPainter painter(this);

	if (!inited || !gpio.update()) {
		inited = gpio.setupConnection(host, port);
		showConnectionErrorOverlay(painter);
		if (!inited)
			usleep(500000);
		this->update();
		return;
	}

	painter.setRenderHint(QPainter::Antialiasing);
	painter.setRenderHint(QPainter::HighQualityAntialiasing);

	if(oled)
		oled->draw(painter);

	if(sevensegment)
	{
		sevensegment->map = translatePinNumberToSevensegment(translateGpioToExtPin(gpio.state));
		sevensegment->draw(painter);
	}

	if(rgbLed)
	{
		rgbLed->map = translatePinNumberToRGBLed(translateGpioToExtPin(gpio.state));
		rgbLed->draw(painter);
	}

	//buttons
	painter.save();
	QColor dark("#101010");
	dark.setAlphaF(0.5);
	painter.setBrush(QBrush(dark));
	if(debugmode)
	{
		painter.setPen(QPen(QColor("red")));
		painter.setFont(QFont("Arial", 12));
	}
	for(unsigned i = 0; i < max_num_buttons; i++)
	{
		if(!buttons[i])
			break;
		if(buttons[i]->pressed || debugmode)
			painter.drawRect(buttons[i]->area);
		if(debugmode)
			painter.drawText(buttons[i]->area, buttons[i]->name, Qt::AlignHCenter | Qt::AlignVCenter);
	}
	painter.restore();


	if (debugmode) {
		if(sevensegment)
			painter.drawRect(QRect(sevensegment->offs, QSize(sevensegment->extent.x(), sevensegment->extent.y())));
	}
	painter.end();
	// intentional slow down
	// TODO: update at fixed rate, async between redraw and gpioserver
	usleep(10000);
	this->update();
}

void VPBreadboard::notifyChange(bool success) {
	assert(success);
	update();
}

void VPBreadboard::keyPressEvent(QKeyEvent* e) {
	this->update();
	// scout << "Yee, keypress" << endl;

	if(debugmode)
	{
		switch(e->key())
		{
		case Qt::Key_Right:
			if(buttons[++moving_button] == nullptr || moving_button >= max_num_buttons)
				moving_button = 0;
			if(buttons[moving_button] == nullptr)
			{
				cout << "No Buttons available" << endl;
			}
			else
			{
				cout << "Moving button " << buttons[moving_button]->name.toStdString() << endl;
			}
			break;

		case Qt::Key_W:
			if(buttons[moving_button] == nullptr)
				break;
			buttons[moving_button]->area.moveTopLeft(buttons[moving_button]->area.topLeft() - QPoint(0, 1));
			cout << buttons[moving_button]->name.toStdString() << " ";
			cout << "X: " << buttons[moving_button]->area.topLeft().x() << " Y: " << buttons[moving_button]->area.topLeft().y() << endl;
			break;
		case Qt::Key_A:
			if(buttons[moving_button] == nullptr)
				break;
			buttons[moving_button]->area.moveTopLeft(buttons[moving_button]->area.topLeft() - QPoint(1, 0));
			cout << buttons[moving_button]->name.toStdString() << " ";
			cout << "X: " << buttons[moving_button]->area.topLeft().x() << " Y: " << buttons[moving_button]->area.topLeft().y() << endl;
			break;
		case Qt::Key_S:
			if(buttons[moving_button] == nullptr)
				break;
			buttons[moving_button]->area.moveTopLeft(buttons[moving_button]->area.topLeft() + QPoint(0, 1));
			cout << buttons[moving_button]->name.toStdString() << " ";
			cout << "X: " << buttons[moving_button]->area.topLeft().x() << " Y: " << buttons[moving_button]->area.topLeft().y() << endl;
			break;
		case Qt::Key_D:
			if(buttons[moving_button] == nullptr)
				break;
			buttons[moving_button]->area.moveTopLeft(buttons[moving_button]->area.topLeft() + QPoint(1, 0));
			cout << buttons[moving_button]->name.toStdString() << " ";
			cout << "X: " << buttons[moving_button]->area.topLeft().x() << " Y: " << buttons[moving_button]->area.topLeft().y() << endl;
			break;
		case Qt::Key_Space:
			cout << "Debug mode off" << endl;
			debugmode = 0;
			break;
		default:
			break;
		}
	}
	else
	{
		//normal mode
		switch (e->key()) {
			case Qt::Key_Escape:
			case Qt::Key_Q:
				QApplication::quit();
				break;
			case Qt::Key_0: {
				uint8_t until = 6;
				for (uint8_t i = 0; i < 8; i++) {
					gpio.setBit(i, i < until ? GpioCommon::Tristate::HIGH : GpioCommon::Tristate::LOW);
				}
				break;
			}
			case Qt::Key_1: {
				for (uint8_t i = 0; i < 8; i++) {
					gpio.setBit(i, GpioCommon::Tristate::LOW);
				}
				break;
			}
			case Qt::Key_Space:
				cout << "Set Debug mode" << endl;
				debugmode = true;
				break;
			default:
				for(unsigned i = 0; i < max_num_buttons; i++)
				{
					if(buttons[i] == nullptr)
						break;	//this is sorted somewhat

					if (buttons[i]->keybinding == e->key()) {
						gpio.setBit(translatePinToGpioOffs(buttons[i]->pin), GpioCommon::Tristate::LOW);  // Active low
						buttons[i]->pressed = true;
					}
				}
				break;
		}
	}
}

void VPBreadboard::keyReleaseEvent(QKeyEvent* e)
{
	for(unsigned i = 0; i < max_num_buttons; i++)
	{
		if(buttons[i] == nullptr)
			break;	//this is sorted somewhat

		if (buttons[i]->keybinding == e->key()) {
			gpio.setBit(translatePinToGpioOffs(buttons[i]->pin),GpioCommon::Tristate::HIGH);
			buttons[i]->pressed = false;
		}
	}
}

void VPBreadboard::mousePressEvent(QMouseEvent* e) {
	if (e->button() == Qt::LeftButton) {
		for(unsigned i = 0; i < max_num_buttons; i++)
		{
			if(buttons[i] == nullptr)
				break;	//this is sorted somewhat

			if (buttons[i]->area.contains(e->pos())) {
				//cout << "button " << i << " click!" << endl;
				gpio.setBit(translatePinToGpioOffs(buttons[i]->pin), GpioCommon::Tristate::LOW);  // Active low
				buttons[i]->pressed = true;
			}
		}
		// cout << "clicked summin elz" << endl;
	} else {
		cout << "Whatcha doin' there?" << endl;
	}
	this->update();
	e->accept();
}

void VPBreadboard::mouseReleaseEvent(QMouseEvent* e) {
	if (e->button() == Qt::LeftButton) {
		for(unsigned i = 0; i < max_num_buttons; i++)
		{
			if(buttons[i] == nullptr)
				break;	//this is sorted somewhat
			if (buttons[i]->area.contains(e->pos())) {
				//cout << "button " << i << " release!" << endl;
				gpio.setBit(translatePinToGpioOffs(buttons[i]->pin), GpioCommon::Tristate::HIGH);
				buttons[i]->pressed = false;
			}
		}
		// cout << "released summin elz" << endl;
	} else {
		cout << "Whatcha doin' there?" << endl;
	}
	this->update();
	e->accept();
}
