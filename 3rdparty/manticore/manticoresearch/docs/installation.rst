Installation
============


Installing Manticore packages on Debian and Ubuntu
--------------------------------------------------
Supported releases:

*  Debian

	* 8.0 (jessie)
	* 9.0 (stretch)
	* 10.0 (buster)
	
*  Ubuntu

	* 14.04 LTS (trusty)
	* 16.04 LTS (xenial)
	* 18.04 LTS (bionic)
	
Supported platforms:

* x86
* x86_64

You can install Manticore with command:

.. code-block:: bash

	$ wget https://github.com/manticoresoftware/manticoresearch/releases/download/3.4.0/manticore_3.4.0-200326-0686d9f-release.jessie_amd64-bin.deb
	$ sudo dpkg -i manticore_3.4.0-200326-0686d9f-release.jessie_amd64-bin.deb

Manticore package depends on zlib and ssl libraries, nothing else is strictly required.
However if you plan to use 'indexer' tool to create indexes from different sources,
you'll need to install appropriate client libraries.
To know what exactly libraries, run `indexer` tool from Manticore and look at the top of it's output:

.. code-block:: bash

	$ indexer
	Manticore 3.4.0 0686d9f@200326 release
	Copyright (c) 2001-2016, Andrew Aksyonoff
	Copyright (c) 2008-2016, Sphinx Technologies Inc (http://sphinxsearch.com)
	Copyright (c) 2017-2020, Manticore Software LTD (http://manticoresearch.com)

	Built by gcc/clang v 6.3.0,

	Built on Linux runner-72989761-project-3858465-concurrent-0 4.19.78-coreos #1 SMP Mon Mar 30 22:56:39 -00 2020 x86_64 GNU/Linux

	Configured by CMake with these definitions: -DCMAKE_BUILD_TYPE=RelWithDebInfo -DDISTR_BUILD=stretch -DUSE_SSL=ON -DDL_UNIXODBC=1 -DUNIXODBC_LIB=libodbc.so.2 -DDL_EXPAT=1 -DEXPAT_LIB=libexpat.so.1 -DUSE_LIBICONV=1 -DDL_MYSQL=1 -DMYSQL_LIB=libmariadbclient.so.18 -DDL_PGSQL=1 -DPGSQL_LIB=libpq.so.5 -DLOCALDATADIR=/var/data -DFULL_SHARE_DIR=/usr/share/manticore -DUSE_ICU=1 -DUSE_BISON=ON -DUSE_FLEX=ON -DUSE_SYSLOG=1 -DWITH_EXPAT=1 -DWITH_ICONV=ON -DWITH_MYSQL=1 -DWITH_ODBC=ON -DWITH_PGSQL=1 -DWITH_RE2=1 -DWITH_STEMMER=1 -DWITH_ZLIB=ON -DGALERA_SOVERSION=31 -DSYSCONFDIR=/etc/manticoresearch

Here you can see mentions of `libodbc.so.2`, `libexpat.so.1`, `libmariadbclient.so.18`, and `libpq.so.5`.

Below is the reference table with list of all client libraries for different debian/ubuntu distributions:


+---------+------------------------+------------+---------------+--------------+
| Distr   | Mysql                  | PostgresQL | Xmlpipe       | Unixodbc     |
+=========+========================+============+===============+==============+
| trusty  | libmysqlclient.so.18   | libpq.so.5 | libexpat.so.1 | libodbc.so.1 |
+---------+------------------------+------------+---------------+--------------+
| xenial  | libmysqlclient.so.20   | libpq.so.5 | libexpat.so.1 | libodbc.so.2 |
+---------+------------------------+------------+---------------+--------------+
| bionic  | libmysqlclient.so.20   | libpq.so.5 | libexpat.so.1 | libodbc.so.2 |
+---------+------------------------+------------+---------------+--------------+
| wheezy  | libmysqlclient.so.18   | libpq.so.5 | libexpat.so.1 | libodbc.so.1 |
+---------+------------------------+------------+---------------+--------------+
| jessie  | libmysqlclient.so.18   | libpq.so.5 | libexpat.so.1 | libodbc.so.2 |
+---------+------------------------+------------+---------------+--------------+
| stretch | libmariadbclient.so.18 | libpq.so.5 | libexpat.so.1 | libodbc.so.2 |
+---------+------------------------+------------+---------------+--------------+
| buster  | libmariadb.so.3        | libpq.so.5 | libexpat.so.1 | libodbc.so.2 |
+---------+------------------------+------------+---------------+--------------+


To find the packages which provide the libraries you can use, for example ``apt-file``:

.. code-block:: bash

	$ apt-file find libmysqlclient.so.20
	libmysqlclient20: /usr/lib/x86_64-linux-gnu/libmysqlclient.so.20
	libmysqlclient20: /usr/lib/x86_64-linux-gnu/libmysqlclient.so.20.2.0
	libmysqlclient20: /usr/lib/x86_64-linux-gnu/libmysqlclient.so.20.3.6

Note, that you need only libs for types of sources you're going to use. So if you plan to make indexes only
from mysql source, then install only lib for mysql client (in case above - ``libmysqlclient20``).

Finally install necessary packages:

.. code-block:: bash

	$ sudo apt-get install libmysqlclient20 libodbc1 libpq5 libexpat1

If you aren't going to use ``indexer`` tool at all, you don't need find and install any libraries.

Manticore can optionally use the ICU library for chinese :ref:`morphology <morphology>` processing. 
The appropriate ICU library used at compiling must be installed for using the ICU processor.
Below is the reference table with the ICU library for different debian/ubuntu distributions:

+---------+------------------------+
| Distr   | ICU library            |
+=========+========================+
| trusty  | libicudata.so.52       | 
+---------+------------------------+
| xenial  | libicudata.so.55       |
+---------+------------------------+
| bionic  | libicudata.so.60       |
+---------+------------------------+
| jessie  | libicudata.so.52       |
+---------+------------------------+
| stretch | libicudata.so.57       |
+---------+------------------------+
| buster  | libicudata.so.63       |
+---------+------------------------+

For example on Debian Stretch ``libicu57`` needs to be additionally installed for ICU support:

.. code-block:: bash

	$ sudo apt-get install libicu57


After preparing configuration file (see :ref:`Quick tour <quick_usage_tour>`), you can start searchd daemon:

.. code-block:: bash

	$ systemctl start manticore



Installing Manticore packages on RedHat and CentOS
--------------------------------------------------

Supported releases:

* CentOS 6 and RHEL 6
* CentOS 7 and RHEL 7
* CentOS 8 and RHEL 8

Supported platforms:

* x86
* x86_64

Manticore package depends on zlib and ssl libraries, nothing else is strictly required.
However if you plan to use 'indexer' tool to create indexes from different sources,
you'll need to install appropriate client libraries. Use yum to download and install these dependencies:

.. code-block:: bash

	$ yum install mysql-libs postgresql-libs expat unixODBC

Note, that you need only libs for types of sources you're going to use. So if you plan to make indexes only
from mysql source, then installing 'mysql-libs' will be enough.
If you don't going to use 'indexer' tool at all, you don't need to install these packages.

For ICU support, additional ``libicu`` package needs to be installed.

.. code-block:: bash

	$ yum install libicu


Installing Manticore Search from Manticore **yum** repository
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Install Manticore yum repository by running as root user or with sudo the following command:


.. code-block:: bash

	$ yum install https://repo.manticoresearch.com/manticore-repo.noarch.rpm

Install Manticore Search by running:

.. code-block:: bash
    
	$ yum install manticore


Install Manticore Search using downloaded rpm packages
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Download RedHat RPM from Manticore website and install it:

.. code-block:: bash

	$ wget https://github.com/manticoresoftware/manticoresearch/releases/download/3.4.0/manticore-3.4.0_200327.b212975-1.el7.centos.x86_64.rpm
	$ rpm -Uhv manticore-3.4.0_200327.b212975-1.el7.centos.x86_64.rpm

Starting Manticore Search
~~~~~~~~~~~~~~~~~~~~~~~~~

After preparing configuration file (see :ref:`Quick tour <quick_usage_tour>`), you can start searchd daemon:

.. code-block:: bash

	$ systemctl start manticore
	

Installing Manticore on Windows
-------------------------------

To install on Windows, you need to download the zip package and unpack it first in a  folder.

In the following example we'll consider folder ``C:\Manticore`` where we unpack the zip content.

.. code-block:: bash
	
	cd C:\Manticore
	unzip manticore-3.4.0-200326-0686d9f0-release-x64-bin.zip


The zip comes with a sample configurations ``manticore.conf.in``.

The configuration contains a ``@CONFIGDIR@`` string which needs to be replaced. The ``@CONFIGDIR@`` is the root directory of ``data`` and ``log`` folders (first is used as location for indexes, second for logs).
The zip package comes with these folders, so they will be available at the location where you unzipped the package. If you want to use a different location, the two folders must be created there.

Install the ``searchd`` system as a Windows service:

.. code-block:: bat

	C:\Manticore\bin> C:\Manticore\bin\searchd --install --config C:\Manticore\manticore.conf.in --servicename Manticore


Make sure to use the full path of the configuration file, otherwise searchd.exe will not be able to know the location of it when it's started as service.

After installation, the service can be started from the Services snap-in of the Microsoft Management Console.

Once started you can access Manticore using the mysql cli:

.. code-block:: bat

	C:\path\to\mysql> mysql -P9306 -h127.0.0.1

(note that in most example, we use ``-h0``, on Windows you need to use ``localhost`` or ``127.0.0.1`` for the local host.)


Installing Manticore on MacOS
-----------------------------

On MacOS Manticore can be installed in 2 easy way:

1. Use the official tar containing binary executables. Download it from the website and unpack it to a folder:


.. code-block:: bash
	
	$ mkdir manticore
	$ tar -zxvf manticore-3.4.0-200326-0686d9f0-release-osx10.14.4-x86_64-bin.tar.gz -C manticore
	$ cd manticore
	$ bin/searchd  -c manticore.conf

The manticore.conf is located in the root folder.
	
2. Use HomeBrew package manager

.. code-block:: bash
	
	$ brew install manticoresearch
	
Start Manticore as a brew service:

.. code-block:: bash
	
	$ brew services start  manticoresearch
	
	
.. _upgrade_from_sphinx:

Upgrading from Sphinx Search
----------------------------

Manticore Search 2.x maintains  compatibility with  Sphinx Search 2.x  and can load existing indexes created with Sphinx Search.
In most cases, upgrading is just a matter of replacing the binaries.

Manticore Search 3.x breaks compatibility with both Sphinx Search 2.x and Manticore Search 2.x indexes. In this case, indexes must be either remade or converted with the provided index converter tool.
For more information check :doc:`getting-started/migrate_from_manticore2`.

In case of Linux distributions, Manticore Search switched the configuration location from  ``/etc/sphinxsearch/sphinx.conf`` ``/etc/manticoresearch/manticore.conf``.

Service name has changed from ``sphinx``/``sphinxsearch`` to ``manticore`` and will run under ``manticore`` user ( Spinx was using ``sphinx`` or ``sphinxsearch``). It also uses a different folder for the PID file.

Default used folders are ``/var/lib/manticore``, ``/var/log/manticore``, ``/var/run/manticore``.
Existing file paths can still be used, but permissions should be updated for data,log, run folders.
If you  use other folders (for data, wordforms files etc.) the ownership must be also switched to ``manticore`` user.
The ``pid_file`` location should be changed to match the manticore.service  to ``/var/run/manticore/searchd.pid``. 

If you want to use the Manticore folder instead, the index files needs to be moved to the new data folder (``/var/lib/manticore``) and permissions to be changed to ``manticore`` user.

	
.. _running_from_docker:

Running Manticore Search in a Docker Container
----------------------------------------------

Docker images of Manticore Search are hosted publicly on Docker Hub at https://hub.docker.com/r/manticoresearch/manticore/.

For more information about using Docker, see the `Docker Docs <https://docs.docker.com/>`__.

The searchd daemon runs in nodetach mode inside the container under **manticore** user. Default configuration includes a simple Real-Time index and listens on the default ports (9306 for SphinxQL, 9312 for SphinxAPI, 9308 for HTTP  and 9312-9325 for replication).

The image uses currently the Manticore binaries from the Debian Stretch package.

Starting a Manticore Search instance in a container
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

To start a container running the latest release of Manticore Search run:

.. code-block:: bash
   
   docker run --name manticore -p 9306:9306 -d manticoresearch/manticore
   
Operations with utility tools over running daemon can be made with `docker exec` command.
Please note that any ``indexer`` command must run under **manticore**  user, otherwise ``searchd`` won't be able to rotate the files:
   
.. code-block:: bash
   
   docker exec -it manticore gosu manticore indexer --all --rotate
   
To stop the Manticore Search container you can simply do:

.. code-block:: bash
   
   docker stop manticore

or (managed stop with no hard-killing):

.. code-block:: bash

   docker exec -it manticore gosu manticore searchd --stopwait
	
Please note that any indexed data or configuration change made is lost if the container is stopped. For persistence, you need to mount the configuration and data folders.

Mounting points 
~~~~~~~~~~~~~~~

The configuration folder inside the image is the usual `/etc/manticoresearch`. 
Index files are located at `/var/lib/manticore/data` and logs at `/var/log/manticore`.  For persistence, mount these points to your local folders.

.. code-block:: bash
   
   docker run --name manticore -v /path/to/config/:/etc/manticoresearch/ -v /path/to/data/:/var/lib/manticore/data -v /path/to/logs/:/var/log/manticore -p 9306:9306 -d manticoresearch/manticore
   

   
.. _compiling_from_source:

Compiling Manticore from source
-------------------------------

.. _Required tools:

Required tools
~~~~~~~~~~~~~~

* a working compiler

	* on Linux - GNU gcc (4.7.2 and above) or clang can be used
	* on Windows - Microsoft Visual Studio 2015 and above (community edition is enough)
	* on Mac OS - XCode

* cmake - used on all plaftorms (version 3.13 or above)

Required libraries/packages on Linux
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

* Development version of **ssl** lib. Usually comes in package named like **libssl-dev** or **openssl-devel**.
* Development version of **boost**. On debian packages **libboost-system-dev** and **libboost-program-options-dev** are enough; on redhat it is **boost-devel**.


Optional dependencies
~~~~~~~~~~~~~~~~~~~~~
* git, flex, bison -  needed if the sources are from cloned repository and not the source tarball
* development version of MySQL client for  MySQL source driver
* development version of unixODBC for the unixODBC source driver
* development version of libPQ for the PostgreSQL source driver
* development version of libexpat for the XMLpipe source driver
* RE2 (bundled in the source tarball) for :ref:`regexp_filter` feature
* development version of libicudata for ICU chinese morphology processor
* lib stemmer (bundled in the source tarball ) for additional language stemmers 

General building options
~~~~~~~~~~~~~~~~~~~~~~~~

For compiling latest version of Manticore, recommended is checkout the latest code from the github repositiory.
Alternative, for compiling a certain version, you can either checked that version from github or use it's respective source tarball.
In last case avoid to use automatic tarballs from github (named there as 'Source code'), but use provided files as **manticore-3.4.0-200326-0686d9f-release.tar.gz**.
When building from git clone you need packages **git**, **flex**, **bison**. When building from tarball they are not necessary. This requirement
may be essential to build on Windows.

.. code-block:: bash

   $ git clone https://github.com/manticoresoftware/manticore.git

.. code-block:: bash

   $ wget https://github.com/manticoresoftware/manticoresearch/releases/download/3.4.0/manticore-3.4.0-200326-0686d9f-release.tar.gz
   $ tar zcvf manticore-3.4.0-200326-0686d9f-release.tar.gz

Next step is to configure the building with cmake. Available list of configuration options:


* ``CMAKE_BUILD_TYPE`` -  can be Debug , Release , MinSizeRel and RelWithDebInfo (default).
* ``SPLIT_SYMBOLS`` (bool) - specify whenever to create separate files with debugging symbols. In the default build type,RelWithDebInfo, the binaries include the debug symbols. With this option specified, the binaries will be stripped of the debug symbols , which will be put in separate files
* ``USE_BISON``, ``USE_FLEX`` (bool)  - enabled by default, specifies whenever to enable bison and flex tools
* ``LIBS_BUNDLE`` - filepath to a folder with different libraries. This is mostly relevant for Windows building
* ``WITH_STEMMER`` (bool) - specifies if the build should include the libstemmer library. The library is searched in several places, starting with 

	* libstemmer_c folder in the source directory
	* common system path. Please note that in this case, the linking is dynamic and libstemmer should be available system-wide on the installed systems
	* libstemmer_c.tgz in  ``LIBS_BUNDLE`` folder.
	* download from snowball project website (https://snowballstem.org/dist/libstemmer_c.tgz). This is done by cmake and no additional tool is required.
	* NOTE: if you have libstemmer in the system, but still want to use static version, say, to build a binary for a system without such lib, provide ``WITH_STEMMER_FORCE_STATIC=1`` in advance.
	
* ``WITH_RE2`` (bool) - specifies if the build should include the RE2 library. The library can be taken from the following locations:

	* in the folder specified by ``WITH_RE2_ROOT`` parameters
	* in libre2 folder of the Manticore sources
	* system wide search, while first looking for headers specified by ``WITH_RE2_INCLUDES`` folder and the lib files in ``WITH_RE2_LIBS`` folder
	* check presence of master.zip in the ``LIBS_BUNDLE`` folder 
	* Download from https://github.com/manticoresoftware/re2/archive/master.zip
	* NOTE: if you have RE2 in the system, but still want to use static version, say, to build a binary for a system without such lib, provide ``WITH_RE2_FORCE_STATIC=1`` in advance.
	
* ``WITH_EXPAT`` (bool)	 enabled compiling with libexpat, used XMLpipe source driver
* ``WITH_MYSQL`` (bool)	 enabled compiling with MySQL client library, used by MySQL source driver. Additional parameters ``WITH_MYSQL_ROOT``, ``WITH_MYSQL_LIBS`` and ``WITH_MYSQL_INCLUDES`` can be used for custom MySQL files
* ``WITH_ODBC`` (bool)	 enabled compiling with ODBC client library, used by ODBC source driver
* ``WITH_PGSQL`` (bool)	 enabled compiling with PostgreSQL client library, used by PostgreSQL source driver
* ``WITH_ICU`` (bool)  enabled compiling with ICU library support, used by morphology processor
* ``DISTR_BUILD``  -  in case the target is packaging, it specifies the target operating system. Supported values are: `rhel6`, `rhel7`, `rhel8`, `wheezy`, `jessie`, `stretch`, `buster`, `trusty`, `xenial`, `bionic`, `macos`, `default`.

Compiling on Linux systems
~~~~~~~~~~~~~~~~~~~~~~~~~~


To install all dependencies on Debian/Ubuntu:

.. code-block:: bash

   $ apt-get install build-essential cmake unixodbc-dev libpq-dev libexpat-dev libmysqlclient-dev libicu-dev libssl-dev libboost-system-dev libboost-program-options-dev git flex bison

Note: on Debian 9 (stretch) package ``libmysqlclient-dev`` is absent. Use ``default-libmysqlclient-dev`` there instead.

To install all dependencies on CentOS/RHEL:

.. code-block:: bash

   $ yum install gcc gcc-c++ make cmake mysql-devel expat-devel postgresql-devel unixODBC-devel libicu-devel openssl-devel boost-devel rpm-build systemd-units  git flex bison 

(git, flex, bison doesn't necessary if you build from tarball)

RHEL/CentOS 6  ship with a old version of the gcc compiler, which doesn't support `-std=c++11` flag, for compiling use `devtools` repository:

.. code-block:: bash

   $ wget http://people.centos.org/tru/devtools-2/devtools-2.repo -O /etc/yum.repos.d/devtools-2.repo
   $ yum upgrade -y
   $ yum install -y devtoolset-2-gcc devtoolset-2-binutils devtoolset-2-gcc-c++
   $ export PATH=/opt/rh/devtoolset-2/root/usr/bin:$PATH

Manticore uses **cmake** for building. We recommend to use a folder outside the sources to keep them clean.

.. code-block:: bash

   $ mkdir build
   $ cd build
   $ cmake3 -D WITH_MYSQL=1 -DWITH_RE2=1 ../manticore

or if we use sources from tarball:

.. code-block:: bash

   $ cmake3 -D WITH_MYSQL=1 -DWITH_RE2=1 ../manticore-3.4.0-200326-0686d9f-release

To simply compile:

.. code-block:: bash

   $ make -j4


This will create the binary files, however we want to either install Manticore or more convenient to create a package.
To install just do 

.. code-block:: bash

   $ make -j4 install

For packaging use ``package``

.. code-block:: bash

   $ make -j4 package


By default, if no operating system was targeted, ``package`` will create only a zip with the binaries.
If, for example, we want to create a deb package for Debian Jessie, we need to specify to cmake the ``DISTR_BUILD`` parameter:

.. code-block:: bash

   $ cmake3 -DDISTR_BUILD=jessie ../manticore
   $ make -j4 package	   

This will create 2 deb packages, a manticore-x.x.x-bin.deb and a manticore-x.x.x-dbg.deb which contains the version with debug symbols.
Another possible target is ``tarball`` , which create a tar.gz file from the sources.


Compiling on Windows
~~~~~~~~~~~~~~~~~~~~
For building on Windows you need:

* Visual Studio
* Cmake for Windows
* Expat, MySQL and PostgreSQL in bundle directory.

If you build from git clone, you also need to provide `git`, `flex`, `bison` tools. They may be fond in `cygwin` framework.
When building from tarball these tools are not necessary.

For a simple building on x64:

.. code-block:: bat

   C:\build>"%PROGRAMW6432%\CMake\bin\cmake.exe" -G "Visual Studio 14 Win64" -DLIBS_BUNDLE="C:\bundle" "C:\manticore"
   C:\build>"%PROGRAMW6432%\CMake\bin\cmake.exe" -DWITH_PGSQL=1 -DWITH_RE2=1 -DWITH_STEMMER=1 .
   C:\build>"%PROGRAMW6432%\CMake\bin\cmake.exe" --build . --target package --config RelWithDebInfo


Compiling on FreeBSD
~~~~~~~~~~~~~~~~~~~~

.. warning::
   Support for FreeBSD is limited and successful compiling is not guaranteed. 
   We recommend checking the issue tracker for unresolved issues on this platform before trying to compile latest versions.

FreeBSD uses clang instead of gcc as system compiler and it's installed by default.

First install required packages:

.. code-block:: bash

   $ pkg install cmake bison flex


To compile a version without optional dependencies:

.. code-block:: bash

   $ cmake -DUSE_GALERA=0 -DWITH_MYSQL=0 -DDISABLE_TESTING=1 ../manticoresearch/
   $ make 

With the exception of Galera, the rest of optional dependencies can be installed:

.. code-block:: bash

   $ pkg install mariadb103-client postgresql-libpqxx unixODBC icu expat
   
(you can replace ``mariadb103-client`` with MySQL client package of your choice)

Building with all optional features and installation system-wide:

.. code-block:: bash

   $ cmake -DUSE_GALERA=0 -DWITH_PGSQL=1 -DDISABLE_TESTING=1 -DCMAKE_INSTALL_PREFIX=/ -DCMAKE_INSTALL_LOCALSTATEDIR=/var ../manticoresearch/
   $ make
   $ make install
   


Recompilation (update)
~~~~~~~~~~~~~~~~~~~~~~

If you didn't change path for sources and build, just move to you build folder and run:

.. code-block:: bash

   cmake3 .
   make clean
   make

If by any reason it doesn't work, you can delete file ``CMakeCache.txt`` located in build folder.
After this step you have to run cmake again, pointing to source folder and configuring the options.

If it also doesn't help, just wipe out your build folder and begin clean :ref:`compiling from sources <compiling_from_source>`

.. _quick_usage_tour:

Quick Manticore usage tour
--------------------------
We are going to use SphinxQL protocol as it's the current recommended way and it's also easy to play with. First we connect to Manticore with the normal MySQL client:

.. code-block:: bash

    $ mysql -h0 -P9306

First we need to create an index, then we add several documents to it:
	
.. code-block:: bash

  mysql> CREATE TABLE testrt (title TEXT, content TEXT, gid INT);
   
  mysql> INSERT INTO testrt VALUES(1,'List of HP business laptops','Elitebook Probook',10);
  Query OK, 1 row affected (0.00 sec)

  mysql> INSERT INTO testrt VALUES(2,'List of Dell business laptops','Latitude Precision Vostro',10);
  Query OK, 1 row affected (0.00 sec)

  mysql> INSERT INTO testrt VALUES(3,'List of Dell gaming laptops','Inspirion Alienware',20);
  Query OK, 1 row affected (0.00 sec)
  
  mysql> INSERT INTO testrt VALUES(4,'Lenovo laptops list','Yoga IdeaPad',30);
  Query OK, 1 row affected (0.01 sec)

  mysql> INSERT INTO testrt VALUES(5,'List of ASUS ultrabooks and laptops','Zenbook Vivobook',30);
  Query OK, 1 row affected (0.01 sec)


.. code-block:: mysql

   mysql>  SELECT * FROM testrt WHERE MATCH('list of laptops');
   +------+------+-------------------------------------+---------------------------+
   | id   | gid  | title                               | content                   |
   +------+------+-------------------------------------+---------------------------+
   |    1 |   10 | List of HP business laptops         | Elitebook Probook         |
   |    2 |   10 | List of Dell business laptops       | Latitude Precision Vostro |
   |    3 |   20 | List of Dell gaming laptops         | Inspirion Alienware       |
   |    5 |   30 | List of ASUS ultrabooks and laptops | Zenbook Vivobook          |
   +------+------+-------------------------------------+---------------------------+
   4 rows in set (0.00 sec)
