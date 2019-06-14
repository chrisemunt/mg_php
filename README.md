# mg_php

A PHP Extension for InterSystems **Cache/IRIS** and **YottaDB**.

Chris Munt <cmunt@mgateway.com>  
13 June 2019, M/Gateway Developments Ltd [http://www.mgateway.com](http://www.mgateway.com)

* Current Release: Version: 3.0; Revision 1 (13 June 2019)

## Overview

**mg_php** is an Open Source PHP extension developed for InterSystems **Cache/IRIS** and the **YottaDB** database.  It will also work with the **GT.M** database and other **M-like** databases.


## Pre-requisites

PHP installation:

       http://www.php.net/

InterSystems **Cache/IRIS** or **YottaDB** (or similar M database):

       https://www.intersystems.com/
       https://yottadb.com/

## Installing mg_php

There are three parts to **mg_php** installation and configuration.

* The PHP extension (**mg_php.so** or **mg_php.dll**).
* The database (or server) side code: **zmgsi**
* A network configuration to bind the former two elements together.

### InterSystems Cache/IRIS

Log in to the Manager UCI and, using the %RI utility (or similar) load the **zmgsi** routines held in **/m/zmgsi.ro**.  Change to your development UCI and check the installation:

       do ^%zmgsi

       M/Gateway Developments Ltd - Service Integration Gateway
       Version: 3.0; Revision 1 (13 June 2019)

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
       Version: 3.0; Revision 1 (13 June 2019)


Note that the version of **zmgsi** is successfully displayed.


## Setting up the network service

The default TCP server port for **zmgsi** is **7041**.  If you wish to use an alternative port then modify the following instructions accordingly.

PHP code using the **mg_php** functions will, by default, expect the database server to be listening on port **7041** of the local server (localhost).  However, **mg_php** provides the functionality to modify these default settings at run-time.  It is not necessary for the web server/PHP installation to reside on the same host as the database server.

### InterSystems Cache/IRIS

Start the Cache/IRIS-hosted concurrent TCP service in the Manager UCI:

       do start^%zmgsi(0) 

To use a server TCP port other than 7041, specify it in the start-up command (as opposed to using zero to indicate the default port of 7041).

### YottaDB

Network connectivity to **YottaDB** is managed via the **xinetd** service.  First create the following launch script (called **zmgsi_ydb** here):

       /usr/local/lib/yottadb/r122/zmgsi_ydb

Content:

       #!/bin/bash
       cd /usr/local/lib/yottadb/r122
       export ydb_dir=/root/.yottadb
       export ydb_dist=/usr/local/lib/yottadb/r122
       export ydb_routines="/root/.yottadb/r1.22_x86_64/o*(/root/.yottadb/r1.22_x86_64/r /root/.yottadb/r) /usr/local/lib/yottadb/r122/libyottadbutil.so"
       export ydb_gbldir="/root/.yottadb/r1.22_x86_64/g/yottadb.gld"
       $ydb_dist/ydb -r xinetd^%zmgsi

Create the **xinetd** script (called **zmgsi_xinetd** here): 

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

* Note: sample copies of **zmgsi_xinetd** and **zmgsi_ydb** are included in the **/unix** directory.

Edit the services file:

       /etc/services

Add the following line to this file:

       zmgsi_xinetd          7041/tcp                        # zmgsi

Finally restart the **xinetd** service:

       /etc/init.d/xinetd restart

## PHP configuration

PHP should be configured to recognise the **mg_php** extension.  The PHP configuration file (**php.ini**) is usually found in the following locations:

####UNIX:

       /usr/local/lib/php.ini

Add the following line to the extensions section:

       extension=mg_php.so

Finally, install the **mg_php.so** file in the PHP modules directory, which is usually:

       /usr/local/lib/php/extensions/[version_information]/

####Windows:

       C:\Windows\php.ini

Add the following line to the extensions section:

       extension=mg_php.dll

Finally, install the **mg_php.dll** file in the PHP modules directory, which is usually:

       C:\Windows\System32\


## Invoking database commands from PHP script

Before invoking database functionality,the following simple script can be used to check that **mg_php** is successfully installed.

       <?php
          print(m_ext_version());
       ?>

This should return something like:

       M/Gateway Developments Ltd. - mg_php: PHP Gateway to M - Version 3.0.1

Now consider the following database script:

       Set ^Person(1)="Chris Munt"
       Set name=$Get(^Person(1))

Equivalent PHP code:

       <?php
          m_set("^Person", 1, "Chris Munt");
          $name = m_get("^Person", 1);
          print("<P>Name ... $name<br>");
       ?>

**mg_php** provides functions to invoke all database commands and functions.

## Resources used by zmgsi

The **zmgsi** server-side code will write to the following global:

* **^zmgsi**: The event Log. 

## License

Copyright (c) 2018-2019 M/Gateway Developments Ltd,
Surrey UK.                                                      
All rights reserved.
 
http://www.mgateway.com                                                  
Email: cmunt@mgateway.com
 
 
Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file except in compliance with the License. You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software distributed under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the License for the specific language governing permissions and limitations under the License.      

