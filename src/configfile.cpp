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

#include "configfile.h"
#include "main.h"
#include "capabilityhandler.h"
#include "log.h"
#include "spectrum_util.h"
#include "transport.h"
#ifdef WIN32
#include "win32/win32dep.h"
#endif

#define LOAD_REQUIRED_STRING(VARIABLE, SECTION, KEY) {if (!loadString((VARIABLE), (SECTION), (KEY))) return DummyConfiguration;}
#define LOAD_REQUIRED_STRING_DEFAULT(VARIABLE, SECTION, KEY, DEFAULT) {if (!loadString((VARIABLE), (SECTION), (KEY), (DEFAULT))) return DummyConfiguration;}

static int create_dir(std::string dir, int mode) {
#ifdef WIN32
	replace(dir, "/", "\\");
#endif
	char *dirname = g_path_get_dirname(dir.c_str());
	int ret = purple_build_dir(dirname, mode);
	g_free(dirname);
	return ret;
}

Configuration DummyConfiguration;

ConfigFile::ConfigFile(const std::string &config) {
	m_loaded = true;
	m_jid = "";
	m_protocol = "";
	m_filename = "";
#ifdef WIN32
	char *appdata = wpurple_get_special_folder(CSIDL_APPDATA);
	m_appdata = std::string(appdata ? appdata : "");
#else
	m_appdata = "";
#endif

	keyfile = g_key_file_new ();
	if (!config.empty())
		loadFromFile(config);
}

void ConfigFile::loadFromFile(const std::string &config) {
	int flags = G_KEY_FILE_KEEP_COMMENTS | G_KEY_FILE_KEEP_TRANSLATIONS;
	char *basename = g_path_get_basename(config.c_str());
	std::string b(basename);

	if (endsWith(b, ".cfg"))
		m_filename = b.substr(0, b.find_last_of('.'));
	else
		m_filename = b;

	if (b.find_last_of(':') == std::string::npos)
		m_port = -1;
	else
		m_port = atoi(m_filename.substr(b.find_last_of(':') + 1, b.size()).c_str());
	m_filename = m_filename.substr(0, b.find_last_of(':'));

	if (m_filename.find_last_of(':') == std::string::npos)
		m_transport = m_filename;
	else
		m_transport = m_filename.substr(m_filename.find_last_of(':') + 1, m_filename.size()).c_str();
	std::transform(m_transport.begin(), m_transport.end(), m_transport.begin(), tolower);

	m_filename = m_filename.substr(0, m_filename.find_last_of(':'));
	g_free(basename);

	if (!g_key_file_load_from_file (keyfile, config.c_str(), (GKeyFileFlags)flags, NULL)) {
		if (!g_key_file_load_from_file (keyfile, std::string( "/etc/spectrum/" + config + ".cfg").c_str(), (GKeyFileFlags)flags, NULL))
		{
			if (!g_key_file_load_from_file (keyfile, std::string( m_appdata + "/spectrum/" + config).c_str(), (GKeyFileFlags)flags, NULL))
			{
				Log("loadConfigFile", "Can't load config file! Tried these paths:");
				Log("loadConfigFile", std::string(config));
				Log("loadConfigFile", std::string("/etc/spectrum/" + config + ".cfg"));
				Log("loadConfigFile", std::string("./" + config));
				m_loaded = false;
			}
		}
	}
}

void ConfigFile::loadFromData(const std::string &data) {
	int flags = G_KEY_FILE_KEEP_COMMENTS | G_KEY_FILE_KEEP_TRANSLATIONS;

	if (!g_key_file_load_from_data (keyfile, data.c_str(), (int) data.size(), (GKeyFileFlags) flags, NULL)) {
		Log("loadConfigFile", "Bad data");
		m_loaded = false;
	}
}

