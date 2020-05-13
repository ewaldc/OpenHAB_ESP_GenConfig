# OpenHab ESP Configuration Generator

This generator is in support of [OpenHab ESP](https://github.com/ewaldc/OpenHAB-ESP), a lightweight, simplied OpenHab 2 server for the ESP Platform

## Getting started ##   

1. Download or clone the repository
1. Install VSCode and EasyCPP extension (Windows portable version is OK and tested)
1. Install Microsoft Visual C++ compiler (VS17/VS19 tested)
1. Modify _.vscode/c_cpp_properties.json_ and _build.bat_ with the location of your MSVC compiler and include files
1. Modify _src/main.cpp_ file with the name of your development OpenHab server (e.g. on Windows or Linux), the port it listens to, the sitemap you want to use and the location of your OpenHab ESP project, also using vscode
1. Compile and run
1. The generator will copy all generator files to your OpenHab ESP project folder
- include/main.h: key structures list list of Items, Groups, item/group count etc.
- data/rest, data/conf/sitemap/, data/rest/services, data/rest/items extracted via REST API

You are done now!
Head over to your OpenHab ESP project to build and run your OH2 server for ESP

## Limitations and know issues ##
* At this moment only Windows 10 64-bit platform is supported
* Not all files are extracted from the OpenHab 2.x development server (e.g. icons).  However, all (static) files needed exist already in the data folder of the OpenHab ESP project folder, including a set of icons to run the demo.
* HabPanel is not (yet) supported by the generator, even though the OpenHab ESP server has initial support for HabPanel (copy the HabPanel files manually)
* Tested on OpenHab 2.4 and 2.5
