#include "OHGenConfig.h"
// Following two variables are defined in main.cpp
extern uint8_t itemCount, groupCount, pageCount;

//OpenHab::OpenHab(Item *items, unsigned short itemCount, const int port) : 
OpenHab::OpenHab(const int port) : 
  _port(port),
  _itemList(nullptr),
  _groupList(nullptr)
  {}

DynamicJsonDocument OpenHab::getJsonDocFromFile(String fileName) {
  DbgPrint(F("getJsonDocFromFile: file name: "), fileName);
  ifstream f(fileName);
  DynamicJsonDocument doc(64 * 1024);
  DbgPrint(" - allocated: ", doc.capacity());
  DeserializationError err = deserializeJson(doc, f, DeserializationOption::NestingLimit(20));
  DbgPrintln(" - deserializeJson: ", doc.memoryUsage());
  if (err == DeserializationError::Ok) doc.shrinkToFit();
  else DbgPrintln(F("deserializeJson failed "), err.c_str());
  f.close();
  return doc;
}


void erase (char *str, const char *pattern) {
  char *p = str;
  size_t len = strlen(pattern);
  while (*p) {
    if (strncmp(p, pattern, len) == 0) p += len;
    if (str != p) *str = *p;
    str++; p++;
  }
  *str = '\0';
}

FORCE_INLINE static void convDateTimeJavaToC(char *dateTime) {
  erase(dateTime, "1$t");
  DbgPrint(" - convDateTimeJavaToC: ", dateTime);
}

char *genFormattedLabel(String label, String pattern, const char* state) {
  String formattedLabel;
  int pos;
  // First check for MAP(<lang>.map) in pattern and remove (until MAP support)
  pos = pattern.find("MAP(");
  if (pos >= 0) {
    DbgPrint(" - MAP pattern: ", pattern);
    pos = pattern.find("):", pos) + 2;
    pattern = pattern.substr(pos);
    DbgPrint(" - MAP pattern cleaned up: ", pattern);
  }
  formattedLabel = label.substr(0, label.find('[') + 1);
  formattedLabel += pattern;
  formattedLabel += label.substr(label.find(']'));
  pos = formattedLabel.find("%unit%");
  if (pos >= 0) {
    String s = state;
    String unit = s.substr(s.find(' ') + 1);
    formattedLabel.replace(pos, sizeof("%unit%")-1 , unit);
  }
  return strdup(formattedLabel.c_str());
}

FORCE_INLINE OpenHab::Group *OpenHab::getGroup(const char *name) {
  Group *group = _groupList;
  while (group && (strcmp(group->name, name)))
    group = group->next;
  return group;
}

FORCE_INLINE uint8_t OpenHab::getPageIdx(const char *name) {
  uint8_t i;
  for (i = 0; i < pageCount; i++)
    if (strcmp(Pages[i], name) == 0) return i;
  if (i == 255)
    DbgPrintln("ERROR Maximum number of Pages > 255 !");
  else Pages[pageCount++] = strdup(name);
  return i;
}

FORCE_INLINE OpenHab::Item *OpenHab::getItem(const char *name) {
  Item *item = _itemList;
  while (item && (strcmp(item->name, name)))
    item = item->next;
  return item;
}

// type == ItemNone: we just have a group name and item is a group member
// type == ItemGroup: we have a true group, check groupType for a group with dynamic label & state 
OpenHab::Group *OpenHab::newGroup(const char *groupName, Item *item, JsonObject obj, ItemType type) {
  GroupFunction groupFunction = FunctionNone;
  //JsonObject obj = item->obj;
  JsonVariant groupTypeVariant = obj[F("groupType")];  
  ItemType groupType = (groupTypeVariant.isNull()) ? ItemNull : getItemType(groupTypeVariant);
  DbgPrint(F("adding group with name: "), groupName);
  DbgPrintln(F(" - grouptype: "), groupType);
  
  if (type == ItemGroup) { // item refers to a true group item
    JsonObject groupFunctionObj = obj[F("function")];  // if it has a function, it has a dynamic label & state
    if (!groupFunctionObj.isNull()) {
      const char *groupFunctionStr = groupFunctionObj[F("name")];
      if (!strcmp(groupFunctionStr, PSTR("AVG")))
        groupFunction = FunctionAVG;
      else if (!strcmp(groupFunctionStr, PSTR("OR"))) groupFunction = FunctionOR;
    }
    DbgPrintln(F(" - groupfunction: "), groupFunction);
  }   

  if (Group *group = getGroup(groupName)) { // we might already have a group with this name but without group leader
    if (groupType != ItemNull) { // group with dynamic label/state found
      DbgPrintln(F("Group leader found: "), groupName);
      group->type = groupType; // update type   
      group->function = groupFunction; // update function
    }
    if (type != ItemGroup) { // we are adding an item to an exising group
      group->itemList = new ItemList{item, group->itemList};
      group->itemCount++;
    }
    return group;
  }
  DbgPrintln(F(" - insert new group in list: "), groupName);
  groupCount++;
  return _groupList = new Group {groupName, item, (type == ItemNull) ? new ItemList{item, nullptr} : nullptr, _groupList,
    groupType, groupFunction, nullptr, (type == ItemNull) ? (uint8_t)1 : (uint8_t)0 };
}

