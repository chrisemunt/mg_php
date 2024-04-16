# mg_php

A PHP Extension for InterSystems **Cache/IRIS** and **YottaDB**.

Chris Munt <cmunt@mgateway.com>  
16 April 2024, MGateway Ltd [http://www.mgateway.com](http://www.mgateway.com)

* Current Release: Version: 3.3; Revision 62.
* Verified to work with PHP versions up to (and including) v8.2.x.
* Two connectivity models to the InterSystems or YottaDB database are provided: High performance via the local database API or network based.
* [Release Notes](#relnotes) can be found at the end of this document.

Contents

* [Overview](#overview)
* [Prerequisites](#prereq)
* [Installing mg\_php](#install)
* [PHP Configuration](#phpconfig)
* [Connecting to the database](#connect)
* [Invocation of database commands](#dbcommands)
* [Invocation of database functions](#dbfunctions)
* [Transaction Processing](#tprocessing)
* [Direct access to InterSystems classes (IRIS and Cache)](#dbclasses)
* [License](#license)


## <a name="overview">Overview</a>

**mg\_php** is an Open Source PHP extension developed for InterSystems **Cache/IRIS** and the **YottaDB** database.  It will also work with the **GT.M** database and other **M-like** databases.


## <a name="prereq">Prerequisites</a> 

PHP installation:

       http://www.php.net/

InterSystems **Cache/IRIS** or **YottaDB** (or similar M database):

       https://www.intersystems.com/
       https://yottadb.com/


## <a name="install">Installing mg\_php</a>

There are three parts to **mg\_php** installation and configuration.

* The PHP extension (**mg\_php.so** or **mg\_php.dll**).
* The DB Superserver: the **%zmgsi** routines.
* A network configuration to bind the former two elements together.

### Building the mg\_php extension

**mg\_php** is written in standard C.  For Linux systems, the PHP installation procedure can use the freely available GNU C compiler (gcc) which can be installed as follows.

Ubuntu:

       apt-get install gcc

Red Hat and CentOS:

       yum install gcc

Apple OS X can use the freely available **Xcode** development environment.

There are two options for Windows, both of which are free:

* Microsoft Visual Studio Community: [https://www.visualstudio.com/vs/community/](https://www.visualstudio.com/vs/community/)
* MinGW: [http://www.mingw.org/](http://www.mingw.org/)

There are built Windows x64 binaries available from:

* [https://github.com/chrisemunt/mg_php/blob/master/bin/winx64](https://github.com/chrisemunt/mg_php/blob/master/bin/winx64)

Having created a suitable development environment, the PHP Extension installer can be used to build and deploy **mg\_php**.  You will find the setup scripts in the /src directory of the distribution.

#### UNIX

Invoke the following commands from the /src directory (i.e. the directory containing **config.m4** file).

       phpize
       ./configure
       make
       make install

#### Windows

Building **mg\_php** for Windows from source is more involved.  Having successfully built PHP from source, create a directory called **mg\_php** underneath the **/ext** directory of PHP distribution.  Typically, this will be something like:

       C:\php-sdk\phpdev\vc15\x64\php-8.2.1-src\ext\mg_php\

Now, copy the contents of the **mg\_php** **/src** directory to that location.  The **...\ext\mg\_php** will now contain the **mg\_php** source code together with the **config.w32** file.  Open the appropriate Visual Studio Developer command Window (i.e. x86 or x64) and change directory to the root of the PHP source tree.  For example: 

       C:\php-sdk\phpdev\vc15\x64\php-8.2.1-src\

Set the essential variables for the build environment by invoking:

       C:\php-sdk\bin\phpsdk_setvars.bat

Invoke the following commands to build the **mg\_php.dll** extension:

       buildconf
       configure --disable-all --enable-cli --enable-mg_php --with-all-shared
       nmake
       nmake install


### Installing the DB Superserver

The DB Superserver is required for:

* Network based access to databases.

Two M routines need to be installed (%zmgsi and %zmgsis).  These can be found in the *Service Integration Gateway* (**mgsi**) GitHub source code repository ([https://github.com/chrisemunt/mgsi](https://github.com/chrisemunt/mgsi)).  Note that it is not necessary to install the whole *Service Integration Gateway*, just the two M routines held in that repository.

#### Installation for InterSystems Cache/IRIS

Log in to the %SYS Namespace and install the **zmgsi** routines held in **/isc/zmgsi\_isc.ro**.

       do $system.OBJ.Load("/isc/zmgsi_isc.ro","ck")

Change to your development Namespace and check the installation:

       do ^%zmgsi

       MGateway Ltd - Service Integration Gateway
       Version: 4.5; Revision 31 (18 November 2023)


#### Installation for YottaDB

The instructions given here assume a standard 'out of the box' installation of **YottaDB** (version 1.38) deployed in the following location:

       /usr/local/lib/yottadb/r138

The primary default location for routines:

       /root/.yottadb/r1.38_x86_64/r

Copy all the routines (i.e. all files with an 'm' extension) held in the GitHub **/yottadb** directory to:

       /root/.yottadb/r1.38_x86_64/r

Change directory to the following location and start a **YottaDB** command shell:

       cd /usr/local/lib/yottadb/r138
       ./ydb

Link all the **zmgsi** routines and check the installation:

       do ylink^%zmgsi

       do ^%zmgsi

       MGateway Ltd - Service Integration Gateway
       Version: 4.5; Revision 31 (18 November 2023)

Note that the version of **zmgsi** is successfully displayed.

Finally, add the following lines to the interface file (**zmgsi.ci** in the example used in the db.open() method).

       sqlemg: ydb_string_t * sqlemg^%zmgsis(I:ydb_string_t*, I:ydb_string_t *, I:ydb_string_t *)
       sqlrow: ydb_string_t * sqlrow^%zmgsis(I:ydb_string_t*, I:ydb_string_t *, I:ydb_string_t *)
       sqldel: ydb_string_t * sqldel^%zmgsis(I:ydb_string_t*, I:ydb_string_t *)
       ifc_zmgsis: ydb_string_t * ifc^%zmgsis(I:ydb_string_t*, I:ydb_string_t *, I:ydb_string_t*)

A copy of this file can be downloaded from the **/unix** directory of the  **mgsi** GitHub repository [here](https://github.com/chrisemunt/mgsi)


### Starting the DB Superserver

The default TCP server port for **zmgsi** is **7041**.  If you wish to use an alternative port then modify the following instructions accordingly.

* For InterSystems DB servers the concurrent TCP service should be started in the **%SYS** Namespace.

Start the DB Superserver using the following command:

       do start^%zmgsi(0) 

To use a server TCP port other than 7041, specify it in the start-up command (as opposed to using zero to indicate the default port of 7041).

* For YottaDB, as an alternative to starting the DB Superserver from the command prompt, Superserver processes can be started via the **xinetd** daemon.  Instructions for configuring this option can be found in the **mgsi** repository [here](https://github.com/chrisemunt/mgsi)

PHP code using the **mg\_php** functions will, by default, expect the database server to be listening on port **7041** of the local server (localhost).  However, **mg\_php** provides the functionality to modify these default settings at run-time.  It is not necessary for the PHP installation to reside on the same host as the database server.

### Resources used by the DB Superserver (%zmgsi)

The **zmgsi** server-side code will write to the following global:

* **^zmgsi**: The event Log. 


## <a name="phpconfig">PHP Configuration</a>

PHP should be configured to recognise the **mg\_php** extension.  The PHP configuration file (**php.ini**) is usually found in the following locations:

### UNIX

       /usr/local/lib/php.ini

Add the following line to the extensions section:

       extension=mg_php.so

Finally, install the **mg\_php.so** file in the PHP modules directory, which is usually:

       /usr/local/lib/php/extensions/[version_information]/

### Windows

       C:\Windows\php.ini

Add the following line to the extensions section:

       extension=mg_php.dll

Finally, install the **mg\_php.dll** file in the PHP modules directory, which is usually:

       C:\Windows\System32\


## <a name="connect">Connecting to the database</a>

Before invoking database functionality,the following simple script can be used to check that **mg\_php** is successfully installed.

       <?php
          print(m_ext_version());
       ?>

This should return something like:

       MGateway Ltd. - mg_php: PHP Gateway to M - Version 3.3.61

### Connecting the database via the network.

By default, **mg\_php** will connect to the server over TCP - the default parameters for which being the database listening locally on port **7041**. This can be modified using the following function.

       m_set_host(<netname>, <port>, <username>, <password>)

If this function is not called, the default server will be used (localhost listening on TCP port 7041).

Example:

       m_set_host("localhost", 7041, "", "");

### Connecting to the database via its API.

As an alternative to connecting to the database using TCP based connectivity, **mg\_php** provides the option of high-performance embedded access to a local installation of the database via its API.

#### InterSystems Caché or IRIS.

Use the following functions to bind to the database API.

       m_set_uci(<namespace>)
       m_bind_server_api(<dbtype>, <path>, <username>, <password>, <envvars>, <params>)

Where:

* namespace: Namespace.
* dbtype: Database type ("Cache", or "IRIS").
* path: Path to database manager directory.
* username: Database username.
* password: Database password.
* envvars: List of required environment variables.
* params: Reserved for future use.

Example:

       m_set_uci("USER");
       $result = m_bind_server_api("IRIS", "/usr/iris20221/mgr", "_SYSTEM", "SYS", "", "");

The bind function will return '1' for success and '0' for failure.

Before leaving your PHP application, it is good practice to gracefully release the binding to the database:

       m_release_server_api()

Example:

       m_release_server_api();

#### YottaDB

Use the following function to bind to the database API.

       m_bind_server_api(<dbtype>, <path>, <username>, <password>, <envvars>, <params>)

Where:

* dbtype: Database type ("YottaDB").
* path: Path to the YottaDB installation/library.
* username: Database username.
* password: Database password.
* envvars: List of required environment variables.
* params: Reserved for future use.

Example:

This example assumes that the YottaDB installation is in: **/usr/local/lib/yottadb/r138**. 
This is where the **libyottadb.so** library is found.
Also, in this directory, as indicated in the environment variables, the YottaDB routine interface file resides (**zmgsi.ci** in this example).  The interface file must contain the following lines:

       sqlemg: ydb_string_t * sqlemg^%zmgsis(I:ydb_string_t*, I:ydb_string_t *, I:ydb_string_t *)
       sqlrow: ydb_string_t * sqlrow^%zmgsis(I:ydb_string_t*, I:ydb_string_t *, I:ydb_string_t *)
       sqldel: ydb_string_t * sqldel^%zmgsis(I:ydb_string_t*, I:ydb_string_t *)
       ifc_zmgsis: ydb_string_t * ifc^%zmgsis(I:ydb_string_t*, I:ydb_string_t *, I:ydb_string_t*)

Moving on to the PHP code for binding to the YottaDB database.  Modify the values of these environment variables in accordance with your own YottaDB installation.  Note that each line is terminated with a linefeed character, with a double linefeed at the end of the list.

       $envvars = "";
       $envvars = $envvars . "ydb_dir=/root/.yottadb\n";
       $envvars = $envvars . "ydb_rel=r1.38_x86_64\n";
       $envvars = $envvars . "ydb_gbldir=/root/.yottadb/r1.38_x86_64/g/yottadb.gld\n";
       $envvars = $envvars . "ydb_routines=/root/.yottadb/r1.38_x86_64/o*(/root/.yottadb/r1.38_x86_64/r root/.yottadb/r) /usr/local/lib/yottadb/r138/libyottadbutil.so\n";
       $envvars = $envvars . "ydb_ci=/usr/local/lib/yottadb/r138/zmgsi.ci\n";
       $envvars = $envvars . "\n";

       $result = m_bind_server_api("YottaDB", "/usr/local/lib/yottadb/r138", "", "", envvars, "");

The bind function will return '1' for success and '0' for failure.

Before leaving your PHP application, it is good practice to gracefully release the binding to the database:

       m_release_server_api()

Example:

       m_release_server_api();


## <a name="dbcommands">Invocation of database commands</a>

Before invoking database functionality,the following simple script can be used to check that **mg\_php** is successfully installed.

       print(m_ext_version());

This should return something like:

       MGateway Ltd. - mg_php: PHP Gateway to M - Version 3.3.61

Now consider the following database script:

       Set ^Person(1)="Chris Munt"
       Set name=$Get(^Person(1))

Equivalent PHP code:

       m_set("^Person", 1, "Chris Munt");
       $name = m_get("^Person", 1);;


**mg\_php** provides functions to invoke all database commands and functions.


### Set a record (m\_set)

       result = m_set(<global>, <key>, <data>)
      
Example:

       $result = m_set("^Person", 1, "Chris Munt");

### Get a record (m\_get)

       result = m_get(<global>, <key>)
      
Example:

       $result = m_get("^Person", 1);

### Delete a record (m\_delete or m\_kill)

       result = m_delete(<dbhandle>, <global>, <key>)
      
Example:

       $result = m_delete("^Person", 1);


### Check whether a record is defined (m\_defined or m\_data)

       result = m_defined(<global>, <key>)
      
Example:

       $result = m_defined("^Person", 1);


### Parse a set of records (in order - m\_order)

       result = m_order(<global>, <key>)
      
Example:

       $key = m_order("^Person", "");
       while ($key != "") {
          $name = m_get("^MGWCust", $key);
          print("\n$key = $name");
          $key = m_order("^Person", $key);
       }


### Parse a set of records (in reverse order - m\_previous)

       result = m_previous(<global>, <key>)
      
Example:

       $key = m_previous("^Person", "");
       while ($key != "") {
          $name = m_get("^MGWCust", $key);
          print("\n$key = $name");
          $key = m_previous("^Person", $key);
       }


### Increment a global node (m\_increment)

       result = m_increment(<global>, <key>, <increment_value>)
      
Example:

       $result = m_increment("^Global", "counter", 1);


This will increment the value of global node ^Global("counter") by 1 and return the next value.

## <a name="dbfunctions">Invocation of database functions</a>

* Use **m\_function** or **m\_proc**.

       result = m_function(<function>, <parameters>)
      
Example:

M routine called 'math':

       add(a, b) ; Add two numbers together
                 quit (a+b)

PHP invocation:

       $result = m_function("add^math", 2, 3);


## <a name="tprocessing">Transaction Processing</a>

M DB Servers implement Transaction Processing by means of the methods described in this section.

### Start a Transaction

       result = m_tstart()

* On successful completion this method will return zero, or an error code on failure.

Example:

       $result = m_tstart()


### Determine the Transaction Level

       result = m_tlevel()

* Transactions can be nested and this method will return the level of nesting.  If no Transaction is active this method will return zero.  Otherwise a positive integer will be returned to represent the current depth of Transaction nesting.

Example:

       $tlevel = m_tlevel()


### Commit a Transaction

       result = m_tcommit()

* On successful completion this method will return zero, or an error code on failure.

Example:

       $result = m_tcommit()


### Rollback a Transaction

       result = m_trollback()

* On successful completion this method will return zero, or an error code on failure.

Example:

       $result = m_trollback()


## <a name="dbclasses">Direct access to InterSystems classes (IRIS and Cache)</a>

### Invocation of a ClassMethod (m\_classmethod)

       result = m_classmethod(<class_name>, <classmethod_name>, <parameters>)
      
Example (Encode a date to internal storage format):

       $result = m_classmethod("%Library.Date", "DisplayToLogical", "10/10/2019");


## <a name="license">License</a>

Copyright (c) 2018-2024 MGateway Ltd,
Surrey UK.                                                      
All rights reserved.
 
http://www.mgateway.com                                                  
Email: cmunt@mgateway.com
 
 
Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file except in compliance with the License. You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software distributed under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the License for the specific language governing permissions and limitations under the License.      

## <a name="relnotes">Release Notes</a>

### v3.0.1 (13 June 2019)

* Initial Experimental Pre-Release

### v3.1.56 (19 February 2020)

* Initial Release

### v3.1.56a (12 January 2021)

* Restructure and update the documentation.

### v3.2.57 (18 February 2021)

* Introduce support for M transaction processing: tstart, $tlevel, tcommit, trollback.
	* Available with DB Superserver v4 and later. 
* Introduce support for the M increment function.

### v3.2.58 (14 March 2021)

* Introduce support for YottaDB Transaction Processing over API based connectivity.
	* This functionality was previously only available over network-based connectivity to YottaDB.

### v3.3.59 (26 January 2023)

* Introduce support for PHP v8.0.x, v8.1.x and v8.2.x.

### v3.3.60 (26 March 2023)

* Properly terminate strings returned from the YottaDB API.

### v3.3.60a (23 June 2023)

* Documentation update.

### v3.3.61 (22 March 2024)

* Correct a fault in the **m\_proc\_byref()** and **m\_method\_byref()** functions.
	* It remains the case that the first two arguments to these functions are passed by value (DB Server name and M function name) but subsequent arguments are passed by reference.

### v3.3.62 (16 April 2024)

* Correct a fault in the management of DB Server connections in multi-process Apache configurations.
	* This fault led web requests failing with 'empty page' errors.


