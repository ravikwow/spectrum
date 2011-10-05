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

#include "registerhandler.h"
#include <gloox/clientbase.h>
#include <glib.h>
#include "sql.h"
#include "capabilityhandler.h"
#include "usermanager.h"
#include "rostermanager.h"
#include "accountcollector.h"
#include "protocols/abstractprotocol.h"
#include "user.h"
#include "transport.h"
#include "main.h"
#include "curl/curl.h"

GlooxRegisterHandler* GlooxRegisterHandler::m_pInstance = NULL;

RegisterExtension::RegisterExtension() : StanzaExtension( ExtRegistration )
{
    m_tag = NULL;
}

RegisterExtension::RegisterExtension(const Tag *tag) : StanzaExtension( ExtRegistration )
{
    m_tag = tag->clone();
}
		
RegisterExtension::~RegisterExtension()
{
        Log("RegisterExtension", "deleting RegisterExtension()");
        delete m_tag;
}
				
const std::string& RegisterExtension::filterString() const
{
    static const std::string filter = "iq/query[@xmlns='jabber:iq:register']";
    return filter;
}
						
Tag* RegisterExtension::tag() const
{
    return m_tag->clone();
}							

GlooxRegisterHandler::GlooxRegisterHandler() : IqHandler() {
	Transport::instance()->registerStanzaExtension( new RegisterExtension );
	m_pInstance = this;
}

GlooxRegisterHandler::~GlooxRegisterHandler(){
}

bool GlooxRegisterHandler::isFieldEnabled(const std::string &name) {
	std::list<std::string> const &fields = CONFIG().reg_extra_fields;
	return (std::find(fields.begin(), fields.end(), name) != fields.end());
}

bool GlooxRegisterHandler::registerUser(const UserRow &row) {
	// TODO: move this check to sql()->addUser(...) and let it return bool
	UserRow res = Transport::instance()->sql()->getUserByJid(row.jid);
	// This user is already registered
	if (res.id != -1)
		return false;

	Log("GlooxRegisterHandler", "adding new user: "<< row.jid << ", " << row.uin <<  ", " << row.language);
	Transport::instance()->sql()->addUser(row);
	
	m_to = row.jid;
	SpectrumRosterManager::sendRosterPush(row.jid, Transport::instance()->jid(), "both", "", "", this, 0);

	return true;
}

bool GlooxRegisterHandler::unregisterUser(const std::string &barejid) {
	UserRow res = Transport::instance()->sql()->getUserByJid(barejid);
	// This user is already registered
	if (res.id == -1)
		return false;

	Log("GlooxRegisterHandler", "removing user " << barejid << " from database and disconnecting from legacy network");
	PurpleAccount *account = NULL;

	m_to = barejid;
	if (CONFIG().forceRemoteRoster) {
		IQ iq(IQ::Result, m_to);
		handleIqID(iq, 1);
	}
	else {
		SpectrumRosterManager::sendRosterPush(barejid, Transport::instance()->jid(), "remove", "", "", this, 1);
	}

	return true;
}

bool GlooxRegisterHandler::handleIq(const IQ &iq) {
	Tag *iqTag = iq.tag();
	if (!iqTag) return true;

	bool ret = handleIq(iqTag);
	delete iqTag;
	return ret;
}

static int writercurl(char *data, size_t size, size_t nmemb, std::string *buffer)
{
  return size * nmemb;
}