FORCE_INLINE OpenHab::Group *OpenHab::addItemToGroup(const char *groupName, Item *item, JsonObject obj) {
  DbgPrint(F("addItemToGroup adding item named: "), item->name);
  DbgPrintln(F(" - to group: "), groupName);
  return newGroup(groupName, item, obj, ItemNull); // create or get group
}

void OpenHab::GenSitemap(const JsonVariant prototype, const char *pageId, size_t uriBaseLen) {
  if (prototype.is<JsonObject>()) {
    const JsonObject& obj = prototype;
    for (const JsonPair pair : obj) {
      uint8_t pageIdx;
      const char *key = pair.key().c_str();
      if (!strcmp(key, "link")) { // Links to sitemap or item
        const char* link = pair.value().as<const char*>();
        obj[pair.key()] = link + uriBaseLen;
        DbgPrintln("Key: link - value: ", link + uriBaseLen);
      } else if (!strcmp_P(key, PSTR("linkedPage")) || !strcmp_P(key, PSTR("homepage"))) { // Pages
        JsonObject pageObj = pair.value().as<JsonObject>();
        pageId = pageObj[F("id")];
        getPageIdx(pageId);
        DbgPrintln(F("Key: page: "), pageId);
		  } else if (!strcmp(key, "item")) {// Items
        JsonObject itemObj = pair.value().as<JsonObject>();
        const char *name = itemObj["name"];
        DbgPrint("Key: item - name: ", name);
        const char *state = itemObj["state"];
        itemObj["state"] = ""; // we have copied state, discard it in JSON
        DbgPrint(" - state: ", state);
        const char *transformedState = itemObj["transformedState"];
        if (transformedState) itemObj["transformedState"] = ""; // we have copied transformed state, discard it in JSON
        JsonObject stateObj = itemObj["stateDescription"];

        if (Item *item = getItem(name)) { // Test if item has already been created
          item->refCount++;     // Increase reference count
          DbgPrint(" - referenceCount", to_string(item->refCount));
          if (stateObj) itemObj.remove("stateDescription");  // We have it still in master item
          if (item->labelFormat) obj["label"] = ""; // Discard formatted label for widget in Json, we have it in master
        } else {  // New item
          itemCount++;  // Increase global count of items
          DbgPrint(" - itemCount: ", to_string(itemCount));
          const char *label = obj["label"];
          DbgPrint(" - label: ", label);
          ItemType itemType = getItemType(itemObj["type"]);
          DbgPrint(" - type: ", to_string(itemType));

          char *pattern = nullptr;
          if (stateObj) {
            pattern = (transformedState) ? "%s" : stateObj["pattern"].as<const char *>();
            //DbgPrint(" - pattern: ", pattern); 
            JsonArray options = stateObj["options"];
            if (options.size() > 0) {
              for (JsonVariant option : options)
                if (!strcmp(option["value"], state))
                  state = option["label"];
              DbgPrint(" - options state: ", state);
            } else itemObj.remove("stateDescription"); //unless we have 'options' we no longer need statedesciption
          }
          char *formattedLabel = nullptr;
          if (pattern) { // we have a formatted label
            formattedLabel = genFormattedLabel(label, pattern, state);
            DbgPrint(" - formattedLabel: ", formattedLabel);
            if (itemType == ItemDateTime)
              convDateTimeJavaToC(formattedLabel); // convert Java label format to C/C++ strftime format
            obj["label"] = ""; // Discard formatted label for widget in Json
          }
          DbgPrintln("");
          Group *group = (itemType == ItemGroup ) ? newGroup(name, nullptr, itemObj, ItemGroup) : nullptr;
          _itemList = new Item{strdup(name), itemType, 0, 255, getPageIdx(pageId), strcmp(state, "NULL") ? strdup(state) : nullptr, 
            (formattedLabel) ? strdup(formattedLabel) : nullptr, group, _itemList};

          JsonArray groupNames = itemObj["groupNames"];
          if (groupNames.size() > 0)
            for (JsonVariant groupName : groupNames)
              addItemToGroup(groupName, _itemList, itemObj);
        }
      }
      GenSitemap(pair.value().as<JsonObject>(), pageId, uriBaseLen); //, pair.key);
      for (auto arr : pair.value().as<JsonArray>()) {
        GenSitemap(arr.as<JsonVariant>(), pageId, uriBaseLen); //, pair.key);
      }
    }
  } else if (prototype.is<JsonArray>()) {
    const JsonArray& arr = prototype;
    for (const auto& elem : arr) GenSitemap(elem.as<JsonVariant>(), pageId, uriBaseLen); //, parent);
  }
}

