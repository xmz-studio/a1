// myini.cc
// c++ myini.cc -I../../cxxa1 -o myini
#include <string>
#include <algorithm>

#include <a1/core/myini.hpp>

#include <libxmz/io.hpp>
#include <libxmz/log.hpp>
#include <libxmz/str.hpp>
#include <libxmz/fs.hpp>
#include <libxmz/aux.hpp>

a1::ini::ini_parser pini;

std::string help_text(const std::string& myself)
{
	return std::string(R"(Usage: )" + myself + " <command> [options] <file> [section.key|.key]") + R"(
Optins:
  -a, --save-append    Append only modified keys (default: cover)
  -c, --save-cover     Rewrite whole file in place (default)
Commands:
  get <file> [section.]key              Get string value
  get-int <file> [section.]key          Get integer value
  get-bool <file> [section.]key         Get boolean value
  set <file> [section.]key <val>        Set string value
  set-int <file> [section.]key <val>    Set integer value
  set-bool <file> [section.]key <val>   Set boolean value
  list-sec <file>                       List sections
  list-key <file> [section]             List keys in section
  rm-key <file> [section.]key           Remove key
  rm-sec <file> <section>               Remove section
  help                                  Show this help
  version                               Show version
)";
}

std::pair<std::string, std::string>
parse_section_key(const std::string& input)
{
	if (input.empty()) return {"", ""};
	if (input[0] == '.')
		return {"", input.substr(1)};
	auto pos = input.find('.');
	if (pos != std::string::npos)
		return {input.substr(0, pos), input.substr(pos + 1)};
	return {"", input};
}

int main(int argc, char *argv[])
{
	if (argc < 2) {
		xmz::println(help_text(argv[0]));
		return 1;
	}

	std::string cmd = argv[1];

	if (cmd == "help" || cmd == "--help" || cmd == "-h") {
		xmz::println(help_text(argv[0]));
		return 0;
	}

	if (cmd == "version" || cmd == "-V" || cmd == "--version") {
		xmz::println(argv[0], "version 0.0.1");
		return 0;
	}

	enum class save_mode { cover, append };
	save_mode savemode = save_mode::cover; // default

	std::vector<std::string> args;
	bool end_of_opts = false;
	for (int i = 1; i < argc; ++i) {
		std::string a = argv[i];
		if (!end_of_opts && a == "--")
		{ end_of_opts = true; continue; }
		if (!end_of_opts) {
			if (a == "-a" || a == "--save-append")
			{ savemode = save_mode::append; continue; }
			if (a == "-c" || a == "--save-cover")
			{ savemode = save_mode::cover;  continue; }
		}
		args.push_back(std::move(a));
	}

	auto arg  = [&](size_t i) -> std::string
	{ return (i < args.size()) ? args[i] : std::string{}; };
	auto argn = [&]() -> size_t { return args.size(); };

	auto do_save = [&](const std::string& path) -> bool {
		if (savemode == save_mode::append)
			return pini.save_append(path);
		return pini.save_cover(path);
	};

	if (cmd == "list-sec" && argn() >= 1) {
		std::string filepath = arg(0);
		if (!xmz::aux::is_file(filepath)) {
			xmz::log::error("File not found:", filepath);
			return 1;
		}
		pini.parse_file(filepath);
		auto secs = pini.get_sec();
		for (const auto& s : secs) xmz::println(s);
		return 0;
	}

	if (cmd == "list-key" && argn() >= 1) {
		std::string filepath = arg(0);
		std::string section  = (argn() >= 2) ? arg(1) : "";
		if (!xmz::aux::is_file(filepath)) {
			xmz::log::error("File not found:", filepath);
			return 1;
		}
		pini.parse_file(filepath);
		auto keys = pini.get_key(section);
		for (const auto& k : keys) xmz::println(k);
		return 0;
	}

	if (cmd == "rm-key" && argn() >= 2) {
		std::string filepath = arg(0);
		auto [sec, key] = parse_section_key(arg(1));
		if (!xmz::aux::is_file(filepath)) {
			xmz::log::error("File not found:", filepath);
			return 1;
		}
		pini.parse_file(filepath);
		if (pini.rmkey(sec, key)) {
			if (!do_save(filepath)) {
				xmz::log::error("Save failed:", filepath);
				return 1;
			}
			xmz::println("Removed key:", sec.empty() ? key : sec + "." + key);
		} else {
			xmz::log::error("Key not found:", sec.empty() ? key : sec + "." + key);
			return 1;
		}
		return 0;
	}

	if (cmd == "rm-sec" && argn() >= 2) {
		std::string filepath = arg(0);
		std::string section  = arg(1);
		if (!xmz::aux::is_file(filepath)) {
			xmz::log::error("File not found:", filepath);
			return 1;
		}
		pini.parse_file(filepath);
		if (pini.rmsec(section)) {
			if (!do_save(filepath)) {
				xmz::log::error("Save failed:", filepath);
				return 1;
			}
			xmz::println("removed section:", section);
		} else {
			xmz::log::error("section not found:", section);
			return 1;
		}
		return 0;
	}

	if ((cmd == "get" || cmd == "get-str" ||
		cmd == "get-int" || cmd == "get-bool") && argn() >= 2) {
		std::string filepath = arg(0);
		auto [sec, key] = parse_section_key(arg(1));
		if (key.empty()) {
			xmz::log::error("Key is required");
			return 1;
		}
		if (!xmz::aux::is_file(filepath)) {
			xmz::log::error("File not found:", filepath);
			return 1;
		}
		pini.parse_file(filepath);
		if (cmd == "get" || cmd == "get-str") {
			std::string val = pini.get(sec, key);
			xmz::println(val);
		} else if (cmd == "get-int") {
			int val = pini.get_int(sec, key);
			xmz::println(val);
		} else if (cmd == "get-bool") {
			bool val = pini.get_bool(sec, key);
			xmz::println(val ? "true" : "false");
		}
		return 0;
	}

	if ((cmd == "set" || cmd == "set-str" ||
		cmd == "set-int" || cmd == "set-bool") && argn() >= 3) {
		std::string filepath = arg(0);
		auto [sec, key] = parse_section_key(arg(1));
		std::string val = arg(2);
		if (key.empty()) {
			xmz::log::error("Key is required");
			return 1;
		}
		if (!xmz::aux::is_file(filepath)) { xmz::fs::touch(filepath); }
		pini.parse_file(filepath);
		if (cmd == "set" || cmd == "set-str") {
			pini.set(sec, key, val);
			xmz::println("Set", sec.empty() ? key : sec + "." + key, "=/:", val);
		} else if (cmd == "set-int") {
			try {
				int ival = std::stoi(val);
				pini.set_int(sec, key, ival);
				xmz::println("Set", sec.empty() ? key : sec + "." + key, "=/:", ival);
			} catch (...) {
				xmz::log::error("Invalid integer value:", val);
				return 1;
			}
		} else if (cmd == "set-bool") {
			bool bval = (val == "true" || val == "1" ||
						val == "yes" || val == "on");
			pini.set_bool(sec, key, bval);
			xmz::println("Set", sec.empty() ? key : sec + "." + key, "=/:", (bval ? "true" : "false"));
		}
		if (!do_save(filepath)) {
			xmz::log::error("Save failed:", filepath);
			return 1;
		}
		return 0;
	}

	xmz::log::error("Unknown command:", cmd);
	xmz::println(help_text(argv[0]));
	return 1;
}