bool GlooxRegisterHandler::handleIq(const Tag *iqTag) {
	Log("GlooxRegisterHandler", iqTag->findAttribute("from") << ": iq:register received (" << iqTag->findAttribute("type") << ")");

	JID from(iqTag->findAttribute("from"));

	if (CONFIG().protocol == "irc") {
		sendError(400, "bad-request", iqTag);
		return false;
	}
	
	User *user = Transport::instance()->userManager()->getUserByJID(from.bare());
	if (!Transport::instance()->getConfiguration().enable_public_registration) {
		std::list<std::string> const &x = Transport::instance()->getConfiguration().allowedServers;
		if (std::find(x.begin(), x.end(), from.server()) == x.end()) {
			Log("GlooxRegisterHandler", "This user has no permissions to register an account");
			sendError(400, "bad-request", iqTag);
			return false;
		}
	}

	const char *_language = user ? user->getLang() : Transport::instance()->getConfiguration().language.c_str();

	// send registration form
	if (iqTag->findAttribute("type") == "get") {
		Tag *reply = new Tag( "iq" );
		reply->addAttribute( "id", iqTag->findAttribute("id") );
		reply->addAttribute( "type", "result" );
		reply->addAttribute( "to", iqTag->findAttribute("from") );
		reply->addAttribute( "from", Transport::instance()->jid() );
		Tag *query = new Tag( "query" );
		query->addAttribute( "xmlns", "jabber:iq:register" );
		UserRow res = Transport::instance()->sql()->getUserByJid(from.bare());

		std::string instructions = CONFIG().reg_instructions.empty() ? PROTOCOL()->text("instructions") : CONFIG().reg_instructions;
		std::string usernameField = CONFIG().reg_username_field.empty() ? PROTOCOL()->text("username") : CONFIG().reg_username_field;

		if (res.id == -1) {
			Log("GlooxRegisterHandler", "* sending registration form; user is not registered");
			query->addChild( new Tag("instructions", tr(_language, instructions)) );
			query->addChild( new Tag("username") );
			if (CONFIG().protocol != "twitter" && CONFIG().protocol != "bonjour")
				query->addChild( new Tag("password") );
		}
		else {
			Log("GlooxRegisterHandler", "* sending registration form; user is registered");
			query->addChild( new Tag("instructions", tr(_language, instructions)) );
			query->addChild( new Tag("registered") );
			query->addChild( new Tag("username", res.uin));
			if (CONFIG().protocol != "twitter" && CONFIG().protocol != "bonjour")
				query->addChild( new Tag("password"));
		}

		Tag *x = new Tag("x");
		x->addAttribute("xmlns", "jabber:x:data");
		x->addAttribute("type", "form");
		x->addChild( new Tag("title", tr(_language, _("Registration"))));
		x->addChild( new Tag("instructions", tr(_language, instructions)) );

		Tag *field = new Tag("field");
		field->addAttribute("type", "hidden");
		field->addAttribute("var", "FORM_TYPE");
		field->addChild( new Tag("value", "jabber:iq:register") );
		x->addChild(field);

		field = new Tag("field");
		field->addAttribute("type", "text-single");
		field->addAttribute("var", "username");
		field->addAttribute("label", tr(_language, usernameField));
		field->addChild( new Tag("required") );
		if (res.id!=-1)
			field->addChild( new Tag("value", res.uin) );
		x->addChild(field);

		if (CONFIG().protocol != "twitter" && CONFIG().protocol != "bonjour") {
			field = new Tag("field");
			field->addAttribute("type", "text-private");
			field->addAttribute("var", "password");
			field->addAttribute("label", tr(_language, _("Password")));
			x->addChild(field);
		}

		field = new Tag("field");
		field->addAttribute("type", "list-single");
		field->addAttribute("var", "language");
		field->addAttribute("label", tr(_language, _("Language")));
		if (res.id!=-1)
			field->addChild( new Tag("value", res.language) );
		else
			field->addChild( new Tag("value", Transport::instance()->getConfiguration().language) );
		x->addChild(field);

		std::map <std::string, std::string> languages = localization.getLanguages();
		for (std::map <std::string, std::string>::iterator it = languages.begin(); it != languages.end(); it++) {
			Tag *option = new Tag("option");
			option->addAttribute("label", (*it).second);
			option->addChild( new Tag("value", (*it).first) );
			field->addChild(option);
		}

		if (isFieldEnabled("encoding")) {
			field = new Tag("field");
			field->addAttribute("type", "text-single");
			field->addAttribute("var", "encoding");
			field->addAttribute("label", tr(_language, _("Encoding")));
			if (res.id!=-1)
				field->addChild( new Tag("value", res.encoding) );
			else
				field->addChild( new Tag("value", Transport::instance()->getConfiguration().encoding) );
			x->addChild(field);
		}

		if (res.id != -1) {
			field = new Tag("field");
			field->addAttribute("type", "boolean");
			field->addAttribute("var", "unregister");
			field->addAttribute("label", tr(_language, _("Remove your registration")));
			field->addChild( new Tag("value", "0") );
			x->addChild(field);
		}

		query->addChild(x);
		reply->addChild(query);
		Transport::instance()->send( reply );
		return true;
	}
	else if (iqTag->findAttribute("type") == "set") {
		bool remove = false;
		Tag *query;
		Tag *usernametag;
		Tag *passwordtag;
		Tag *languagetag;
		Tag *encodingtag;
		std::string username("");
		std::string password("");
		std::string language("");
		std::string encoding(CONFIG().encoding);

		UserRow res = Transport::instance()->sql()->getUserByJid(from.bare());
		
		query = iqTag->findChild( "query" );
		if (!query) return true;

		Tag *xdata = query->findChild("x", "xmlns", "jabber:x:data");
		if (xdata) {
			if (query->hasChild( "remove" ))
				remove = true;
			for (std::list<Tag*>::const_iterator it = xdata->children().begin(); it != xdata->children().end(); ++it) {
				std::string key = (*it)->findAttribute("var");
				if (key.empty()) continue;

				Tag *v = (*it)->findChild("value");
				if (!v) continue;

				if (key == "username")
					username = v->cdata();
				else if (key == "password")
					password = v->cdata();
				else if (key == "language")
					language = v->cdata();
				else if (key == "encoding")
					encoding = v->cdata();
				else if (key == "unregister")
					remove = atoi(v->cdata().c_str());
			}
		}
		else {
			if (query->hasChild( "remove" ))
				remove = true;
			else {
				usernametag = query->findChild("username");
				passwordtag = query->findChild("password");
				languagetag = query->findChild("language");
				encodingtag = query->findChild("encoding");

				if (languagetag)
					language = languagetag->cdata();
				else
					language = Transport::instance()->getConfiguration().language;

				if (encodingtag)
					encoding = encodingtag->cdata();
				else
					encoding = Transport::instance()->getConfiguration().encoding;

				if (usernametag==NULL || (passwordtag==NULL && CONFIG().protocol != "twitter" && CONFIG().protocol != "bonjour")) {
					sendError(406, "not-acceptable", iqTag);
					return false;
				}
				else {
					username = usernametag->cdata();
					if (passwordtag)
						password = passwordtag->cdata();
					if (username.empty() || (password.empty() && CONFIG().protocol != "twitter" && CONFIG().protocol != "bonjour")) {
						sendError(406, "not-acceptable", iqTag);
						return false;
					}
				}
			}
		}

		if (Transport::instance()->getConfiguration().protocol == "xmpp") {
			// User tries to register himself.
			if ((JID(username).bare() == from.bare())) {
				sendError(406, "not-acceptable", iqTag);
				return false;
			}

			// User tries to register someone who's already registered.
			UserRow user_row = Transport::instance()->sql()->getUserByJid(JID(username).bare());
			if (user_row.id != -1) {
				sendError(406, "not-acceptable", iqTag);
				return false;
			}
		}

		if (remove) {
			unregisterUser(from.bare());

			Tag *reply = new Tag("iq");
			reply->addAttribute( "type", "result" );
			reply->addAttribute( "from", Transport::instance()->jid() );
			reply->addAttribute( "to", iqTag->findAttribute("from") );
			reply->addAttribute( "id", iqTag->findAttribute("id") );
			Transport::instance()->send( reply );

			return true;
		}

		// Register or change password

		std::string jid = from.bare();

		if (username.empty() || (password.empty() && CONFIG().protocol != "twitter" && CONFIG().protocol != "bonjour") || localization.getLanguages().find(language) == localization.getLanguages().end()) {
			sendError(406, "not-acceptable", iqTag);
			return false;
		}

		Transport::instance()->protocol()->prepareUsername(username);

		std::string newUsername(username);
		if (!CONFIG().username_mask.empty()) {
			newUsername = CONFIG().username_mask;
			replace(newUsername, "$username", username.c_str());
		}

		if (!Transport::instance()->protocol()->isValidUsername(newUsername)) {
			Log("GlooxRegisterHandler", "This is not valid username: "<< newUsername);
			sendError(400, "bad-request", iqTag);
			return false;
		}

#if GLIB_CHECK_VERSION(2,14,0)
		if (!CONFIG().reg_allowed_usernames.empty() &&
			!g_regex_match_simple(CONFIG().reg_allowed_usernames.c_str(), newUsername.c_str(),(GRegexCompileFlags) (G_REGEX_CASELESS | G_REGEX_EXTENDED), (GRegexMatchFlags) 0)) {
			Log("GlooxRegisterHandler", "This is not valid username: "<< newUsername);
			sendError(400, "bad-request", iqTag);
			return false;
		}
#endif
		if (res.id == -1) {
			res.jid = from.bare();
			res.uin = username;
			res.password = password;
			res.language = language;
			res.encoding = encoding;
			res.vip = 0;

			//Log("RavikWoW_Debug", "adding new user: jid^"<< res.jid << ", uin^" << res.uin <<  ", pass^" << res.password <<  ", lang^" << res.language <<  ", enc^" << res.encoding);
			//Log("RavikWoW_Debug", "Protocol: "<< CONFIG().protocol << ", Transport: " << Transport::instance()->jid());
			std::string urltransport = Transport::instance()->jid();
			urltransport = urltransport.substr(0,urltransport.find_first_of('.')-1);
			//Log("RavikWoW_Debug", "Protocol: "<< urltransport);
			if (urltransport=="vkontakte")
			{
				static char errorBuffer[CURL_ERROR_SIZE];
				char * lasturl;
				static std::string buffer;
				CURL *curl;
				CURLcode result;
				curl = curl_easy_init();
				if (curl)
				{
					curl_easy_setopt(curl, CURLOPT_URL, "http://vk.com/login.php");
					curl_easy_setopt(curl, CURLOPT_HEADER, 0);
					curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1);
					curl_easy_setopt(curl, CURLOPT_POST, 1);
					std::string req =(std::string)"email="+res.uin+(std::string)"&pass="+res.password;
					curl_easy_setopt(curl, CURLOPT_POSTFIELDS, req.c_str());
					curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)req.length());
					curl_easy_setopt(curl, CURLOPT_USERAGENT, "Mozilla/5.0 (Windows; U; Windows NT 5.1; en-US; rv:1.8.1.1) Gecko/20061204 Firefox/2.0.0.1");
					curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writercurl);
					curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buffer);
					result = curl_easy_perform(curl);
					if (result == CURLE_OK)
					{
						result = curl_easy_getinfo(curl, CURLINFO_EFFECTIVE_URL, &lasturl);
						if (result == CURLE_OK) {
							std::string str = lasturl;
							str = str.substr(str.find_last_of('/')+1);
							if(str!="login.php")
							{
								Log("GlooxRegisterHandler", "Change VKontakte login: " << res.uin << " to " << str);
								res.uin = str;
							}
						}
					}
				}
				curl_easy_cleanup(curl);
			}
			registerUser(res);
		}
		else {
			// change passwordhttp://soumar.jabbim.cz/phpmyadmin/index.php
			Log("GlooxRegisterHandler", "changing user password: "<< jid << ", " << username);
			res.jid = from.bare();
			res.password = password;
			res.language = language;
			res.encoding = encoding;
			Transport::instance()->sql()->updateUser(res);
		}

		Tag *reply = new Tag( "iq" );
		reply->addAttribute( "id", iqTag->findAttribute("id") );
		reply->addAttribute( "type", "result" );
		reply->addAttribute( "to", iqTag->findAttribute("from") );
		reply->addAttribute( "from", Transport::instance()->jid() );
		Transport::instance()->send( reply );

		return true;
	}
	return false;
}

