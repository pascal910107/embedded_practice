#ifndef CONFIG_H
#define CONFIG_H

#include <iostream>
#include <fstream>
#include <string>
#include <map>
#include <stdexcept>
#include <string>


extern std::string upgradeServerUrl;
extern std::string upgradeServerPath;
extern std::string appName;
extern std::string version;
extern std::string appDir;
extern std::string downloadDir;
extern std::string beforeUpgradeScript;
extern std::string afterUpgradeScript;
extern std::string needBackup;


void readConfig();



#endif // CONFIG_H
