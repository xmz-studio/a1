// myini.hpp
#ifndef A1_MYINI_HPP
#define A1_MYINI_HPP
#include <string>
#include <map>
#include <vector>
#include <algorithm>
#include <cctype>
#include <libxmz/fs.hpp>
#include <libxmz/str.hpp>

namespace a1::ini {
class ini_parser {
public:
	struct ini_line {
		enum kind_t { kind_section, kind_key_value, kind_comment, kind_blank };
		kind_t kind = kind_blank;
		std::string raw;		// Original line text
		std::string section;	// Belongs to the section
		std::string key;		// key_value use
		std::string value;		// key_value for (already unescape)
		std::string indent;		// Blank at the beginning of the line
		char sep = ':';			// The original separator '=' or ':'
		bool dirty = false;		// Modify the status
	};
private:
	std::vector<ini_line> lines_;
	std::map<std::string, std::map<std::string, size_t>> index_;
	std::vector<std::string> section_order_;
	std::map<std::string, std::vector<std::string>> key_order_;
public:
	bool parse_file(const std::string& filepath)
	{
		std::string content = xmz::fs::readfile_str(filepath);
		if (content.empty()) return false;
		return parse_str(content);
	}

	bool readfile(const std::string& filepath)
	{ return parse_file(filepath); }

	bool parse_str(const std::string& content)
	{
		lines_.clear();
		index_.clear();
		section_order_.clear();
		key_order_.clear();

		std::string current_section;
		auto raw_lines = xmz::str::split(content, "\n");

		for (auto& raw : raw_lines) {
			std::string line = raw;
			if (!line.empty() && line.back() == '\r') line.pop_back();

			ini_line L;
			L.raw = line;

			std::string trimmed = xmz::str::trim(line);
			if (trimmed.empty()) {
				L.kind = ini_line::kind_blank;
				lines_.push_back(std::move(L));
				continue;
			}
			if (trimmed[0] == ';' || trimmed[0] == '#') {
				L.kind = ini_line::kind_comment;
				lines_.push_back(std::move(L));
				continue;
			}

			std::string stripped = remove_comments(line);
			std::string core = xmz::str::trim(stripped);
			if (core.empty()) {
				L.kind = ini_line::kind_comment;
				lines_.push_back(std::move(L));
				continue;
			}

			if (core[0] == '[' && core.back() == ']') {
				std::string name = xmz::str::trim(core.substr(1, core.size() - 2));
				current_section = unescape_string(name);
				L.kind = ini_line::kind_section;
				L.section = current_section;
				if (!index_.count(current_section)) {
					index_[current_section];
					section_order_.push_back(current_section);
				}
				lines_.push_back(std::move(L));
				continue;
			}

			auto pos_eq = core.find('=');
			auto pos_col = core.find(':');
			size_t pos = std::string::npos;
			if (pos_eq != std::string::npos && pos_col != std::string::npos)
				pos = std::min(pos_eq, pos_col);
			else if (pos_eq != std::string::npos)
				pos = pos_eq;
			else if (pos_col != std::string::npos)
				pos = pos_col;

			if (pos != std::string::npos) {
				std::string key = xmz::str::trim(core.substr(0, pos));
				std::string val = xmz::str::trim(core.substr(pos + 1));
				key = unescape_string(key);
				val = unescape_string(val);

				if (!key.empty()) {
					size_t first = line.find_first_not_of(" \t");
					L.indent = (first == std::string::npos) ? "" : line.substr(0, first);
					L.sep = core[pos];

					L.kind = ini_line::kind_key_value;
					L.section = current_section;
					L.key = key;
					L.value = val;

					size_t idx = lines_.size();
					if (!index_[current_section].count(key))
						key_order_[current_section].push_back(key);
					index_[current_section][key] = idx;
				}
			}
			lines_.push_back(std::move(L));
		}
		return true;
	}

	bool readstr(const std::string& content)
	{ return parse_str(content); }

	std::string get(const std::string& sec,
					const std::string& key,
					const std::string& def = "") const
	{
		auto si = index_.find(sec);
		if (si == index_.end()) return def;
		auto ki = si->second.find(key);
		if (ki == si->second.end()) return def;
		return lines_[ki->second].value;
	}

