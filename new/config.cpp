#include "config.h"

using namespace std;


string upgradeServerUrl;
string upgradeServerPath;
string appName;
string version;
string appDir;
string downloadDir;
string beforeUpgradeScript;
string afterUpgradeScript;
string needBackup;


// ===================
// 讀取設定檔
// fileName: 設定檔名稱
// ===================
void readConfig() {
    string filePath = "/home/pascal/projects/new/upgrade-client/upgrade/config/upgrade.cfg";
    ifstream configFile(filePath);
    string line;
    map<string, string> configValues;


    // 1. 讀取 cfg 設定檔
    cout << "[Step 1] 讀取 cfg 設定檔 " << endl;
    if (configFile.is_open()) {
        cout << "讀取 " << filePath << endl << "==================================================" << endl;
        while (getline(configFile, line)) {
            size_t delimiterPos = line.find("=");
            if (delimiterPos != string::npos) {
                string key = line.substr(0, delimiterPos);
                string value = line.substr(delimiterPos + 1);
                configValues[key] = value;
                cout << key << " = " << value << endl;
            }
        }
        cout << "==================================================" << endl;
        configFile.close();
    } else {
        throw runtime_error("無法打開配置文件: " + filePath);
    }

    upgradeServerUrl = configValues["Upgrade_Server_Url"];
    upgradeServerPath = configValues["Upgrade_Server_Path"];
    appName = configValues["App_Name"];
    version = configValues["Version"];
    appDir = configValues["App_Dir"];
    downloadDir = configValues["Download_Dir"];
    beforeUpgradeScript = configValues["Before_Upgrade_Script"];
    afterUpgradeScript = configValues["After_Upgrade_Script"];
    needBackup = configValues["Need_Backup"];

}

