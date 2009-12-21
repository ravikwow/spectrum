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

#ifndef _ABSTRACT_USER_H
#define _ABSTRACT_USER_H

#include <string>
#include <list>
#include "account.h"
#include "value.h"
#include "resourcemanager.h"

class AbstractUser : public ResourceManager
{
	public:
		virtual ~AbstractUser() {}
		virtual const std::string &userKey() = 0;
		virtual long storageId() = 0;
		virtual PurpleAccount *account() = 0;
		virtual const std::string &jid() = 0;
		virtual PurpleValue *getSetting(const char *key) = 0;
		virtual bool hasTransportFeature(int feature) = 0;
		virtual int getFeatures() = 0;
		virtual const std::string &username() = 0;
		const char *getLang() { return "en"; }
		virtual GHashTable *settings() = 0;
		virtual bool isConnectedInRoom(const std::string &room) = 0;
		virtual bool isConnected() = 0;
		virtual bool readyForConnect() = 0;
		virtual void connect() = 0;
		
		void setProtocolData(void *protocolData) { m_protocolData = protocolData; }
		void *protocolData() { return m_protocolData; }
		std::string getRoomResource(const std::string &room) { return m_roomResources[room]; }
		void setRoomResource(const std::string &room, const std::string &resource) { m_roomResources[room] = resource; }
		
		guint removeTimer;

	private:
		void *m_protocolData;
		std::map<std::string, std::string> m_roomResources;
		
};

#endif
