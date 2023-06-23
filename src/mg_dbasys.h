/*
   ----------------------------------------------------------------------------
   | mg_dba.so|dll                                                            |
   | Description: An abstraction of the InterSystems Cache/IRIS API           |
   |              and YottaDB API                                             |
   | Author:      Chris Munt cmunt@mgateway.com                               |
   |                         chris.e.munt@gmail.com                           |
   | Copyright (c) 2019-2023 MGateway Ltd                                     |
   | Surrey UK.                                                               |
   | All rights reserved.                                                     |
   |                                                                          |
   | http://www.mgateway.com                                                  |
   |                                                                          |
   | Licensed under the Apache License, Version 2.0 (the "License"); you may  |
   | not use this file except in compliance with the License.                 |
   | You may obtain a copy of the License at                                  |
   |                                                                          |
   | http://www.apache.org/licenses/LICENSE-2.0                               |
   |                                                                          |
   | Unless required by applicable law or agreed to in writing, software      |
   | distributed under the License is distributed on an "AS IS" BASIS,        |
   | WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. |
   | See the License for the specific language governing permissions and      |
   | limitations under the License.                                           |      
   |                                                                          |
   ----------------------------------------------------------------------------
*/

#ifndef MG_DBASYS_H
#define MG_DBASYS_H

#define MAJORVERSION             1
#define MINORVERSION             3
#define MAINTVERSION             17
#define BUILDNUMBER              17

#define DBX_VERSION_MAJOR        "1"
#define DBX_VERSION_MINOR        "3"
#define DBX_VERSION_BUILD        "17"

#define DBX_VERSION              DBX_VERSION_MAJOR "." DBX_VERSION_MINOR "." DBX_VERSION_BUILD
#define DBX_COMPANYNAME          "MGateway Ltd\0"
#define DBX_FILEDESCRIPTION      "API Abstraction for InterSystems IRIS/Cache and YottaDB\0"
#define DBX_FILEVERSION          DBX_VERSION
#define DBX_INTERNALNAME         "mg_dba\0"
#define DBX_LEGALCOPYRIGHT       "Copyright 2017-2023, MGateway Ltd\0"
#define DBX_ORIGINALFILENAME     "mg_dba\0"
#define DBX_PLATFORM             PROCESSOR_ARCHITECTURE
#define DBX_PRODUCTNAME          "mg_dba\0"
#define DBX_PRODUCTVERSION       DBX_VERSION
#define DBX_BUILD                DBX_VERSION

#endif

