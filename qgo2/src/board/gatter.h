/***************************************************************************
 *   Copyright (C) 2006 by EB   *
 *      *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/


#ifndef GATTER_H
#define GATTER_H

#include "imagehandler.h"

#include <vector>

#include <QtGui>

class Gatter
{
public:
	Gatter(QGraphicsScene *Canvas, int board_size);
	~Gatter();
	void hide (int x, int y);
	void show (int x, int y);
	void resize(int offsetX, int offsetY, int square_size);
	void showAll();

private:
	int board_size;
	QGraphicsScene *canvas;
	std::vector< std::vector<QGraphicsLineItem *> > VGatter, HGatter ;
	QMap<int,QGraphicsEllipseItem*> hoshisList ;
};

#endif