void OpenHab::GetREST(const char *uriBase, String path, const char* restURI) {
  DbgPrint("GetRestData - URI: ", restURI);
  DbgPrint(" - path: ", path);
  DbgPrint(" - uriBase: ", uriBase);
  DbgPrint(" - port: ", _port);
  httplib::Client httpClient (uriBase, _port);
  shared_ptr<httplib::Response> response = httpClient.Get(restURI); //Send the request
  if (response && response->status == 200) {
    String uri = String("http://") + uriBase + String(":") + to_string(_port);
    //DbgPrintln(" - erasing: ", uri.c_str());
    const char *body = (response->body).c_str();
    erase((char *)body, uri.c_str());
    //DbgPrintln(" - body: ", body);
    String fileName = String(path) + String(restURI);
    ofstream wfile(fileName, ios_base::trunc);
    if (wfile)
      wfile << body << endl;
    else DbgPrintln("Failed to open file: ", fileName);
    DbgPrintln(" - HTTP GET completed");
    wfile.close();
  } else DbgPrintln("HTTP GET failed");  
}

void OpenHab::GetSitemap(const char *uriBase, const char *sitemap) {
  DbgPrint("GenConfig - uriBase: ", uriBase);
  DbgPrint(" - port: ", _port);
  httplib::Client httpClient (uriBase, _port);
  String uri = "/rest/sitemaps/";
  uri += sitemap;
  DbgPrintln(" - uri: ", uri.c_str());
  shared_ptr<httplib::Response> response = httpClient.Get(uri.c_str()); //Send the request
  if (response && response->status == 200) {
    ofstream wfile("sitemap.tmp", ios_base::trunc);
    if (!wfile.bad())
      wfile << response->body << endl;
    DbgPrintln(" - HTTP GET completed");
    wfile.close();
    // Read sitemap as JsonObject from file
    _sitemap = new Sitemap{"sitemap.tmp", getJsonDocFromFile("sitemap.tmp")};
  } else DbgPrintln("HTTP GET failed");  
}

