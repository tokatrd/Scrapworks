//
//  CPluginsUpdate.hpp
//  obs-browser
//
//  Created by apple on 2019/5/30.
//

#ifndef CPluginsUpdate_hpp
#define CPluginsUpdate_hpp

#include <stdio.h>
#include <string>
using namespace std;
int getUUID(char uuid[256]);
int popSaveService(const char * message);
string getDistVersion();
string getPluginVersion();

bool path_file_exit(const char *path);
string getFrameworkspath(const char *path);
bool shouldAddSourceToScene(const char *configPath);
void setupSourceHaveAdded(const char *configPath);
#endif /* CPluginsUpdate_hpp */
