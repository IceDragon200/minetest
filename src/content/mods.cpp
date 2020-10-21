/*
Minetest
Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as published by
the Free Software Foundation; either version 2.1 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

#include <cctype>
#include <fstream>
#include <json/json.h>
#include <algorithm>
#include "content/mods.h"
#include "filesys.h"
#include "log.h"
#include "content/subgames.h"
#include "settings.h"
#include "porting.h"
#include "convert_json.h"
#include "script/common/c_internal.h"

void ModSpec::checkAndLog() const
{
	if (!string_allowed(name, MODNAME_ALLOWED_CHARS)) {
		throw ModError("Error loading mod \"" + name +
			"\": Mod name does not follow naming conventions: "
				"Only characters [a-z0-9_] are allowed.");
	}

	// Log deprecation messages
	auto handling_mode = get_deprecated_handling_mode();
	if (!deprecation_msgs.empty() && handling_mode != DeprecatedHandlingMode::Ignore) {
		std::ostringstream os;
		os << "Mod " << name << " at " << path << ":" << std::endl;
		for (auto msg : deprecation_msgs)
			os << "\t" << msg << std::endl;

		if (handling_mode == DeprecatedHandlingMode::Error)
			throw ModError(os.str());
		else
			warningstream << os.str();
	}
}

bool parseDependsString(std::string &dep, std::unordered_set<char> &symbols)
{
	dep = trim(dep);
	symbols.clear();
	size_t pos = dep.size();
	while (pos > 0 &&
			!string_allowed(dep.substr(pos - 1, 1), MODNAME_ALLOWED_CHARS)) {
		// last character is a symbol, not part of the modname
		symbols.insert(dep[pos - 1]);
		--pos;
	}
	dep = trim(dep.substr(0, pos));
	return !dep.empty();
}

void parseModContents(ModSpec &spec)
{
	// NOTE: this function works in mutual recursion with getModsInPath

	spec.depends.clear();
	spec.optdepends.clear();
	spec.is_modpack = false;
	spec.modpack_content.clear();

	// Handle modpacks (defined by containing modpack.txt)
	std::ifstream modpack_is((spec.path + DIR_DELIM + "modpack.txt").c_str());
	std::ifstream modpack2_is((spec.path + DIR_DELIM + "modpack.conf").c_str());
	if (modpack_is.good() || modpack2_is.good()) {
		if (modpack_is.good())
			modpack_is.close();

		if (modpack2_is.good())
			modpack2_is.close();

		spec.is_modpack = true;
		spec.modpack_content = getModsInPath(spec.path, true);

	} else {
		Settings info;
		info.readConfigFile((spec.path + DIR_DELIM + "mod.conf").c_str());

		if (info.exists("name"))
			spec.name = info.get("name");
		else
			spec.deprecation_msgs.push_back("Mods not having a mod.conf file with the name is deprecated.");

		if (info.exists("author"))
			spec.author = info.get("author");

		if (info.exists("release"))
			spec.release = info.getS32("release");

		// Attempt to load dependencies from mod.conf
		bool mod_conf_has_depends = false;
		if (info.exists("depends")) {
			mod_conf_has_depends = true;
			std::string dep = info.get("depends");
			// clang-format off
			dep.erase(std::remove_if(dep.begin(), dep.end(),
					static_cast<int (*)(int)>(&std::isspace)), dep.end());
			// clang-format on
			for (const auto &dependency : str_split(dep, ',')) {
				spec.depends.insert(dependency);
			}
		}

		if (info.exists("optional_depends")) {
			mod_conf_has_depends = true;
			std::string dep = info.get("optional_depends");
			// clang-format off
			dep.erase(std::remove_if(dep.begin(), dep.end(),
					static_cast<int (*)(int)>(&std::isspace)), dep.end());
			// clang-format on
			for (const auto &dependency : str_split(dep, ',')) {
				spec.optdepends.insert(dependency);
			}
		}

		// Fallback to depends.txt
		if (!mod_conf_has_depends) {
			std::vector<std::string> dependencies;

			std::ifstream is((spec.path + DIR_DELIM + "depends.txt").c_str());

			if (is.good())
				spec.deprecation_msgs.push_back("depends.txt is deprecated, please use mod.conf instead.");

			while (is.good()) {
				std::string dep;
				std::getline(is, dep);
				dependencies.push_back(dep);
			}

			for (auto &dependency : dependencies) {
				std::unordered_set<char> symbols;
				if (parseDependsString(dependency, symbols)) {
					if (symbols.count('?') != 0) {
						spec.optdepends.insert(dependency);
					} else {
						spec.depends.insert(dependency);
					}
				}
			}
		}

		if (info.exists("description"))
			spec.desc = info.get("description");
		else if (fs::ReadFile(spec.path + DIR_DELIM + "description.txt", spec.desc))
			spec.deprecation_msgs.push_back("description.txt is deprecated, please use mod.conf instead.");
	}
}

std::map<std::string, ModSpec> getModsInPath(
		const std::string &path, bool part_of_modpack)
{
	// NOTE: this function works in mutual recursion with parseModContents

	std::map<std::string, ModSpec> result;
	std::vector<fs::DirListNode> dirlist = fs::GetDirListing(path);
	std::string modpath;

	for (const fs::DirListNode &dln : dirlist) {
		if (!dln.dir)
			continue;

		const std::string &modname = dln.name;
		// Ignore all directories beginning with a ".", especially
		// VCS directories like ".git" or ".svn"
		if (modname[0] == '.')
			continue;

		modpath.clear();
		modpath.append(path).append(DIR_DELIM).append(modname);

		ModSpec spec(modname, modpath, part_of_modpack);
		parseModContents(spec);
		result.insert(std::make_pair(modname, spec));
	}
	return result;
}

std::vector<ModSpec> flattenMods(const std::map<std::string, ModSpec> &mods)
{
	std::vector<ModSpec> result;
	for (const auto &it : mods) {
		const ModSpec &mod = it.second;
		if (mod.is_modpack) {
			std::vector<ModSpec> content = flattenMods(mod.modpack_content);
			result.reserve(result.size() + content.size());
			result.insert(result.end(), content.begin(), content.end());

		} else // not a modpack
		{
			result.push_back(mod);
		}
	}
	return result;
}

// Look ma, an inline class!
class ModsResolver
{
public:
	// the mods here is a copy of the ModConfiguration unsatisifed mods
	ModsResolver(std::vector<ModSpec> mods)
	{
		for (ModSpec mod : mods) {
			m_mods_by_name[mod.name] = mod;
		}
	};

	// the main entry point to start resolving the graph
	void run(std::vector<ModSpec> &sorted_mods,
			std::vector<ModSpec> &unsatisfied_mods,
			std::vector<ModSpec> &mods_with_unsatisfied_dependencies);

	// any mod that appears twice in a single resolution stack
	std::list<ModWithCircularDependency> mods_with_circular_dependencies;

private:
	// resolve the mod by name, not spec, since a mod may not exist
	void resolveMod(const std::string &modname);

	std::list<std::string> m_resolution_stack;
	// mods that have already been resolved
	std::list<std::string> m_resolved_modnames;
	// mods that have been seen (not neccessarily resolved)
	std::set<std::string> m_seen_modnames;
	// inner mapping
	std::map<std::string, ModSpec> m_mods_by_name;
};

void ModsResolver::run(std::vector<ModSpec> &sorted_mods,
		std::vector<ModSpec> &unsatisfied_mods,
		std::vector<ModSpec> &mods_with_unsatisfied_optionals)
{
	// Step 1: Resolve a mod's dependencies
	for (auto mod_iter = m_mods_by_name.begin(); mod_iter != m_mods_by_name.end();) {
		ModSpec &mod = (*mod_iter).second;
		resolveMod(mod.name);
		++mod_iter;
	}

	// Step 2: Clear any satisfied dep
	for (auto mod_iter = m_mods_by_name.begin(); mod_iter != m_mods_by_name.end();) {
		ModSpec &mod = (*mod_iter).second;

		// Setup unsatisfied mandatory lists
		mod.unsatisfied_depends = mod.depends;

		// Setup the optional list based on what was actually seen
		for (const std::string &modname : mod.optdepends) {
			if (m_seen_modnames.count(modname) > 0) {
				mod.unsatisfied_optdepends.insert(modname);
			}
		}

		// m_resolved_modnames only contain mods that are known to exist
		// This loop removes the mod from the unsatisfied deps of the
		// target mod
		for (const std::string &modname : m_resolved_modnames) {
			mod.unsatisfied_depends.erase(modname);
			mod.unsatisfied_optdepends.erase(modname);
		}

		// increment iterator
		++mod_iter;
	}

	// Step 3.0: Check that the mod's deps have been properly satisfied
	for (const std::string &modname : m_resolved_modnames) {
		auto mod_iter = m_mods_by_name.find(modname);
		// sanity check
		if (mod_iter != m_mods_by_name.end()) {
			ModSpec &mod = (*mod_iter).second;

			// Check if the mod has its mandatory mods satisfied
			if (mod.unsatisfied_depends.empty()) {
				// if it has, it can be pushed unto the sorted_mods list
				sorted_mods.push_back(mod);
				// now to give the user some feedback, check if the mod
				// also satisfied it's optional dependencies
				if (!mod.unsatisfied_optdepends.empty()) {
					// if not, then we push it unto the optionals list
					mods_with_unsatisfied_optionals.push_back(mod);
				}
			} else {
				// the mod failed one or more mandatory dependencies
				unsatisfied_mods.push_back(mod);
			}
		}
	}
}

void ModsResolver::resolveMod(const std::string &modname)
{
	// behold recursion - the funny thing about this, it sorts itself
	// based on dependencies, mods that require other mods will naturally
	// try to load them before itself, ultimately performing the best
	// sorting possible.
	// And with recursion the dependencies of the dependencies can be
	// resolved just the same
	//
	// Now the only caveat I can think of is punching a whole in the stack with
	// a deep dependency graph.

	// This checks the resolving list (stack) for the specified mod
	bool isResolving = std::find(m_resolution_stack.begin(), m_resolution_stack.end(),
					   modname) != m_resolution_stack.end();

	if (isResolving) {
		// if a mod is currently resolving, then a circular dependency has occured
		// push the entry along with the current resolution stack for later
		// logging
		mods_with_circular_dependencies.emplace(
				mods_with_circular_dependencies.end(), modname,
				m_resolution_stack);
	} else if (m_seen_modnames.count(modname) == 0) {
		// immediately mark the mod as seen, to avoid circular deps later on
		m_seen_modnames.insert(modname);
		m_resolution_stack.push_back(modname);
		auto mod_iter = m_mods_by_name.find(modname);
		// Chances are the mod may or may not exist
		if (mod_iter != m_mods_by_name.end()) {
			ModSpec &mod = (*mod_iter).second;

			// resolve all hard dependencies
			for (const std::string &depname : mod.depends) {
				resolveMod(depname);
			}

			// resolve all soft dependencies
			for (const std::string &depname : mod.optdepends) {
				resolveMod(depname);
			}

			// the mod is known to be resolved (as best as it could)
			m_resolved_modnames.push_back(modname);
		}
		m_resolution_stack.pop_back();
	}
}

ModConfiguration::ModConfiguration(const std::string &worldpath)
{
}

void ModConfiguration::printUnsatisfiedModsError() const
{
	for (const ModSpec &mod : m_unsatisfied_mods) {
		errorstream << "mod \"" << mod.name
			    << "\" has unsatisfied dependencies: ";
		for (const std::string &unsatisfied_depend : mod.unsatisfied_depends)
			errorstream << " \"" << unsatisfied_depend << "\"";
		errorstream << std::endl;
	}
}

void ModConfiguration::printModsWithUnsatisfiedOptionalsWarning() const
{
	for (const ModSpec &mod : m_mods_with_unsatisfied_optionals) {
		warningstream << "mod \"" << mod.name
			      << "\" has unsatisfied dependencies (optional): ";
		for (const std::string &unsatisfied_depend : mod.unsatisfied_optdepends)
			warningstream << " \"" << unsatisfied_depend << "\"";
		warningstream << std::endl;
	}
}

void ModConfiguration::printModsWithCircularDependenciesWarning() const
{
	for (const ModWithCircularDependency &mwcd : m_mods_with_circular_dependencies) {
		warningstream << "circular dependency triggered by \"" << mwcd.name
			      << "\" check mods in chain; resolution-chain: ";
		for (const std::string &modname : mwcd.resolution_stack)
			warningstream << " \"" << modname << "\"";
		warningstream << std::endl;
	}
}

void ModConfiguration::printConsistencyMessages() const
{
	printUnsatisfiedModsError();
	printModsWithUnsatisfiedOptionalsWarning();
	printModsWithCircularDependenciesWarning();
}

void ModConfiguration::addModsInPath(const std::string &path)
{
	addMods(flattenMods(getModsInPath(path)));
}

void ModConfiguration::addMods(const std::vector<ModSpec> &new_mods)
{
	// Maintain a map of all existing m_unsatisfied_mods.
	// Keys are mod names and values are indices into m_unsatisfied_mods.
	std::map<std::string, u32> existing_mods;
	for (u32 i = 0; i < m_unsatisfied_mods.size(); ++i) {
		existing_mods[m_unsatisfied_mods[i].name] = i;
	}

	// Add new mods
	for (int want_from_modpack = 1; want_from_modpack >= 0; --want_from_modpack) {
		// First iteration:
		// Add all the mods that come from modpacks
		// Second iteration:
		// Add all the mods that didn't come from modpacks

		std::set<std::string> seen_this_iteration;

		for (const ModSpec &mod : new_mods) {
			if (mod.part_of_modpack != (bool)want_from_modpack)
				continue;

			if (existing_mods.count(mod.name) == 0) {
				// GOOD CASE: completely new mod.
				m_unsatisfied_mods.push_back(mod);
				existing_mods[mod.name] = m_unsatisfied_mods.size() - 1;
			} else if (seen_this_iteration.count(mod.name) == 0) {
				// BAD CASE: name conflict in different levels.
				u32 oldindex = existing_mods[mod.name];
				const ModSpec &oldmod = m_unsatisfied_mods[oldindex];
				warningstream << "Mod name conflict detected: \""
					      << mod.name << "\"" << std::endl
					      << "Will not load: " << oldmod.path
					      << std::endl
					      << "Overridden by: " << mod.path
					      << std::endl;
				m_unsatisfied_mods[oldindex] = mod;

				// If there was a "VERY BAD CASE" name conflict
				// in an earlier level, ignore it.
				m_name_conflicts.erase(mod.name);
			} else {
				// VERY BAD CASE: name conflict in the same level.
				u32 oldindex = existing_mods[mod.name];
				const ModSpec &oldmod = m_unsatisfied_mods[oldindex];
				warningstream << "Mod name conflict detected: \""
					      << mod.name << "\"" << std::endl
					      << "Will not load: " << oldmod.path
					      << std::endl
					      << "Will not load: " << mod.path
					      << std::endl;
				m_unsatisfied_mods[oldindex] = mod;
				m_name_conflicts.insert(mod.name);
			}

			seen_this_iteration.insert(mod.name);
		}
	}
}

void ModConfiguration::addModsFromConfig(
		const std::string &settings_path, const std::set<std::string> &mods)
{
	Settings conf;
	std::set<std::string> load_mod_names;

	conf.readConfigFile(settings_path.c_str());
	std::vector<std::string> names = conf.getNames();
	for (const std::string &name : names) {
		if (name.compare(0, 9, "load_mod_") == 0 && conf.get(name) != "false" &&
				conf.get(name) != "nil")
			load_mod_names.insert(name.substr(9));
	}

	std::vector<ModSpec> addon_mods;
	for (const std::string &i : mods) {
		std::vector<ModSpec> addon_mods_in_path = flattenMods(getModsInPath(i));
		for (std::vector<ModSpec>::const_iterator it = addon_mods_in_path.begin();
				it != addon_mods_in_path.end(); ++it) {
			const ModSpec &mod = *it;
			if (load_mod_names.count(mod.name) != 0)
				addon_mods.push_back(mod);
			else
				conf.setBool("load_mod_" + mod.name, false);
		}
	}
	conf.updateConfigFile(settings_path.c_str());

	addMods(addon_mods);
	checkConflictsAndDeps();

	// complain about mods declared to be loaded, but not found
	for (const ModSpec &addon_mod : addon_mods)
		load_mod_names.erase(addon_mod.name);

	for (const ModSpec &unsatisfiedMod : m_unsatisfied_mods)
		load_mod_names.erase(unsatisfiedMod.name);

	if (!load_mod_names.empty()) {
		errorstream << "The following mods could not be found:";
		for (const std::string &mod : load_mod_names)
			errorstream << " \"" << mod << "\"";
		errorstream << std::endl;
	}
}

void ModConfiguration::checkConflictsAndDeps()
{
	// report on name conflicts
	if (!m_name_conflicts.empty()) {
		std::string s = "Unresolved name conflicts for mods ";
		for (std::unordered_set<std::string>::const_iterator it =
						m_name_conflicts.begin();
				it != m_name_conflicts.end(); ++it) {
			if (it != m_name_conflicts.begin())
				s += ", ";
			s += std::string("\"") + (*it) + "\"";
		}
		s += ".";
		throw ModError(s);
	}

	// get the mods in order
	resolveDependencies();
}

void ModConfiguration::resolveDependencies()
{
	ModsResolver resolver(m_unsatisfied_mods);

	// reset lists
	m_sorted_mods.clear();
	m_unsatisfied_mods.clear();
	m_mods_with_unsatisfied_optionals.clear();

	resolver.run(m_sorted_mods, m_unsatisfied_mods,
			m_mods_with_unsatisfied_optionals);

	m_mods_with_circular_dependencies.assign(
			resolver.mods_with_circular_dependencies.begin(),
			resolver.mods_with_circular_dependencies.end());
}

#ifndef SERVER
ClientModConfiguration::ClientModConfiguration(const std::string &path) :
		ModConfiguration(path)
{
	std::set<std::string> paths;
	std::string path_user = porting::path_user + DIR_DELIM + "clientmods";
	paths.insert(path);
	paths.insert(path_user);

	std::string settings_path = path_user + DIR_DELIM + "mods.conf";
	addModsFromConfig(settings_path, paths);
}
#endif

ModMetadata::ModMetadata(const std::string &mod_name) : m_mod_name(mod_name)
{
}

void ModMetadata::clear()
{
	Metadata::clear();
	m_modified = true;
}

bool ModMetadata::save(const std::string &root_path)
{
	Json::Value json;
	for (StringMap::const_iterator it = m_stringvars.begin();
			it != m_stringvars.end(); ++it) {
		json[it->first] = it->second;
	}

	if (!fs::PathExists(root_path)) {
		if (!fs::CreateAllDirs(root_path)) {
			errorstream << "ModMetadata[" << m_mod_name
				    << "]: Unable to save. '" << root_path
				    << "' tree cannot be created." << std::endl;
			return false;
		}
	} else if (!fs::IsDir(root_path)) {
		errorstream << "ModMetadata[" << m_mod_name << "]: Unable to save. '"
			    << root_path << "' is not a directory." << std::endl;
		return false;
	}

	bool w_ok = fs::safeWriteToFile(
			root_path + DIR_DELIM + m_mod_name, fastWriteJson(json));

	if (w_ok) {
		m_modified = false;
	} else {
		errorstream << "ModMetadata[" << m_mod_name << "]: failed write file."
			    << std::endl;
	}
	return w_ok;
}

bool ModMetadata::load(const std::string &root_path)
{
	m_stringvars.clear();

	std::ifstream is((root_path + DIR_DELIM + m_mod_name).c_str(),
			std::ios_base::binary);
	if (!is.good()) {
		return false;
	}

	Json::Value root;
	Json::CharReaderBuilder builder;
	builder.settings_["collectComments"] = false;
	std::string errs;

	if (!Json::parseFromStream(builder, is, &root, &errs)) {
		errorstream << "ModMetadata[" << m_mod_name
			    << "]: failed read data "
			       "(Json decoding failure). Message: "
			    << errs << std::endl;
		return false;
	}

	const Json::Value::Members attr_list = root.getMemberNames();
	for (const auto &it : attr_list) {
		Json::Value attr_value = root[it];
		m_stringvars[it] = attr_value.asString();
	}

	return true;
}

bool ModMetadata::setString(const std::string &name, const std::string &var)
{
	m_modified = Metadata::setString(name, var);
	return m_modified;
}