void OpenHab::GenConfig(const char *OpenHabServer, const int port, const char *sitemap, const char *path) {
  char *userPath = getenv("USERPROFILE");
  String ESPpath = String(userPath) + String(path);
  String uriBase = String("http://") + OpenHabServer + String(":") + to_string(_port);
  // Get sitemap from full OpenHab 2.X server and write to SPIFFS in a buffered way
  GetSitemap(OpenHabServer, sitemap);
  GetREST(OpenHabServer, ESPpath + String("data"), "/rest/items");
  GetREST(OpenHabServer, ESPpath + String("data"), "/rest/services");
  GetREST(OpenHabServer, ESPpath + String("data\\conf"), "/rest");

  // Adapt the sitemap to conserve memory
  GenSitemap(_sitemap->jsonDoc.as<JsonObject>(), nullptr, uriBase.length());

  // Print out the generated sitemap Json Object
  String sitemapFilename = ESPpath + String("data/conf/sitemaps/") + sitemap;
  ofstream sitemapFile(sitemapFilename.c_str(), ios::trunc);
  serializeJson(_sitemap->jsonDoc, sitemapFile);
  sitemapFile << endl;
  
  // Create an array of pointers to list of Items
  DbgPrintln(F("Item count: "), to_string(itemCount));
  Item **itemList = new Item*[itemCount];
  Item *itemPtr = _itemList;

  auto getItemIdx = [&](const char *name) {
    uint8_t middle, first = 0, last = itemCount;
    do {
      middle = (first + last) >> 1;
      int cmp = strcmp(itemList[middle]->name, name);
      if (cmp == 0) return middle;
      else if (cmp < 0) first = middle + 1;
      else last = middle - 1;
    } while (first <= last);
    return (uint8_t) 255;   
  };

  for (unsigned int i = 0; i < itemCount; i++, itemPtr = itemPtr->next) {
    itemList[i] = itemPtr;
  }

  // Bubble sort Items
  unsigned int n = 1;
  while (n < itemCount) {
    if (strcmp(itemList[n]->name, itemList[n - 1]->name) < 0) {
      Item *item = itemList[n];
      itemList[n] = itemList[n - 1];
      itemList[n - 1] = item;
      if (n > 1) n--;
    } else n++;
  }

  // Create an array of pointers to list of Groups
  DbgPrintln(F("Total Group count: "), to_string(groupCount));
  Group **groupList = new Group*[groupCount];
  uint8_t groupCnt = 0;
  for (n = 0; n < itemCount; n++) {
    if (itemList[n]->type == ItemGroup) {
      groupList[groupCnt] = itemList[n]->group;
      itemList[n]->groupIdx = groupCnt++;
    }
  }
  DbgPrintln(F("Dropped groups which are not used in Sitemap: "), to_string(groupCount - groupCnt));
  DbgPrintln(F("Final Group count: "), to_string(groupCount));

  // Finally, write out the generated item, itemState and group list in include/main.h
  String items = "OpenHab::Item items[] PROGMEM = {";
  for (unsigned int i = 0; i < itemCount; ) {
    items += "{\"";
    items += itemList[i]->name;
    items += "\", ";
    items += ItemTypeString[itemList[i]->type];
    items += ", ";
    items += to_string(itemList[i]->refCount);
    items += ", ";
    items += to_string(itemList[i]->groupIdx);
    items += ", ";
    items += to_string(itemList[i]->pageIdx);
    items += ", ";
    const char *state = itemList[i]->state;
    if (state) {  // Non-empty state
      items += "\"";
      items += state;
      items += "\", ";
    } else
      switch (itemList[i]->type ) { // Empty state -> make "" or "0"
        case ItemNumber:
        case ItemNumber_Angle:
        case ItemDimmer:
        case ItemRollerShutter:
        case ItemColor:
          items += "\"0\", "; break;
        default:  // All other types
           //items += "\"\", ";
           items += "nullptr, ";
      }  
    const char *labelFormat = itemList[i]->labelFormat;
    if (labelFormat) {
      items += "\"";
      items += labelFormat;
      items += "\"}";    
    } else items += "nullptr}";
    if (++i == itemCount) items += "};";
    else items += ", ";
  }

  //String itemState = "OpenHab::ItemState itemStates[";
  //itemState += to_string(itemCount);
  //itemState += "];";

  String groups = "OpenHab::Group groups[] = {";
  for (unsigned int i = 0; i < groupCnt; ) {
    groups += "{";
    //groups += groupList[i]->name;
    //groups += ", ";   
    groups += ItemTypeString[groupList[i]->type];
    groups += ", (GroupFunction) ";
    groups += to_string(groupList[i]->function);
    groups += ", new uint8_t[";
    groups += to_string(groupList[i]->itemCount);
    groups += "]{";
    ItemList *member = groupList[i]->itemList;
    while (member) {
      groups += to_string(getItemIdx(member->item->name));
      member = member->next;
      if (member) groups += ", ";
    }
    groups += "}, ";
    groups += to_string(groupList[i]->itemCount);
    if (++i == groupCount) groups += "}};";
    else groups += "}, ";
  }

  // Create an array of pointers to page Id's
  DbgPrintln("Page count: ", to_string(pageCount));
  String pages = "const char *pages[] PROGMEM = {\"";
  for (unsigned int i = 0; i < pageCount; ) {
    pages += Pages[i]; 
    pages += (++i == pageCount) ? "\"};" : "\", \"";
  }
  // Copy the following lines to include/main.h;
  String mainIncludeFile = String(ESPpath) + String("include/main.h");
  ofstream file(mainIncludeFile, ios::trunc);
  file << "extern const uint8_t itemCount = " << to_string(itemCount) << ";\n"; // max 255 items on ESP8266
  file << "char *itemStates[itemCount];\n";
  file << "OpenHab::ItemReference *itemRef[itemCount];\n";
  file << items << endl;
  file << "extern const uint8_t groupCount = " << to_string(groupCnt) << ";\n"; // max 255 items on ESP8266
  file << groups << endl;
  file << "extern const uint8_t pageCount = " << to_string(pageCount) << ";\n"; // max 255 items on ESP8266
  file << pages << endl;
  file << "JsonObject pageObj[pageCount];\n"; // max 255 items on ESP8266
  file.close();
}