	int get_int(const std::string& sec,
				const std::string& key,
				int def = 0) const
	{
		auto s = get(sec, key);
		if (s.empty()) return def;
		try { return std::stoi(s); }
		catch (...) { return def; }
	}

	bool get_bool(const std::string& sec,
				const std::string& key,
				bool def = false) const
	{
		auto s = get(sec, key);
		if (s.empty()) return def;
		return s == "true" || s == "1" || s == "yes" || s == "on";
	}

	std::vector<std::string> get_sections() const
	{ return section_order_; }
	inline std::vector<std::string> get_sec() const
	{ return get_sections(); }

	std::vector<std::string> get_key(const std::string& sec) const
	{
		auto it = key_order_.find(sec);
		if (it != key_order_.end()) return it->second;
		return {};
	}

	void clear()
	{
		lines_.clear();
		index_.clear();
		section_order_.clear();
		key_order_.clear();
	}

	void set(const std::string& sec,
			const std::string& key,
			const std::string& val)
	{
		auto& kvs = index_[sec];
		auto it = kvs.find(key);
		if (it != kvs.end()) {
			auto& L = lines_[it->second];
			L.value = val;
			L.dirty = true;
			return;
		}

		size_t sec_start = std::string::npos;
		size_t sec_end	 = lines_.size();
		for (size_t i = 0; i < lines_.size(); ++i) {
			if (lines_[i].kind == ini_line::kind_section && lines_[i].section == sec) {
				sec_start = i;
				break;
			}
		}
		if (sec_start != std::string::npos) {
			sec_end = lines_.size();
			for (size_t i = sec_start + 1; i < lines_.size(); ++i) {
				if (lines_[i].kind == ini_line::kind_section) { sec_end = i; break; }
			}
		}

		size_t insert_at;
		if (sec_start == std::string::npos) {
			ini_line S;
			S.kind = ini_line::kind_section;
			S.section = sec;
			S.raw = "[" + sec + "]";
			insert_at = lines_.size();
			lines_.push_back(std::move(S));
			section_order_.push_back(sec);
			index_[sec];
			insert_at = lines_.size();
		} else
			insert_at = sec_end;

		ini_line L;
		L.kind = ini_line::kind_key_value;
		L.section = sec;
		L.key = key;
		L.value = val;
		L.sep = ':';
		L.raw = escape_string(key) + ": " + escape_string(val);

		lines_.insert(lines_.begin() + insert_at, std::move(L));

		for (auto& [s, m] : index_)
			for (auto& [k, i] : m)
				if (i >= insert_at && !(s == sec && k == key)) ++i;

		index_[sec][key] = insert_at;
		if (std::find(key_order_[sec].begin(),
			key_order_[sec].end(), key) == key_order_[sec].end())
			key_order_[sec].push_back(key);
	}

	void set_int(const std::string& sec, const std::string& key, int val)
	{ set(sec, key, std::to_string(val)); }
	void set_bool(const std::string& sec, const std::string& key, bool val)
	{ set(sec, key, val ? "true" : "false"); }

	bool remove_key(const std::string& sec, const std::string& key)
	{
		auto si = index_.find(sec);
		if (si == index_.end()) return false;
		auto ki = si->second.find(key);
		if (ki == si->second.end()) return false;

		size_t idx = ki->second;
		lines_.erase(lines_.begin() + idx);
		si->second.erase(ki);

		for (auto& [s, m] : index_)
			for (auto& [k, i] : m)
				if (i > idx) --i;

		auto& ko = key_order_[sec];
		ko.erase(std::remove(ko.begin(), ko.end(), key), ko.end());
		return true;
	}

	bool remove_section(const std::string& sec)
	{
		if (!index_.count(sec)) return false;

		std::vector<ini_line> new_lines;
		std::string current;
		for (auto& L : lines_) {
			if (L.kind == ini_line::kind_section) {
				current = L.section;
				if (current == sec) continue;
				new_lines.push_back(L);
				continue;
			}
			if (L.kind == ini_line::kind_key_value && L.section == sec) continue;
			new_lines.push_back(L);
		}
		lines_.swap(new_lines);

		rebuild_index();
		return true;
	}

