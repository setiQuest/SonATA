<%--

 File:    systemControl.jsp
 Project: OpenSonATA
 Authors: The OpenSonATA code is the result of many programmers
          over many years

 Copyright 2011 The SETI Institute

 OpenSonATA is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.
 
 OpenSonATA is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.
 
 You should have received a copy of the GNU General Public License
 along with OpenSonATA.  If not, see<http://www.gnu.org/licenses/>.
 
 Implementers of this code are requested to include the caption
 "Licensed through SETI" with a link to setiQuest.org.
 
 For alternate licensing arrangements, please contact
 The SETI Institute at www.seti.org or setiquest.org. 

--%>

<%-- Runsse startup & shutdown control --%>

<html>
<head>
<title> SonATA Operations: System Control</title>
</head>
<body>
<%@ include file="/header.jspf" %>

<%-- see if runsse is currently executing --%>

<c:import var="procStatus" url="/servlet/CmdExecServlet">
   <c:param name="command" value="ps -ef | grep runsse.sh | grep -v grep" />
   <c:param name="noEcho" value="true" />
</c:import>

<%--
Process check:<br>
${procStatus}
<br>
--%>

Runsse current status:<br>

<c:choose>
   <c:when test="${empty procStatus}">
     <img src="../icons/redHorizBar.gif"><br>
      *Offline*
   </c:when>
   <c:otherwise>
     <img src="../icons/greenHorizBar.gif"><br>
      *Active*
   </c:otherwise>
</c:choose>
<br><br>

<%-- make only the appropriate startup/shutdown button visible --%>
<form action="systemControl.jsp" method="post">
<c:choose>
<c:when test="${empty procStatus}">
   <input type="submit" value="startup" name="runsseAction">
</c:when>
<c:otherwise>
   <input type="submit" value="shutdown" name="runsseAction">
</c:otherwise>
</c:choose>
</form>

<c:if test="${param.runsseAction == 'startup'}">
<jsp:forward page="sysCntlConfirmStartup.jsp" />
</c:if>

<c:if test="${param.runsseAction == 'shutdown'}">
<jsp:forward page="sysCntlConfirmShutdown.jsp" />
</c:if>

</body>
</html>