bool ConfigFile::loadString(std::string &variable, const std::string &section, const std::string &key, const std::string &def) {
	char *value;
	if ((value = g_key_file_get_string(keyfile, section.c_str(), key.c_str(), NULL)) != NULL) {
		variable.assign(value);
		if (!m_jid.empty())
			replace(variable, "$jid", m_jid.c_str());
		if (!m_protocol.empty())
			replace(variable, "$protocol", m_protocol.c_str());
		replace(variable, "$appdata", m_appdata.c_str());
		replace(variable, "$filename:jid", m_filename.c_str());
		replace(variable, "$filename:protocol", m_transport.c_str());
		g_free(value);
	}
	else {
		if (def == "required") {
			Log("loadConfigFile", "You have to specify `" << key << "` in [" << section << "] section of config file.");
			return false;
		}
		else
			variable = def;
	}
	return true;
}

bool ConfigFile::loadInteger(int &variable, const std::string &section, const std::string &key, int def) {
	if (g_key_file_has_key(keyfile, section.c_str(), key.c_str(), NULL)) {
		GError *error = NULL;
		variable = (int) g_key_file_get_integer(keyfile, section.c_str(), key.c_str(), &error);
		if (error) {
			if (error->code == G_KEY_FILE_ERROR_INVALID_VALUE) {
				char *value;
				if ((value = g_key_file_get_string(keyfile, section.c_str(), key.c_str(), NULL)) != NULL) {
					if (strcmp( value,"$filename:port") == 0) {
						variable = m_port;
						g_error_free(error);
						return true;
					}
				}
				Log("loadConfigFile", "Value of key `" << key << "` in [" << section << "] section is not integer.");
			}
			g_error_free(error);
			return false;
		}
	}
	else {
		if (def == INT_MAX) {
			Log("loadConfigFile", "You have to specify `" << key << "` in [" << section << "] section of config file.");
			return false;
		}
		else
			variable = def;
	}
	return true;
}

bool ConfigFile::loadBoolean(bool &variable, const std::string &section, const std::string &key, bool def, bool required) {
	if (g_key_file_has_key(keyfile, section.c_str(), key.c_str(), NULL))
		variable = g_key_file_get_boolean(keyfile, section.c_str(), key.c_str(), NULL);
	else {
		if (required) {
			Log("loadConfigFile", "You have to specify `" << key << "` in [" << section << "] section of config file.");
			return false;
		}
		else
			variable = def;
	}
	return true;
}

bool ConfigFile::loadStringList(std::list <std::string> &variable, const std::string &section, const std::string &key) {
	char **bind;
	variable.clear();
	if (g_key_file_has_key(keyfile, section.c_str(), key.c_str(), NULL)) {
		bind = g_key_file_get_string_list(keyfile, section.c_str(), key.c_str(), NULL, NULL);
		for (int i = 0; bind[i]; i++) {
			variable.push_back(bind[i]);
		}
		g_strfreev (bind);
		return true;
	}
	return false;
}

bool ConfigFile::loadHostPort(std::string &host, int &port, const std::string &section, const std::string &key, const std::string &def_host, const int &def_port) {
	std::string str;
	if (!g_key_file_has_key(keyfile, section.c_str(), key.c_str(), NULL)) {
		if (def_host == "required") {
			Log("loadConfigFile", "You have to specify `" << key << "` in [" << section << "] section of config file.");
			return false;
		}
		host = def_host;
		port = def_port;
		return true;
	}
	loadString(str, section, key);
	
	if (str.find_first_of(':') == std::string::npos)
		port = 0;
	else {
		std::string p = str.substr(str.find_first_of(':') + 1, str.size()).c_str();
		replace(p, "$filename:port", stringOf(m_port).c_str());
		port = atoi(p.c_str());
	}
	host = str.substr(0, str.find_last_of(':'));
	return true;
}

