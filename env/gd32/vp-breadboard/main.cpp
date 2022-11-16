#include <QApplication>
#include <QDirIterator>
#include <iostream>
#include <string>
#include <vector>

#include "window/window.h"

class InputParser {
   public:
	InputParser(int &argc, char **argv) {
		for (int i = 1; i < argc; ++i) this->tokens.push_back(std::string(argv[i]));
	}
	const std::string &getCmdOption(const std::string &option) const {
		std::vector<std::string>::const_iterator itr;
		itr = std::find(this->tokens.begin(), this->tokens.end(), option);
		if (itr != this->tokens.end() && ++itr != this->tokens.end()) {
			return *itr;
		}
		static const std::string empty_string("");
		return empty_string;
	}
	bool cmdOptionExists(const std::string &option) const {
		return std::find(this->tokens.begin(), this->tokens.end(), option) != this->tokens.end();
	}

   private:
	std::vector<std::string> tokens;
};

int main(int argc, char *argv[]) {
	QApplication a(argc, argv);
	std::string configfile = ":/conf/tft_shield.json";
	std::string scriptpath = "";
	std::string host = "localhost";
	bool overwrite_integrated_devices = false;

	InputParser input(argc, argv);
	if (input.cmdOptionExists("-h") || input.cmdOptionExists("--help")) {
		std::cout << "Usage: " << argv[0] << " [options]" << std::endl;
		std::cout << "    options:" << std::endl;
		std::cout << "\t-h, --help\t prints this help text" << std::endl;
		std::cout << "\t-c <config_file> (default " << configfile << ")" << std::endl;
		std::cout << "\t\t\t Integrated config-files:";
		{
			QDirIterator it(":/conf");
			if (!it.hasNext())
				std::cout << " NONE";
			while (it.hasNext()) {
				std::cout << std::endl << "\t\t\t   " << it.next().toStdString();
			}
			std::cout << std::endl;
		}
		std::cout << "\t-s <custom_device_folder>" << std::endl;
		std::cout << "\t\t\t Builtin scripted devices:";
		{
			QDirIterator it(":/devices/lua");
			if (!it.hasNext())
				std::cout << " NONE";
			while (it.hasNext()) {
				it.next();
				std::cout << std::endl << "\t\t\t   " << it.fileName().toStdString();
			}
			std::cout << std::endl;
		}
		std::cout << "\t--overwrite \tCustom scripts will take priority in registry" << std::endl;
		std::cout << "\t-d <target_host> (default " << host << ")" << std::endl;
		return 0;
	}

	if (argc <= 1) {
		std::cout << "Using default parameters. For usage instructions, pass -h" << std::endl;
	}

	{
		const std::string &conf_c = input.getCmdOption("-c");
		if (!conf_c.empty()) {
			configfile = conf_c;
		}
	}
	{
		const std::string &host_c = input.getCmdOption("-d");
		if (!host_c.empty()) {
			host = host_c;
		}
	}
	{
		const std::string &scriptpath_c = input.getCmdOption("-s");
		if (!scriptpath_c.empty()) {
			scriptpath = scriptpath_c;
		}
	}
	{ overwrite_integrated_devices = input.cmdOptionExists("--overwrite"); }

	MainWindow w(QString(configfile.c_str()), scriptpath.c_str(), host.c_str(), overwrite_integrated_devices);
	w.show();

	return a.exec();
}
