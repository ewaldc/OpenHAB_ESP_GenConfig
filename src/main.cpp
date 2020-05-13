//#define ArduinoJsonDebug
#define OpenHABDebug
// WiFi parameters are inlcuded from secret.h which is not posted on GitHub
// Edit include/secret_example.h and rename to include/secret.h
#include "OHGenConfig.h"

uint8_t itemCount = 0, groupCount = 0;      // required for GenConfig
// Global parameters for configuration generation
// -------------- Modify ------------------------
static const char *SITEMAP = "demos";
static const char *OPENHAB_SERVER = "192.168.1.20";
static const int  LISTEN_PORT = 8080;
#define OPENHAB_ESP_PATH "\\Documents\\Arduino\\OpenHAB_ESP\\"  // %USERPROFILE% will be prefixed to this PATH
#define LISTEN_PORT 8080                // The port to listen for incoming TCP connections
// -------------- Modify ------------------------

OpenHab OpenHabServer(LISTEN_PORT);   // Create an instance of the OpenHAB server

// For every item that is changed by the end user (UI), this function will be called

int main() {
	// Set console code page to UTF-8 so console known how to interpret string data
	SetConsoleOutputCP(CP_UTF8);
	// Enable buffering to prevent VS from chopping up UTF-8 byte sequences
	setvbuf(stdout, nullptr, _IOFBF, 1000);
	std::cout << "OpenHab Generate Configuration for sitemap: " << SITEMAP << "\n";

	OpenHabServer.GenConfig(OPENHAB_SERVER, LISTEN_PORT, SITEMAP, OPENHAB_ESP_PATH);
	DbgPrintln("OpenHab config generation completed");
}

// Run program: Ctrl + F5 or Debug > Start Without Debugging menu
// Debug program: F5 or Debug > Start Debugging menu

// Tips for Getting Started: 
//   1. Use the Solution Explorer window to add/manage files
//   2. Use the Team Explorer window to connect to source control
//   3. Use the Output window to see build output and other messages
//   4. Use the Error List window to view errors
//   5. Go to Project > Add New Item to create new code files, or Project > Add Existing Item to add existing code files to the project
//   6. In the future, to open this project again, go to File > Open > Project and select the .sln file