bool ConfigFile::loadFeatures(int &features, const std::string &section) {
	if (!g_key_file_has_group(keyfile, section.c_str()))
		return false;
	char **keys = g_key_file_get_keys(keyfile, section.c_str(), NULL, NULL);
	bool val;
	int feature;
	features = TRANSPORT_FEATURE_ALL;
	for (int i = 0; keys[i]; i++) {
		std::string key(keys[i]);
		loadBoolean(val, section, key);

		if (key == "chatstate")
			feature = TRANSPORT_FEATURE_TYPING_NOTIFY;
		else if (key == "avatars")
			feature = TRANSPORT_FEATURE_AVATARS;
		else if (key == "filetransfer")
			feature = TRANSPORT_FEATURE_FILETRANSFER;
		else if (key == "statistics")
			feature = TRANSPORT_FEATURE_STATISTICS;
		else if (key == "xstatus")
			feature = TRANSPORT_FEATURE_STATISTICS;
		else
			continue;
		if (val)
			features = features | feature;
		else
			features = features & (~feature);
	}
	g_strfreev(keys);
	return true;
}

void ConfigFile::loadPurpleAccountSettings(Configuration &configuration) {
	if (!m_loaded)
		return;
	if (!g_key_file_has_group(keyfile, "purple"))
		return;

	
	char **keys = g_key_file_get_keys(keyfile, "purple", NULL, NULL);
	for (int i = 0; keys[i]; i++) {
		bool found = false;
		PurplePlugin *plugin = purple_find_prpl(Transport::instance()->protocol()->protocol().c_str());
		PurplePluginProtocolInfo *prpl_info = PURPLE_PLUGIN_PROTOCOL_INFO(plugin);
		for (GList *l = prpl_info->protocol_options; l != NULL; l = l->next) {
			PurpleAccountOption *option = (PurpleAccountOption *) l->data;
			PurpleAccountSettingValue v;
			v.type = purple_account_option_get_type(option);
			std::string key(purple_account_option_get_setting(option));

			if (key != keys[i])
				continue;

			found = true;

			switch (v.type) {
				case PURPLE_PREF_BOOLEAN:
					if (g_key_file_has_key(keyfile, "purple", key.c_str(), NULL)) {
						loadBoolean(v.b, "purple", key);
						configuration.purple_account_settings[key] = v;
					}
	// 				else
	// 					v.b = purple_account_option_get_default_bool(option);
					break;

				case PURPLE_PREF_INT:
					if (g_key_file_has_key(keyfile, "purple", key.c_str(), NULL)) {
						loadInteger(v.i, "purple", key);
						configuration.purple_account_settings[key] = v;
					}
	// 				else
	// 					v.i = purple_account_option_get_default_int(option);
					break;

				case PURPLE_PREF_STRING:
				case PURPLE_PREF_STRING_LIST:
					if (g_key_file_has_key(keyfile, "purple", key.c_str(), NULL)) {
						std::string str;
						loadString(str, "purple", key);
						v.str = g_strdup(str.c_str());
						configuration.purple_account_settings[key] = v;
					}
	// 				else {
	// 					const char *str = purple_account_option_get_default_string(option);
	// 					v.str = str ? g_strdup(str) : NULL;
	// 				}
					break;

	// 			case PURPLE_PREF_STRING_LIST:
	// 				if (g_key_file_has_key(keyfile, "purple", key.c_str(), NULL)) {
	// 					loadStringList(v.strlist, "purple", key);
	// 					configuration.purple_account_settings[key] = v;
	// 				}
	// 				break;

				default:
					continue;
			}
		}
		
		if (!found && g_key_file_has_key(keyfile, "purple", keys[i], NULL)) {
			std::cout << "setting " << keys[i] << " AAAAAAAAA\n";
			PurpleAccountSettingValue v;
			v.type = PURPLE_PREF_STRING;
			std::string str;
			loadString(str, "purple", keys[i]);
			v.str = g_strdup(str.c_str());
			configuration.purple_account_settings[keys[i]] = v;
		}
	}
	g_strfreev(keys);
}

