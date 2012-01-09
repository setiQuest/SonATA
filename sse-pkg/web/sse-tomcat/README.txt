################################################################################
#
# File:    README.txt
# Project: OpenSonATA
# Authors: The OpenSonATA code is the result of many programmers
#          over many years
#
# Copyright 2011 The SETI Institute
#
# OpenSonATA is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
# 
# OpenSonATA is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
# 
# You should have received a copy of the GNU General Public License
# along with OpenSonATA.  If not, see<http://www.gnu.org/licenses/>.
# 
# Implementers of this code are requested to include the caption
# "Licensed through SETI" with a link to setiQuest.org.
# 
# For alternate licensing arrangements, please contact
# The SETI Institute at www.seti.org or setiquest.org. 
#
################################################################################

This is the base directory for the sonata web server files,
to be run under the jakarta tomcat web server.

INSTALLING TOMCAT

mkdir ~/tomcat
cd ~/tomcat
wget http://mirror.nyi.net/apache//tomcat/tomcat-5/v5.5.31/bin/apache-tomcat-5.5.31.tar.gz
gunzip apache-tomcat-5.5.31.tar.gz
tar -xvf apache-tomcat-5.5.31.tar

This is how it was installed on sse100:

1) Put the tomcat release into /usr/local/jakarta-tomcat.
2) made new directory /home/sonata/java
3) put jdk-1_5_0_22-linux-amd64.bin in /home/sonata/java and executed it.
4) In .cshrc ${HOME}/OpenSonATA/scripts is added to the path.
4) In .cshrc  add this at end:
# For tomcat
setenv CATALINA_HOME /usr/local/jakarta-tomcat
setenv CATALINA_BASE /home/sonata/OpenSonATA/sse-pkg/web/sse-tomcat
setenv CATALINA_OPTS -Djava.awt.headless=true
setenv JAVA_OPTS "-Xms128m -Xmx128m -Djava.awt.headless=true"
setenv DISPLAY localhost:1.0
5) In /etc/init.d create new file "xvfb". It should contin this command:
   /usr/bin/Xvfb :1 -screen 0 1024x768x16 &
6) In /etc/init.d/rc3.d "ln -s ../xvfb S99xvfb"

Edit conf/tomcat-users.xml to look like this:

<?xml version='1.0' encoding='utf-8'?>
<tomcat-users>
  <role rolename="observer"/>
  <user username="user1" password="pass1" roles="observer"/>
</tomcat-users>

This defines the user name and password for web page login. Specify your
desired username and password in place of user1 and pass1.

To build:

CREATE servlets-ssi.jar

Make sure to rename the tomcat server/lib/servlets-ssi.renametojar
file to servlets-ssi.jar.

BUILD AND INSTALL
cd webapps/sonata/WEB-INF/src
make

TO START
$CATALINA_HOME/bin/startup.sh

TO STOP
$CATALINA_HOME/bin/shutdown.sh

By default, the web server runs on port 8080,
so to access it with your browser the URL should
look something like:

  http://localhost:8080
