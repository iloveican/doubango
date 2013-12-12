/*
* Copyright (C) 2010-2011 Mamadou Diop.
*
* Contact: Mamadou Diop <diopmamadou(at)doubango.org>
*	
* This file is part of Open Source Doubango Framework.
*
* DOUBANGO is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*	
* DOUBANGO is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*	
* You should have received a copy of the GNU General Public License
* along with DOUBANGO.
*
*/
#ifndef TINYDSHOW_VIDEOGRABBERNAME_H
#define TINYDSHOW_VIDEOGRABBERNAME_H

#include "tinydshow_config.h"

#include<string>

class VideoGrabberName
{
public:
	VideoGrabberName() {}

	VideoGrabberName(std::string name, std::string description);

	std::string getName() const;

	std::string getDescription() const;

	int operator==( const VideoGrabberName &dev ) const;

private:
	std::string name;
	std::string description;
};

#endif /* TINYDSHOW_VIDEOGRABBERNAME_H */
