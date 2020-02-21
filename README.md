# mg_php

A PHP Extension for InterSystems **Cache/IRIS** and **YottaDB**.

Chris Munt <cmunt@mgateway.com>  
17 February 2020, M/Gateway Developments Ltd [http://www.mgateway.com](http://www.mgateway.com)

* Current Release: Version: 3.1; Revision 56.
* [Release Notes](#RelNotes) can be found at the end of this document.

## Overview

**mg\_php** is an Open Source PHP extension developed for InterSystems **Cache/IRIS** and the **YottaDB** database.  It will also work with the **GT.M** database and other **M-like** databases.


## Pre-requisites

PHP installation:

       http://www.php.net/

InterSystems **Cache/IRIS** or **YottaDB** (or similar M database):

       https://www.intersystems.com/
       https://yottadb.com/

## Installing mg_php

There are three parts to **mg\_php** installation and configuration.

* The PHP extension (**mg\_php.so** or **mg\_php.dll**).
* The database (or server) side code: **zmgsi**
* A network configuration to bind the former two elements together.

### Building the mg_php extension

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

####UNIX:

Invoke the following commands from the /src directory (i.e. the directory containing **config.m4** file).

       phpize
       ./configure
       make
       make install

####Windows:

Building **mg\_php** for Windows from source is more involved.  Having successfully built PHP from source, create a directory called **mg\_php** underneath the **/ext** directory of PHP distribution.  Typically, this will be something like:

       C:\php-sdk\phpdev\vc15\x64\php-7.4.2-src\ext\mg_php\

Now, copy the contents of the **mg\_php** **/src** directory to that location.  The **...\ext\mg\_php** will now contain the **mg\_php** source code together with the **config.w32** file.  Open the appropriate Visual Studio Developer command Window (i.e. x86 or x64) and change directory to the root of the PHP source tree.  For example: 

       C:\php-sdk\phpdev\vc15\x64\php-7.4.2-src\

Set the essential variables for the build environment by invoking:

       C:\php-sdk\bin\phpsdk_setvars.bat

Invoke the following commands to build the **mg\_php.dll** extension:

       buildconf
       configure --disable-all --enable-cli --enable-mg_php --with-all-shared
       nmake
       nmake install


### InterSystems Cache/IRIS

Log in to the Manager UCI and install the **zmgsi** routines held in either **/m/zmgsi\_cache.xml** or **/m/zmgsi\_iris.xml** as appropriate.

       do $system.OBJ.Load("/m/zmgsi_cache.xml","ck")

Change to your development UCI and check the installation:

       do ^%zmgsi

       M/Gateway Developments Ltd - Service Integration Gateway
       Version: 3.2; Revision 5 (17 January 2020)

### YottaDB

The instructions given here assume a standard 'out of the box' installation of **YottaDB** deployed in the following location:

       /usr/local/lib/yottadb/r122

The primary default location for routines:

       /root/.yottadb/r1.22_x86_64/r

Copy all the routines (i.e. all files with an 'm' extension) held in the GitHub **/yottadb** directory to:

       /root/.yottadb/r1.22_x86_64/r

Change directory to the following location and start a **YottaDB** command shell:

       cd /usr/local/lib/yottadb/r122
       ./ydb

Link all the **zmgsi** routines and check the installation:

       do ylink^%zmgsi

       do ^%zmgsi

       M/Gateway Developments Ltd - Service Integration Gateway
       Version: 3.2; Revision 5 (17 January 2020)


Note that the version of **zmgsi** is successfully displayed.


## Setting up the network service

The default TCP server port for **zmgsi** is **7041**.  If you wish to use an alternative port then modify the following instructions accordingly.

PHP code using the **mg\_php** functions will, by default, expect the database server to be listening on port **7041** of the local server (localhost).  However, **mg\_php** provides the functionality to modify these default settings at run-time.  It is not necessary for the web server/PHP installation to reside on the same host as the database server.

### InterSystems Cache/IRIS

Start the Cache/IRIS-hosted concurrent TCP service in the Manager UCI:

       do start^%zmgsi(0) 

To use a server TCP port other than 7041, specify it in the start-up command (as opposed to using zero to indicate the default port of 7041).

### YottaDB

Network connectivity to **YottaDB** is managed via the **xinetd** service.  First create the following launch script (called **zmgsi\_ydb** here):

       /usr/local/lib/yottadb/r122/zmgsi_ydb

Content:

       #!/bin/bash
       cd /usr/local/lib/yottadb/r122
       export ydb_dir=/root/.yottadb
       export ydb_dist=/usr/local/lib/yottadb/r122
       export ydb_routines="/root/.yottadb/r1.22_x86_64/o*(/root/.yottadb/r1.22_x86_64/r /root/.yottadb/r) /usr/local/lib/yottadb/r122/libyottadbutil.so"
       export ydb_gbldir="/root/.yottadb/r1.22_x86_64/g/yottadb.gld"
       $ydb_dist/ydb -r xinetd^%zmgsi

Create the **xinetd** script (called **zmgsi\_xinetd** here): 

       /etc/xinetd.d/zmgsi_xinetd

Content:

       service zmgsi_xinetd
       {
            disable         = no
            type            = UNLISTED
            port            = 7041
            socket_type     = stream
            wait            = no
            user            = root
            server          = /usr/local/lib/yottadb/r122/zmgsi_ydb
       }

* Note: sample copies of **zmgsi\_xinetd** and **zmgsi\_ydb** are included in the **/unix** directory.

Edit the services file:

       /etc/services

Add the following line to this file:

       zmgsi_xinetd          7041/tcp                        # zmgsi

Finally restart the **xinetd** service:

       /etc/init.d/xinetd restart

## PHP configuration

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


## Using mg_php

Before invoking database functionality,the following simple script can be used to check that **mg\_php** is successfully installed.

       <?php
          print(m_ext_version());
       ?>

This should return something like:

       M/Gateway Developments Ltd. - mg_php: PHP Gateway to M - Version 3.1.56

### Connecting the database.

By default, **mg\_php** will connect to the server over TCP - the default parameters for which being the database listening locally on port **7041**. This can be modified using the following function.

       m_set_host(<netname>, <port>, <username>, <password>)

If this function is not called, the default server will be used (localhost listening on TCP port 7041).

Example:

       m_set_host("localhost", 7041, "", "");

#### Connecting to the database via its API.

As an alternative to connecting to the database using TCP based connectivity, **mg\_php** provides the option of high-performance embedded access to a local installation of the database via its API.

##### InterSystems Caché or IRIS.

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
       $result = m_bind_server_api("IRIS", "/usr/iris20191/mgr", "_SYSTEM", "SYS", "", "");

The bind function will return '1' for success and '0' for failure.

Before leaving your PHP application, it is good practice to gracefully release the binding to the database:

       m_release_server_api()

Example:

       m_release_server_api();

##### YottaDB

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

This example assumes that the YottaDB installation is in: **/usr/local/lib/yottadb/r122**. 
This is where the **libyottadb.so** library is found.
Also, in this directory, as indicated in the environment variables, the YottaDB routine interface file resides (**zmgsi.ci** in this example).  The interface file must contain the following line:

       ifc_zmgsis: ydb_char_t * ifc^%zmgsis(I:ydb_char_t*, I:ydb_char_t *, I:ydb_char_t*)

Moving on to the PHP code for binding to the YottaDB database.  Modify the values of these environment variables in accordance with your own YottaDB installation.  Note that each line is terminated with a linefeed character, with a double linefeed at the end of the list.

       $envvars = "";
       $envvars = $envvars . "ydb_dir=/root/.yottadb\n";
       $envvars = $envvars . "ydb_rel=r1.22_x86_64\n";
       $envvars = $envvars . "ydb_gbldir=/root/.yottadb/r1.22_x86_64/g/yottadb.gld\n";
       $envvars = $envvars . "ydb_routines=/root/.yottadb/r1.22_x86_64/o*(/root/.yottadb/r1.22_x86_64/r root/.yottadb/r) /usr/local/lib/yottadb/r122/libyottadbutil.so\n";
       $envvars = $envvars . "ydb_ci=/usr/local/lib/yottadb/r122/zmgsi.ci\n";
       $envvars = $envvars . "\n";

       $result = m_bind_server_api("YottaDB", "/usr/local/lib/yottadb/r122", "", "", envvars, "");

The bind function will return '1' for success and '0' for failure.

Before leaving your PHP application, it is good practice to gracefully release the binding to the database:

       m_release_server_api()

Example:

       m_release_server_api();



## Invoking database commands from PHP script

Before invoking database functionality,the following simple script can be used to check that **mg\_php** is successfully installed.

       print(m_ext_version());

This should return something like:

       M/Gateway Developments Ltd. - mg_php: PHP Gateway to M - Version 3.1.56

Now consider the following database script:

       Set ^Person(1)="Chris Munt"
       Set name=$Get(^Person(1))

Equivalent PHP code:

       m_set("^Person", 1, "Chris Munt");
       $name = m_get("^Person", 1);;


**mg\_php** provides functions to invoke all database commands and functions.


#### Set a record (m\_set)

       result = m_set(<global>, <key>, <data>)
      
Example:

       $result = m_set("^Person", 1, "Chris Munt");

#### Get a record (m\_get)

       result = m_get(<global>, <key>)
      
Example:

       $result = m_get("^Person", 1);

#### Delete a record (m\_delete or m\_kill)

       result = m_delete(<dbhandle>, <global>, <key>)
      
Example:

       $result = m_delete("^Person", 1);


#### Check whether a record is defined (m\_defined or m\_data)

       result = m_defined(<global>, <key>)
      
Example:

       $result = m_defined("^Person", 1);


#### Parse a set of records (in order - m\_order)

       result = m_order(<global>, <key>)
      
Example:

       $key = m_order("^Person", "");
       while ($key != "") {
          $name = m_get("^MGWCust", $key);
          print("\n$key = $name");
          $key = m_order("^Person", $key);
       }


#### Parse a set of records (in reverse order - m\_previous)

       result = m_previous(<global>, <key>)
      
Example:

       $key = m_previous("^Person", "");
       while ($key != "") {
          $name = m_get("^MGWCust", $key);
          print("\n$key = $name");
          $key = m_previous("^Person", $key);
       }

## Invocation of database functions (m\_function or m\_proc)

       result = m_function(<function>, <parameters>)
      
Example:

M routine called 'math':

       add(a, b) ; Add two numbers together
                 quit (a+b)

PHP invocation:

       $result = m_function("add^math", 2, 3);


## Direct access to InterSystems classes (IRIS and Cache)

#### Invocation of a ClassMethod (m\_classmethod)

       result = m_classmethod(<class_name>, <classmethod_name>, <parameters>)
      
Example (Encode a date to internal storage format):

       $result = m_classmethod("%Library.Date", "DisplayToLogical", "10/10/2019");

## Resources used by zmgsi

The **zmgsi** server-side code will write to the following global:

* **^zmgsi**: The event Log. 

## License

Copyright (c) 2018-2020 M/Gateway Developments Ltd,
Surrey UK.                                                      
All rights reserved.
 
http://www.mgateway.com                                                  
Email: cmunt@mgateway.com
 
 
Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file except in compliance with the License. You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software distributed under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the License for the specific language governing permissions and limitations under the License.      

## <a name="RelNotes"></a>Release Notes

### v3.0.1 (13 June 2019)

* Initial Experimental Pre-Release

3.0; Revision 1 (13 June 2019)

### v3.1.56 (19 February 2020)

* Initial Release