void GlooxRegisterHandler::handleIqID (const IQ &iq, int context){
	// adding user
	if (context == 0) {
		if (iq.subtype() == IQ::Error) {
			Tag *reply = new Tag("presence");
			reply->addAttribute( "from", Transport::instance()->jid() );
			reply->addAttribute( "to", m_to);
			reply->addAttribute( "type", "subscribe" );
			Transport::instance()->send( reply );
		}
	}
	// removing user
	else if (context == 1) {
		PurpleAccount *account = NULL;
		UserRow res = Transport::instance()->sql()->getUserByJid(m_to);
		// This user is already unregistered
		if (res.id == -1)
			return;
		User *user = Transport::instance()->userManager()->getUserByJID(m_to);
		if (iq.subtype() == IQ::Result) {
			std::list <std::string> roster;
			roster = Transport::instance()->sql()->getBuddies(res.id);
			for(std::list<std::string>::iterator u = roster.begin(); u != roster.end() ; u++){
				std::string name = *u;
				SpectrumRosterManager::sendRosterPush(m_to, name + "@" + Transport::instance()->jid(), "remove");
			}
		}
		else {
			if (user && user->hasFeature(GLOOX_FEATURE_ROSTERX) && false) {
				std::cout << "* sending rosterX\n";
				Tag *tag = new Tag("message");
				tag->addAttribute( "to", m_to );
				std::string _from;
				_from.append(Transport::instance()->jid());
				tag->addAttribute( "from", _from );
				tag->addChild(new Tag("body","removing users"));
				Tag *x = new Tag("x");
				x->addAttribute("xmlns","http://jabber.org/protocol/rosterx");

				std::list <std::string> roster;
				roster = Transport::instance()->sql()->getBuddies(res.id);

				Tag *item;
				for(std::list<std::string>::iterator u = roster.begin(); u != roster.end() ; u++){
					std::string name = *u;
					item = new Tag("item");
					item->addAttribute("action", "delete");
					item->addAttribute("jid", name + "@" + Transport::instance()->jid());
					x->addChild(item);
				}

				tag->addChild(x);
				Transport::instance()->send(tag);
			}
			else {
				std::list <std::string> roster;
				// roster contains already escaped jids
				roster = Transport::instance()->sql()->getBuddies(res.id);

				Tag *tag;
				for(std::list<std::string>::iterator u = roster.begin(); u != roster.end() ; u++){
					std::string name = *u;

					tag = new Tag("presence");
					tag->addAttribute( "to", m_to );
					tag->addAttribute( "type", "unsubscribe" );
					tag->addAttribute( "from", name + "@" + Transport::instance()->jid());
					Transport::instance()->send( tag );

					tag = new Tag("presence");
					tag->addAttribute( "to", m_to );
					tag->addAttribute( "type", "unsubscribed" );
					tag->addAttribute( "from", name + "@" + Transport::instance()->jid());
					Transport::instance()->send( tag );
				}
			}
			Tag *reply = new Tag( "presence" );
			reply->addAttribute( "type", "unsubscribe" );
			reply->addAttribute( "from", Transport::instance()->jid() );
			reply->addAttribute( "to", m_to );
			Transport::instance()->send( reply );

			reply = new Tag("presence");
			reply->addAttribute( "type", "unsubscribed" );
			reply->addAttribute( "from", Transport::instance()->jid() );
			reply->addAttribute( "to", m_to );
			Transport::instance()->send( reply );
		}

		// Remove user from database
		if (res.id != -1) {
			Transport::instance()->sql()->removeUser(res.id);
		}

		// Disconnect the user
		if (user != NULL) {
			account = user->account();
			Transport::instance()->userManager()->removeUser(user);	
		}

		if (!account) {
			account = purple_accounts_find(res.uin.c_str(), Transport::instance()->protocol()->protocol().c_str());
		}

#ifndef TESTS
		// Remove account from memory
		if (account)
			Transport::instance()->collector()->collectNow(account, true);
#endif
	}
}

void GlooxRegisterHandler::sendError(int code, const std::string &err, const Tag *iqTag) {
	Tag *iq = new Tag("iq");
	iq->addAttribute("type", "error");
	iq->addAttribute("from", Transport::instance()->jid());
	iq->addAttribute("to", iqTag->findAttribute("from"));
	iq->addAttribute("id", iqTag->findAttribute("id"));

	Tag *error = new Tag("error");
	error->addAttribute("code", code);
	error->addAttribute("type", "modify");
	Tag *bad = new Tag(err);
	bad->addAttribute("xmlns", "urn:ietf:params:xml:ns:xmpp-stanzas");

	error->addChild(bad);
	iq->addChild(error);

	Transport::instance()->send(iq);
}