Configuration ConfigFile::getConfiguration() {
	Configuration configuration;
	char **bind;
	int i;
	
	if (!m_loaded)
		return DummyConfiguration;

	// Service section
	LOAD_REQUIRED_STRING(configuration.protocol, "service", "protocol");
	std::transform(configuration.protocol.begin(), configuration.protocol.end(), configuration.protocol.begin(), tolower);
	m_protocol = configuration.protocol;

	LOAD_REQUIRED_STRING(configuration.jid, "service", "jid");
	m_jid = configuration.jid;

	LOAD_REQUIRED_STRING(configuration.discoName, "service", "name");
	LOAD_REQUIRED_STRING(configuration.server, "service", "server");
	LOAD_REQUIRED_STRING(configuration.password, "service", "password");
	LOAD_REQUIRED_STRING(configuration.filetransferCache, "service", "filetransfer_cache");
	LOAD_REQUIRED_STRING_DEFAULT(configuration.config_interface, "service", "config_interface", "/var/run/spectrum/" + configuration.jid + ".sock");
	if (!loadInteger(configuration.port, "service", "port")) return DummyConfiguration;
	loadString(configuration.filetransferWeb, "service", "filetransfer_web", "");
	loadString(configuration.pid_f, "service", "pid_file", "/var/run/spectrum/" + configuration.jid);
	loadString(configuration.language, "service", "language", "en");
	loadString(configuration.encoding, "service", "encoding", "");
	loadString(configuration.eventloop, "service", "eventloop", "glib");
	loadBoolean(configuration.enable_commands, "service", "enable_commands", true);
	loadBoolean(configuration.jid_escaping, "service", "jid_escaping", true);
	loadBoolean(configuration.onlyForVIP, "service", "only_for_vip", false);
	loadBoolean(configuration.VIPEnabled, "service", "vip_mode", false);
	loadBoolean(configuration.useProxy, "service", "use_proxy", false);
	loadBoolean(configuration.require_tls, "service", "require_tls", true);
	loadBoolean(configuration.forceRemoteRoster, "service", "force_remote_roster", false);
	loadBoolean(configuration.filetransferForceDir, "service", "filetransfer_force_cache_storage", false);
	loadStringList(configuration.allowedServers, "service", "allowed_servers");
	loadStringList(configuration.admins, "service", "admins");
	loadHostPort(configuration.filetransfer_proxy_ip, configuration.filetransfer_proxy_port, "service", "filetransfer_bind_address", "", 0);
	loadHostPort(configuration.filetransfer_proxy_streamhost_ip, configuration.filetransfer_proxy_streamhost_port, "service", "filetransfer_public_address", configuration.filetransfer_proxy_ip, configuration.filetransfer_proxy_port);

	if (!loadFeatures(configuration.transportFeatures, "features")) {
		configuration.transportFeatures = TRANSPORT_FEATURE_ALL;
		// TODO: transport_features and vip_features are depracted. remove it for 0.4
		if(g_key_file_has_key(keyfile,"service","transport_features",NULL)) {
			bind = g_key_file_get_string_list (keyfile,"service","transport_features",NULL, NULL);
			configuration.transportFeatures = 0;
			for (i = 0; bind[i]; i++){
				std::string feature(bind[i]);
				if (feature == "avatars")
					configuration.transportFeatures = configuration.transportFeatures | TRANSPORT_FEATURE_AVATARS;
				else if (feature == "chatstate")
					configuration.transportFeatures = configuration.transportFeatures | TRANSPORT_FEATURE_TYPING_NOTIFY;
				else if (feature == "filetransfer")
					configuration.transportFeatures = configuration.transportFeatures | TRANSPORT_FEATURE_FILETRANSFER;
			}
			g_strfreev (bind);
		}
		else configuration.transportFeatures = TRANSPORT_FEATURE_AVATARS | TRANSPORT_FEATURE_FILETRANSFER | TRANSPORT_FEATURE_TYPING_NOTIFY;
	}

	if (!loadFeatures(configuration.VIPFeatures, "vip-features")) {
		configuration.VIPFeatures = TRANSPORT_FEATURE_ALL;
		if(g_key_file_has_key(keyfile,"service","vip_features",NULL)) {
			bind = g_key_file_get_string_list (keyfile,"service","vip_features",NULL, NULL);
			configuration.VIPFeatures = 0;
			for (i = 0; bind[i]; i++){
				std::string feature(bind[i]);
				if (feature == "avatars")
					configuration.VIPFeatures |= TRANSPORT_FEATURE_AVATARS;
				else if (feature == "chatstate")
					configuration.VIPFeatures |= TRANSPORT_FEATURE_TYPING_NOTIFY;
				else if (feature == "filetransfer")
					configuration.VIPFeatures |= TRANSPORT_FEATURE_FILETRANSFER;
			}
			g_strfreev (bind);
		}
		else configuration.VIPFeatures = TRANSPORT_FEATURE_AVATARS | TRANSPORT_FEATURE_FILETRANSFER | TRANSPORT_FEATURE_TYPING_NOTIFY;
	}

	// Database section
	LOAD_REQUIRED_STRING(configuration.sqlType, "database", "type");
	LOAD_REQUIRED_STRING(configuration.sqlDb, "database", "database");
	LOAD_REQUIRED_STRING_DEFAULT(configuration.sqlHost, "database", "host", configuration.sqlType == "sqlite" ? "optional" : "");
	LOAD_REQUIRED_STRING_DEFAULT(configuration.sqlPassword, "database", "password", configuration.sqlType == "sqlite" ? "optional" : "");
	LOAD_REQUIRED_STRING_DEFAULT(configuration.sqlUser, "database", "user", configuration.sqlType == "sqlite" ? "optional" : "");
	LOAD_REQUIRED_STRING_DEFAULT(configuration.sqlPrefix, "database", "prefix", configuration.sqlType == "sqlite" ? "" : "required");
	LOAD_REQUIRED_STRING_DEFAULT(configuration.sqlCryptKey, "database", "useless_encryption_key", "");
	loadString(configuration.sqlVIP, "database", "vip_statement", "");
	LOAD_REQUIRED_STRING(configuration.userDir, "purple", "userdir");

	// Logging section
	loadString(configuration.logfile, "logging", "log_file", "");
	if (g_key_file_has_key(keyfile, "logging", "log_areas", NULL)) {
		bind = g_key_file_get_string_list(keyfile, "logging", "log_areas", NULL, NULL);
		configuration.logAreas = 0;
		for (i = 0; bind[i]; i++) {
			std::string feature(bind[i]);
			if (feature == "xml") {
				configuration.logAreas = configuration.logAreas | LOG_AREA_XML;
			}
			else if (feature == "purple")
				configuration.logAreas = configuration.logAreas | LOG_AREA_PURPLE;
		}
		g_strfreev (bind);
	}
	else configuration.logAreas = LOG_AREA_XML | LOG_AREA_PURPLE;


	// Registration section
	loadBoolean(configuration.enable_public_registration, "registration", "enable_public_registration", true);
	loadString(configuration.username_mask, "registration", "username_mask", "");
	loadString(configuration.reg_instructions, "registration", "instructions", "");
	loadString(configuration.reg_username_field, "registration", "username_label", "");
	loadString(configuration.reg_allowed_usernames, "registration", "allowed_usernames", "");
	loadStringList(configuration.reg_extra_fields, "registration", "extra_fields");
	if (configuration.reg_extra_fields.size() == 0) {
		configuration.reg_extra_fields.push_back("encoding");
	}

	if (configuration.sqlType == "sqlite")
		create_dir(configuration.sqlDb, 0750);
	create_dir(configuration.filetransferCache, 0750);
	create_dir(configuration.config_interface, 0750);
	create_dir(configuration.pid_f, 0750);
	create_dir(configuration.userDir, 0750);
	create_dir(configuration.logfile, 0750);
	return configuration;
}

ConfigFile::~ConfigFile() {
	g_key_file_free(keyfile);
}

