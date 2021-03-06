/**
 * XMPP - libpurple transport
 *
 * Copyright (C) 2009, Jan Kaluza <hanzz@soc.pidgin.im>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02111-1301  USA
 */

#ifndef _HI_ABSTRACT_PURPLE_REQUEST_H
#define _HI_ABSTRACT_PURPLE_REQUEST_H

#include <string>
#include <list>
#include "purple.h"
#include "user.h"

using namespace gloox;

// Abstract class for PurpleRequest handler.
class AbstractPurpleRequest {
	public:
		virtual ~AbstractPurpleRequest() {}

		// Sets request type.
		void setRequestType(AdhocDataCallerType type) { m_rtype = type; }

		// Returns request type.
		AdhocDataCallerType & requestType() { return m_rtype; }

	private:
		AdhocDataCallerType m_rtype;
};

#endif