	bool rmkey(const std::string& sec, const std::string& key)
	{ return remove_key(sec, key); }
	bool rmsec(const std::string& sec)
	{ return remove_section(sec); }

	bool save_cover(const std::string& filepath) const
	{
		std::string out = serialize();
		return xmz::fs::writefile(out, filepath);
	}

	bool save_append(const std::string& filepath) const
	{
		std::string out;
		for (const auto& L : lines_) {
			if (L.kind == ini_line::kind_key_value && L.dirty)
				out += L.indent + escape_string(L.key) + " " + L.sep + " " + escape_string(L.value) + "\n";
		}
		if (out.empty()) return true;
		return xmz::fs::append(out, filepath);
	}

	static std::string escape_string(const std::string& str)
	{
		std::string result;
		result.reserve(str.size() * 2);
		for (char c : str) {
			switch (c) {
				case '\n': result += "\\n"; break;
				case '\r': result += "\\r"; break;
				case '\t': result += "\\t"; break;
				case '\v': result += "\\v"; break;
				case '\f': result += "\\f"; break;
				case '\b': result += "\\b"; break;
				case '\\': result += "\\\\"; break;
				case '\"': result += "\\\""; break;
				case '\'': result += "\\'"; break;
				case ';': result += "\\;"; break;
				case '#': result += "\\#"; break;
				default: result += c; break;
			}
		}
		return result;
	}

	static std::string unescape_string(const std::string& str)
	{
		std::string result;
		result.reserve(str.size());
		for (size_t i = 0; i < str.size(); ++i) {
			if (str[i] == '\\' && i + 1 < str.size()) {
				char next = str[i + 1];
				switch (next) {
					case 'n': result += '\n'; ++i; break;
					case 'r': result += '\r'; ++i; break;
					case 't': result += '\t'; ++i; break;
					case 'v': result += '\v'; ++i; break;
					case 'f': result += '\f'; ++i; break;
					case 'b': result += '\b'; ++i; break;
					case '\\': result += '\\'; ++i; break;
					case '\"': result += '\"'; ++i; break;
					case '\'': result += '\''; ++i; break;
					case ';': result += ';'; ++i; break;
					case '#': result += '#'; ++i; break;
					default: result += str[i]; break;
				}
			} else
				result += str[i];
		}
		return result;
	}

	static std::string remove_comments(const std::string& line)
	{
		bool in_quotes = false;
		bool escape_next = false;
		std::string result;
		for (size_t i = 0; i < line.size(); ++i) {
			char c = line[i];
			if (escape_next) {
				result += c;
				escape_next = false;
				continue;
			}
			if (c == '\\') {
				if (i + 1 < line.size()) {
					char next = line[i + 1];
					if (next == ';' || next == '#') {
						result += c;
						result += next;
						++i;
						continue;
					}
				}
				escape_next = true;
				result += c;
				continue;
			}
			if (c == '"') {
				in_quotes = !in_quotes;
				result += c;
				continue;
			}
			if (!in_quotes && (c == ';' || c == '#')) return result;
			result += c;
		}
		return result;
	}
private:
	std::string serialize() const
	{
		std::string out;
		for (const auto& L : lines_) {
			switch (L.kind) {
				case ini_line::kind_blank:
				case ini_line::kind_comment:
				case ini_line::kind_section:
					out += L.raw + "\n";
					break;
				case ini_line::kind_key_value:
					if (L.dirty)
						out += L.indent + escape_string(L.key) + " " + L.sep + " " + escape_string(L.value) + "\n";
					else
						out += L.raw + "\n";
					break;
			}
		}
		return out;
	}

	void rebuild_index()
	{
		index_.clear();
		section_order_.clear();
		key_order_.clear();

		std::string current;
		for (size_t i = 0; i < lines_.size(); ++i) {
			auto& L = lines_[i];
			if (L.kind == ini_line::kind_section) {
				current = L.section;
				if (!index_.count(current)) {
					index_[current];
					section_order_.push_back(current);
				}
			} else if (L.kind == ini_line::kind_key_value) {
				if (!index_[current].count(L.key))
					key_order_[current].push_back(L.key);
				index_[current][L.key] = i;
			}
		}
	}
};
} /* namespace a1::ini */
#endif /* A1_MYINI_HPP */

