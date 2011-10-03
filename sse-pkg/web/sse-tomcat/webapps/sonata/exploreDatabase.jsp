<%--

 File:    exploreDatabase.jsp
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

<%-- 
Purpose: User specified database queries.
Description: 
   User sets query parameters, and executes it.
   Result is an HTML table.
   Query params are also stored in session scope for later reuse.
--%>

<%@ include file="taglibsHeader.jspf" %>

<%-- turn off browser caching to get fresh content --%>
<sonatautils:doNotCache/>


<%-- set the default 'save as' filename --%>
<% response.setHeader("Content-disposition", "filename=" +
                  "sonata-query.txt"); %>


<html>
<head>
   <title> SonATA Operations: Explore Database</title>

   <%@ include file="scroll-body-css.jspf" %>

   <%-- Right-align all text input fields --%>
   <style type="text/css">
      INPUT {text-align:right}
      TD {text-align:right}
      
      #userQueryText {
         margin-bottom: 5px;
      }
      #queryTextEntered {
         margin-top: 5px;
         margin-bottom: 5px;
         padding: 5px;
      }
      #clearButton {
         display:block;
      }
   </style>

   <script type="text/javascript" src="js/jquery-1.6.2.min.js"></script>
   
   <script type="text/javascript">
      var html5_storage = supports_html5_storage();
      
      // handles "User Query" UI
      function queryHandler() {
         var val = $("#queryTypeField").val();
         
         if (val == "User Query") {
            var lastUserQuery = "";
            if (html5_storage) {
               lastUserQuery = localStorage.getItem("lastUserQuery");  
            }
            
            $("#userQueryText").append('<div id="queryHeader"><b>Enter Query: </b></div>');
            $("#userQueryText").append('<textarea name="queryTextEntered" id="queryTextEntered" rows="20" cols="100"></textarea>');
            if (lastUserQuery) {
	            $("#queryTextEntered").val(lastUserQuery);
	         }
            $("#userQueryText").append('<input id="clearButton" type="button" value="Clear Query" onClick="clearQueryHandler()" />');
         } else {
            $("#userQueryText").html('');
         }
      }
 
      /*
      Checks the first word that the user entered for "User Query".
      Only select, desc(ribe), show are allowed
      */
      function enterQueryHandler() {
         var val = $("#queryTypeField").val();
         if (val != "User Query")  { 
            $("#userQueryText").hide();
            return; 
         }
          
         var acceptedCmd = new Array("select", "desc", "describe", "show");
         var text = $("#queryTextEntered").val();
         var words = text.split(" ");
         var firstWord = words[0].toLowerCase();
         var accepted = false;
         for (var i=0; i<acceptedCmd.length; i++) {
            if (firstWord == acceptedCmd[i]) {
               accepted = true;
               break;
            }
         }
         if (accepted == true) {
            if (html5_storage) {
               localStorage.setItem("lastUserQuery", text);
            }
         } else {
            alert("ERROR: \"" + firstWord + "\" is not accepted!");
            $("#hiddenQueryStatus").val("UserQueryError");
            $("#queryTypeField").val('');
         }
      }

      function clearQueryHandler() {
         $("#queryTextEntered").val("");
         document.getElementById("queryTextEntered").focus();
      }
      
      function supports_html5_storage() {
        try {
          return 'localStorage' in window && window['localStorage'] !== null;
        } catch (e) {
          return false;
        }
      }

   </script>
</head>

<body onload='document.location="#contentTop"'>

<script type="text/javascript">
   $(document).ready(function() {
      queryHandler();
   });
</script>
 
<div id="header">
<%@ include file="header.jspf" %>
<br>
</div>

<div id="contents">

<%-- set focus to top of page so page up/down keys will work --%>
<div id="contentTop"></div>

<%-- get database configuration information from the config file --%>
<ptags:dbConfig dbHost="configDbHost" dbName="configDbName"/>

<jsp:useBean id="dbFormBean" class="sonata.beans.DbQueryFormBean">
   <jsp:setProperty name="dbFormBean" property="*"/>
</jsp:useBean>

<%-- ---- prepare user supplied form fields --- --%>
<c:set var="idLength" value="10"/>

<c:set var="maxIdDefault" value="100000000"/>

<c:set var="minActIdDefault" value="1"/>
<c:set var="maxActIdDefault" value="${maxIdDefault}"/>

<c:set var="minTargetIdDefault" value="0"/>
<c:set var="maxTargetIdDefault" value="${maxIdDefault}"/>

<c:set var="freqLength" value="12"/>
<c:set var="minRfFreqMhzDefault" value="-1.0"/>
<c:set var="maxRfFreqMhzDefault" value="12000.0"/>

<c:set var="dateLength" value="19"/>
<c:set var="minDateDefault" value="2001-01-01 00:00:00"/>
<c:set var="maxDateDefault" value="2020-01-01 00:00:00"/>

<c:set var="maskHitsDefault" value="0"/>

<c:set var="dbHostDefault" value="${configDbHost}"/>
<c:set var="dbNameDefault" value="${configDbName}"/>

<%-- list of query types for selection options ------%>
<c:set var="queryTypeConfirmCand" value="Confirmed Candidates"/>
<c:set var="queryTypeCand" value="Candidates"/>
<c:set var="queryTypeCandFreqDriftPlot" value="Cand Freq/Drift Plot"/>
<c:set var="queryTypeSignalFreqDriftPlot" value="Signals Freq/Drift Plot"/>
<c:set var="queryTypeSignalFreqPowerPlot" value="Signals Freq/Power Plot"/>
<c:set var="queryTypeSignalFreqCountPlot" value="Signals Freq/Count Plot"/>
<c:set var="queryTypeSignals" value="Signals"/>
<c:set var="queryTypeObsHist" value="Observation History"/>
<c:set var="queryTypeActUnits" value="Activity Units (DXs)"/>
<c:set var="queryTypeIfcStatus" value="IFC Status"/>
<c:set var="queryTypeFailedActs" value="Failed Activities"/>
<c:set var="queryTypeFailedActUnits" value="Failed Act Units"/>
<c:set var="queryTypeBirdieMask" value="Birdie Mask"/>
<c:set var="queryTypeBirdieMaskByBeam" value="Birdie Mask By Beam"/>
<c:set var="queryTypePermRfiMask" value="Perm RFI Mask"/>
<c:set var="queryTypePermRfiMaskPlot" value="Perm RFI Mask Plot"/>
<c:set var="queryTypeShiftedSubbands" value="Shifted Subbands"/>
<c:set var="queryTypeTestSignal" value="Test Signal Counts"/>
<c:set var="queryTypeTargetHistory" value="Target History"/>
<c:set var="queryTypeTargetHistPlot" value="Target Hist RA/Dec Plot"/>
<c:set var="queryTypeTargetHistFreqPlot" value="Target Hist Freq Plot"/>
<c:set var="queryTypeTargetCatalog" value="Target Catalog"/>
<c:set var="queryTypeTargetCatPlot" value="Target Cat RA/Dec Plot"/>
<c:set var="queryTypeUserQuery" value="User Query"/>

<c:set var="queryTypeDefault" value="${queryTypeObsHist}"/>
<%-- -------------------------------- --%>
<%-- list of output format types --%>
<c:set var="tableFormatHtml" value="HTML"/>
<c:set var="tableFormatTabs" value="Tabs"/>
<c:set var="tableFormatCommas" value="Commas (CSV)"/>

<c:set var="tableFormatDefault" value="${tableFormatHtml}"/>
<%-- -------------------------------- --%>

<%-- -------------------------------- --%>
<%-- list of beams --%>
<c:set var="beamOne" value="1"/>
<c:set var="beamTwo" value="2"/>
<c:set var="beamThree" value="3"/>
<c:set var="beamOneTwo" value="1,2"/>
<c:set var="beamOneThree" value="1,3"/>
<c:set var="beamTwoThree" value="2,3"/>
<c:set var="beamOneTwoThree" value="1,2,3"/>

<c:set var="beamDefault" value="${beamOneTwo}"/>
<%-- -------------------------------- --%>


<%--
Setting the form values:

Use defaults if explicitly requested.
Otherwise, use the values available, in this order:
   params, session, defaults.
--%>


<c:choose>
<c:when test="${param.restoreDefaults}">
   <c:set var="formValueSource" value="defaults"/>
</c:when>
<c:otherwise>
   <c:choose>
   <c:when test="${param.submitted}">
       <c:set var="formValueSource" value="params"/>
   </c:when>
   <c:when test="${sessionScope.dbQueryValuesSaved}">
       <c:set var="formValueSource" value="session"/>
   </c:when>
   <c:otherwise>
      <c:set var="formValueSource" value="defaults"/>
   </c:otherwise>
   </c:choose>
</c:otherwise>
</c:choose>


<c:choose>
<c:when test="${formValueSource == 'defaults'}">
   <c:set var="minDateValue" value="${minDateDefault}"/>
   <c:set var="maxDateValue" value="${maxDateDefault}"/>
   <c:set var="minActIdValue" value="${minActIdDefault}"/>
   <c:set var="maxActIdValue" value="${maxActIdDefault}"/>
   <c:set var="minRfFreqMhzValue" value="${minRfFreqMhzDefault}"/>
   <c:set var="maxRfFreqMhzValue" value="${maxRfFreqMhzDefault}"/>
   <c:set var="minTargetIdValue" value="${minTargetIdDefault}"/>
   <c:set var="maxTargetIdValue" value="${maxTargetIdDefault}"/>
   <c:set var="maskHitsValue" value="${maskHitsDefault}"/>
   <c:set var="dbHostValue" value="${dbHostDefault}"/>
   <c:set var="dbNameValue" value="${dbNameDefault}"/>
   <c:set var="queryTypeValue" value="${queryTypeDefault}"/>
   <c:set var="beamValue" value="${beamDefault}"/>
   <c:set var="tableFormatValue" value="${tableFormatDefault}"/>
</c:when>
<c:when test="${formValueSource == 'session'}">
   <c:set var="minDateValue" value="${sessionScope.dbQueryMinDateValue}"/>
   <c:set var="maxDateValue" value="${sessionScope.dbQueryMaxDateValue}"/>
   <c:set var="minActIdValue" value="${sessionScope.dbQueryMinActIdValue}"/>
   <c:set var="maxActIdValue" value="${sessionScope.dbQueryMaxActIdValue}"/>
   <c:set var="minRfFreqMhzValue" value="${sessionScope.dbQueryMinRfFreqMhzValue}"/>
   <c:set var="maxRfFreqMhzValue" value="${sessionScope.dbQueryMaxRfFreqMhzValue}"/>
   <c:set var="minTargetIdValue" value="${sessionScope.dbQueryMinTargetIdValue}"/>
   <c:set var="maxTargetIdValue" value="${sessionScope.dbQueryMaxTargetIdValue}"/>
   <c:set var="maskHitsValue" value="${sessionScope.dbQueryMaskHitsValue}"/>
   <c:set var="dbHostValue" value="${sessionScope.dbQueryDbHostValue}"/>
   <c:set var="dbNameValue" value="${sessionScope.dbQueryDbNameValue}"/>
   <c:set var="queryTypeValue" value="${sessionScope.dbQueryQueryTypeValue}"/>
   <c:set var="beamValue" value="${sessionScope.dbQueryBeamValue}"/>
   <c:set var="tableFormatValue" value="${sessionScope.dbQueryTableFormatValue}"/>
</c:when>
<c:when test="${formValueSource == 'params'}">
   <c:set var="minDateValue" value="${param.minDateField}"/>
   <c:set var="maxDateValue" value="${param.maxDateField}"/>
   <c:set var="minActIdValue" value="${param.minActIdField}"/>
   <c:set var="maxActIdValue" value="${param.maxActIdField}"/>
   <c:set var="minRfFreqMhzValue" value="${param.minRfFreqMhzField}"/>
   <c:set var="maxRfFreqMhzValue" value="${param.maxRfFreqMhzField}"/>
   <c:set var="minTargetIdValue" value="${param.minTargetIdField}"/>
   <c:set var="maxTargetIdValue" value="${param.maxTargetIdField}"/>
   <c:set var="maskHitsValue" value="${param.maskHitsField}"/>
   <c:set var="dbHostValue" value="${param.dbHostField}"/>
   <c:set var="dbNameValue" value="${param.dbNameField}"/>
   <c:set var="queryTypeValue" value="${param.queryTypeField}"/>
   <c:set var="beamValue" value="${param.beamField}"/>
   <c:set var="tableFormatValue" value="${param.tableFormatField}"/>
</c:when>
<c:otherwise>
    Error: Invalid formValueSource: ${formValueSource}<br>
</c:otherwise>
</c:choose>

<%-- prepare some earlier start times for date range --%>
<c:set var="datePattern" value="yyyy-MM-dd HH:mm:ss"/>
<c:set var="timeZoneValue" value="UTC"/>

<jsp:useBean id="oneDayAgo" class="java.util.GregorianCalendar"/>
<% oneDayAgo.add(java.util.GregorianCalendar.DAY_OF_WEEK, -1); %>
<fmt:formatDate pattern="${datePattern}"
   timeZone="${timeZoneValue}" value="${oneDayAgo.time}" var="oneDayAgoString"/>

<jsp:useBean id="oneWeekAgo" class="java.util.GregorianCalendar"/>
<% oneWeekAgo.add(java.util.GregorianCalendar.DAY_OF_WEEK, -7); %>
<fmt:formatDate pattern="${datePattern}"
   timeZone="${timeZoneValue}" value="${oneWeekAgo.time}" var="oneWeekAgoString" />


<%--
Entry form for database fields.  Form is configured to prevent
premature submit due to pressing Enter/Return key in any of the
text fields.
--%>


<c:set var="formValid" value="true"/>
<c:set var="colNameAlign" value="right"/>

<form onsubmit="return false">
<input type=hidden name="restoreDefaults" value="false">
<input type=hidden name="submitted" value="true">
<input id="hiddenQueryStatus" name="hiddenQueryStatus" type="hidden" value="" />

<table>
    <tr>
       <%-- min/max date --%>
       <th align=${colNameAlign}>Date Min:</th>
       <td><input type=text name=minDateField 
          maxlength=${dateLength} value="${minDateValue}"></td>

       <th align=${colNameAlign}>Date Max:</th>
       <td><input type=text name=maxDateField 
          maxlength=${dateLength} value="${maxDateValue}"></td>

       <th align=${colNameAlign}>UTC</th>

       <td><input type=button value="All" 
          onclick="this.form.minDateField.value='${minDateDefault}';
          this.form.maxDateField.value='${maxDateDefault}'">
       </td>

       <td><input type=button value="Prev. Day" 
          onclick="this.form.minDateField.value='${oneDayAgoString}';
          this.form.maxDateField.value='${maxDateDefault}'">
       </td>

       <td><input type=button value="Prev. Week" 
          onclick="this.form.minDateField.value='${oneWeekAgoString}';
          this.form.maxDateField.value='${maxDateDefault}'">
       </td>

    </tr>   
    <tr>
       <%-- min/max act id --%>
       <th align=${colNameAlign}>Act Id Min:</th>
       <td><input type=text name=minActIdField
          maxlength=${idLength} value="${minActIdValue}"></td>

       <th align=${colNameAlign}>Act Id Max:</th>
       <td><input type=text name=maxActIdField 
          maxlength=${idLength} value="${maxActIdValue}"></td>

       <th align=${colNameAlign}>   </th>

       <td><input type=button value="All" 
          onclick="this.form.minActIdField.value='${minActIdDefault}';
          this.form.maxActIdField.value='${maxActIdDefault}'">
       </td>


    </tr>   
    <tr>
       <%-- min/max rf freq --%>
       <th align=${colNameAlign}>RF Freq Min:</th>
       <td><input type=text name=minRfFreqMhzField
          maxlength=${freqLength} value="${minRfFreqMhzValue}"></td>

       <th align=${colNameAlign}>RF Freq Max:</th>
       <td><input type=text name=maxRfFreqMhzField
          maxlength=${freqLength} value="${maxRfFreqMhzValue}"></td>

       <th align=${colNameAlign}>MHz</th>

       <td><input type=button value="All" 
          onclick="this.form.minRfFreqMhzField.value='${minRfFreqMhzDefault}';
          this.form.maxRfFreqMhzField.value='${maxRfFreqMhzDefault}'">
       </td>


    </tr>   
    <tr>
       <%-- min/max target id --%>
       <th align=${colNameAlign}>Target Id Min:</th>
       <td><input type=text name=minTargetIdField
          maxlength=${idLength} value="${minTargetIdValue}"></td>

       <th align=${colNameAlign}>Target Id Max:</th>
       <td><input type=text name=maxTargetIdField
          maxlength=${idLength} value="${maxTargetIdValue}"></td>

       <th align=${colNameAlign}>   </th>

       <td><input type=button value="All" 
          onclick="this.form.minTargetIdField.value='${minTargetIdDefault}';
          this.form.maxTargetIdField.value='${maxTargetIdDefault}'">
       </td>

    </tr>   

    <tr>
       <th align=${colNameAlign}>DB Host:</th>
       <td><input type=text name=dbHostField value="${dbHostValue}"></td>

       <th align=${colNameAlign}>DB Name:</th>
       <td><input type=text name=dbNameField value="${dbNameValue}"></td>
    </tr>

    <tr>
       <%-- mask count threshold --%>
       <th align=${colNameAlign}>Mask Hits:</th>
       <td><input type=text name=maskHitsField value="${maskHitsValue}"></td>
    </tr>

    <tr>
       <%-- beam number selection --%>
       <th align=${colNameAlign}>Beams:</th>
       <td>
       <select name=beamField size=1>
          <option ${beamValue == beamOne ? 'selected' : ''}>
              ${beamOne}
          <option ${beamValue == beamTwo ? 'selected' : ''}>
              ${beamTwo}
          <option ${beamValue == beamThree ? 'selected' : ''}>
              ${beamThree}
          <option ${beamValue == beamOneTwo ? 'selected' : ''}>
              ${beamOneTwo}
          <option ${beamValue == beamOneThree ? 'selected' : ''}>
              ${beamOneThree}
          <option ${beamValue == beamTwoThree ? 'selected' : ''}>
              ${beamTwoThree}
          <option ${beamValue == beamOneTwoThree ? 'selected' : ''}>
              ${beamOneTwoThree}
       </select>
       </td>

       <%-- format type --%>
       <th align=${colNameAlign}>Table Format:</th>
       <td>
       <select name=tableFormatField size=1>
          <option ${tableFormatValue == tableFormatHtml ? 'selected' : ''}>
              ${tableFormatHtml}
          <option ${tableFormatValue == tableFormatTabs ? 'selected' : ''}>
              ${tableFormatTabs}
          <option ${tableFormatValue == tableFormatCommas ? 'selected' : ''}>
              ${tableFormatCommas}
       </select>
       </td>
    </tr>

    <tr>
       <%-- query type --%>
       <th align=${colNameAlign}>Query Type:</th>
       <td>
       <select name=queryTypeField id="queryTypeField" size=1>
          <option ${queryTypeValue == queryTypeConfirmCand ? 'selected' : ''}>
              ${queryTypeConfirmCand}
          <option ${queryTypeValue == queryTypeCand ? 'selected' : ''}>
              ${queryTypeCand}
          <option ${queryTypeValue == queryTypeCandFreqDriftPlot ? 'selected' : ''}>
              ${queryTypeCandFreqDriftPlot}
          <option ${queryTypeValue == queryTypeSignalFreqDriftPlot ? 'selected' : ''}>
              ${queryTypeSignalFreqDriftPlot}
          <option ${queryTypeValue == queryTypeSignalFreqPowerPlot ? 'selected' : ''}>
              ${queryTypeSignalFreqPowerPlot}
          <option ${queryTypeValue == queryTypeSignalFreqCountPlot ? 'selected' : ''}>
              ${queryTypeSignalFreqCountPlot}
          <option ${queryTypeValue == queryTypeSignals ? 'selected' : ''}>
              ${queryTypeSignals}
          <option ${queryTypeValue == queryTypeObsHist ? 'selected' : ''}>
              ${queryTypeObsHist}
          <option ${queryTypeValue == queryTypeActUnits ? 'selected' : ''}>
              ${queryTypeActUnits}
          <option ${queryTypeValue == queryTypeIfcStatus ? 'selected' : ''}>
              ${queryTypeIfcStatus}
          <option ${queryTypeValue == queryTypeFailedActs ? 'selected' : ''}>
              ${queryTypeFailedActs}
          <option ${queryTypeValue == queryTypeFailedActUnits ? 'selected' : ''}>
              ${queryTypeFailedActUnits}
          <option ${queryTypeValue == queryTypeBirdieMask ? 'selected' : ''}>
              ${queryTypeBirdieMask}
          <option ${queryTypeValue == queryTypeBirdieMaskByBeam ? 'selected' : ''}>
              ${queryTypeBirdieMaskByBeam}
          <option ${queryTypeValue == queryTypePermRfiMask ? 'selected' : ''}>
              ${queryTypePermRfiMask}
          <option ${queryTypeValue == queryTypePermRfiMaskPlot ? 'selected' : ''}>
              ${queryTypePermRfiMaskPlot}
          <option ${queryTypeValue == queryTypeShiftedSubbands ? 'selected' : ''}>
              ${queryTypeShiftedSubbands}
          <option ${queryTypeValue == queryTypeTestSignal ? 'selected' : ''}>
              ${queryTypeTestSignal}
          <option ${queryTypeValue == queryTypeTargetCatalog ? 'selected' : ''}>
              ${queryTypeTargetCatalog}
          <option ${queryTypeValue == queryTypeTargetCatPlot ? 'selected' : ''}>
              ${queryTypeTargetCatPlot}
          <option ${queryTypeValue == queryTypeTargetHistory ? 'selected' : ''}>
              ${queryTypeTargetHistory}
          <option ${queryTypeValue == queryTypeTargetHistPlot ? 'selected' : ''}>
              ${queryTypeTargetHistPlot}
          <option ${queryTypeValue == queryTypeTargetHistFreqPlot ? 'selected' : ''}>
              ${queryTypeTargetHistFreqPlot}
          <option ${queryTypeValue == queryTypeUserQuery ? 'selected' : ''}>
              ${queryTypeUserQuery}
       </select>
       </td>
    </tr>


 </table>
 <br>
 
 <div id="userQueryText">
    <!--
    <div id="queryHeader"><b>Enter Query: </b></div>
    <textarea name="queryTextEntered" id="queryTextEntered" rows="20" cols="100"></textarea>
    <input id="clearButton" type="button" value="Clear Query" onClick="clearQueryHandler()" />
    -->
 </div>

 <script type="text/javascript">
   $("select#queryTypeField").change(queryHandler);
 </script>
 
 <input type=button value="Execute Query" 
       onclick="this.form.restoreDefaults.value=false; enterQueryHandler(); this.form.submit()">

 <input type=button value="Restore Defaults" 
       onclick="this.form.restoreDefaults.value=true; this.form.submit()">

</form>
<hr>
<c:if test="${param.submitted && param.restoreDefaults != 'true'}">
   <c:if test="${dbFormBean.formValid == false}">
      Invalid input, please correct:<br>
      ${dbFormBean.errors}
      <c:set var="formValid" value="false"/>
   </c:if>
</c:if>


<%-- ok to run query? --%>
<c:if test="${param.restoreDefaults != 'true' && param.submitted &&
   formValid=='true'}">


<%-- save entered params in session for later reuse --%>
<c:set var="dbQueryValuesSaved" scope="session" value="true"/>
<c:set var="dbQueryMinDateValue" scope="session" value="${param.minDateField}"/>
<c:set var="dbQueryMaxDateValue" scope="session" value="${param.maxDateField}"/>
<c:set var="dbQueryMinActIdValue" scope="session" value="${param.minActIdField}"/>
<c:set var="dbQueryMaxActIdValue" scope="session" value="${param.maxActIdField}"/>
<c:set var="dbQueryMinRfFreqMhzValue" scope="session" value="${param.minRfFreqMhzField}"/>
<c:set var="dbQueryMaxRfFreqMhzValue" scope="session" value="${param.maxRfFreqMhzField}"/>
<c:set var="dbQueryMinTargetIdValue" scope="session" value="${param.minTargetIdField}"/>
<c:set var="dbQueryMaxTargetIdValue" scope="session" value="${param.maxTargetIdField}"/>
<c:set var="dbQueryMaskHitsValue" scope="session" value="${param.maskHitsField}"/>
<c:set var="dbQueryDbHostValue" scope="session" value="${param.dbHostField}"/>
<c:set var="dbQueryDbNameValue" scope="session" value="${param.dbNameField}"/>
<c:set var="dbQueryQueryTypeValue" scope="session" value="${param.queryTypeField}"/>
<c:set var="dbQueryTableFormatValue" scope="session" value="${param.tableFormatField}"/>
<c:set var="dbQueryBeamValue" scope="session" value="${param.beamField}"/>


<%-- make time col title wide enough to match col width for tabbing etc --%>
<c:set var="timeColName" value="StartTime (UTC)    "/>    

Database Query: [${param.dbNameField} on ${param.dbHostField}]<br>

<c:set var="outputFormat" value="${param.tableFormatField}"/>

<%--
Some common constants:
--%>
<c:set var="driftNumDecimals" value="3"/>
<c:set var="rfFreqNumDecimals" value="3"/>

<c:choose>
<%-- ---------------------------------------- --%>
<c:when test="${param.queryTypeField == queryTypeConfirmCand}">

Confirmed Candidates:

<%--
handle very large negative pfa values by limiting them to this cutoff
value, to keep the output column from getting too wide.
--%>
<c:set var="pfaCutoff" value="-99999"/>
<c:set var="queryText"
   value="select activityId as ActId, 
   activityStartTime as '${timeColName}', 
   Activities.type as 'ActType',
   targetId as TargetId, 
   beamNumber as Beam,
   dxNumber as DX, signalIdNumber as SigId,
   CandidateSignals.type as Type, 
   rfFreq as 'RF Freq MHz',
   format(power,1) as Power,
   format(drift,${driftNumDecimals}) as Drift,
   subchanNumber as Subb,
   pol as Pol, 
   sigClass as Class, 
   reason as Reason,
   format(if (pfa < ${pfaCutoff},${pfaCutoff},pfa),1) as PFA,
   format(snr,3) as SNR
   from CandidateSignals, Activities where
   (reason='Confrm' or reason='RConfrm' or reason='NtSnOff')
   and (Activities.id = activityId)
   and (activityId >= ${param.minActIdField} 
      && activityId <= ${param.maxActIdField}) 
   and (activityStartTime >= '${param.minDateField}'
      && activityStartTime <= '${param.maxDateField}')
   and (rfFreq >= '${param.minRfFreqMhzField}'
      && rfFreq <= '${param.maxRfFreqMhzField}')
   and (targetId >= '${param.minTargetIdField}'
      && targetId <= '${param.maxTargetIdField}')
   order by activityId asc
" />

</c:when>
<%-- ---------------------------------------- --%>
<c:when test="${param.queryTypeField == queryTypeCand}">

Candidates:

<%--
handle very large negative pfa values by limiting them to this cutoff
value, to keep the output column from getting too wide.
--%>
<c:set var="pfaCutoff" value="-99999"/>
<c:set var="queryText"
   value="select activityId as ActId, 
   activityStartTime as '${timeColName}', 
   Activities.type as 'ActType',
   targetId as TargetId, 
   beamNumber as Beam,
   dxNumber as DX, signalIdNumber as SigId,
   CandidateSignals.type as Type, 
   rfFreq as 'RF Freq MHz',
   format(power,1) as Power,
   format(drift,${driftNumDecimals}) as Drift,
   subchanNumber as Subb,
   pol as Pol, 
   sigClass as Class, 
   reason as Reason,
   format(if (pfa < ${pfaCutoff},${pfaCutoff},pfa),1) as PFA,
   format(snr,3) as SNR
   from CandidateSignals, Activities where
   (Activities.id = activityId)
   and (activityId >= ${param.minActIdField} 
      && activityId <= ${param.maxActIdField}) 
   and (activityStartTime >= '${param.minDateField}'
      && activityStartTime <= '${param.maxDateField}')
   and (rfFreq >= '${param.minRfFreqMhzField}'
      && rfFreq <= '${param.maxRfFreqMhzField}')
   and (targetId >= '${param.minTargetIdField}'
      && targetId <= '${param.maxTargetIdField}')
   order by activityId asc
" />

</c:when>
<%-- ---------------------------------------- --%>
<c:when test="${param.queryTypeField == queryTypeCandFreqDriftPlot}">

Candidates Freq vs Drift:

   <c:set var="outputFormat" value="plot"/>
   <c:set var="plotTitle" value="Candidates: Freq vs. Drift"/>
   <c:set var="queryText"
   value="select
   rfFreq as 'RF Freq (MHz)',
   format(drift,${driftNumDecimals}) as 'Drift (Hz/s)'
   from CandidateSignals where
   (reason!='PsPwrT')
   and (activityId >= ${param.minActIdField} 
      && activityId <= ${param.maxActIdField}) 
   and (activityStartTime >= '${param.minDateField}'
      && activityStartTime <= '${param.maxDateField}')
   and (rfFreq >= '${param.minRfFreqMhzField}'
      && rfFreq <= '${param.maxRfFreqMhzField}')
   and (targetId >= '${param.minTargetIdField}'
      && targetId <= '${param.maxTargetIdField}')
   and (beamNumber in (${param.beamField}))
" />

</c:when>
<%-- ---------------------------------------- --%>
<c:when test="${param.queryTypeField == queryTypeSignalFreqDriftPlot}">

Signals Freq vs Drift:

   <c:set var="outputFormat" value="plot"/>
   <c:set var="plotTitle" value="Signals: Freq vs. Drift"/>
   <c:set var="queryText"
   value="select
   rfFreq as 'RF Freq (MHz)',
   format(drift,${driftNumDecimals}) as 'Drift (Hz/s)'
   from Signals where
   (activityId >= ${param.minActIdField} 
      && activityId <= ${param.maxActIdField}) 
   and (activityStartTime >= '${param.minDateField}'
      && activityStartTime <= '${param.maxDateField}')
   and (rfFreq >= '${param.minRfFreqMhzField}'
      && rfFreq <= '${param.maxRfFreqMhzField}')
   and (targetId >= '${param.minTargetIdField}'
      && targetId <= '${param.maxTargetIdField}')
   and (drift between -1 and 1)
   and (beamNumber in (${param.beamField}))
" />

</c:when>
<%-- ---------------------------------------- --%>
<c:when test="${param.queryTypeField == queryTypeSignalFreqPowerPlot}">

<%--
Limit max power so that very strong signals don't dominate the plot.
--%>

Signals Freq vs Power:

   <c:set var="outputFormat" value="plot"/>
   <c:set var="plotTitle" value="Signals: Freq vs. Power"/>
   <c:set var="queryText"
   value="select
   rfFreq as 'RF Freq (MHz)',
   power as 'Power'
   from Signals where
   power < 1000
   and (activityId >= ${param.minActIdField} 
      && activityId <= ${param.maxActIdField}) 
   and (activityStartTime >= '${param.minDateField}'
      && activityStartTime <= '${param.maxDateField}')
   and (rfFreq >= '${param.minRfFreqMhzField}'
      && rfFreq <= '${param.maxRfFreqMhzField}')
   and (targetId >= '${param.minTargetIdField}'
      && targetId <= '${param.maxTargetIdField}')
   and (beamNumber in (${param.beamField}))
" />

</c:when>
<%-- ---------------------------------------- --%>
<c:when test="${param.queryTypeField == queryTypeSignalFreqCountPlot}">

Signals Freq Count:

<%-- Note that commas are removed from the rfFreq format so that 
sql decimal conversion in the plot servlet does not throw an exception
 --%>
   <c:set var="outputFormat" value="plot"/>
   <c:set var="plotTitle" value="Signals: Freq vs. Count"/>
   <c:set var="queryText"
   value="select
   replace(format(rfFreq,${rfFreqNumDecimals}),',','') as RfFreq,
   count(*)
   from Signals where
   (activityId >= ${param.minActIdField} 
      && activityId <= ${param.maxActIdField}) 
   and (activityStartTime >= '${param.minDateField}'
      && activityStartTime <= '${param.maxDateField}')
   and (rfFreq >= '${param.minRfFreqMhzField}'
      && rfFreq <= '${param.maxRfFreqMhzField}')
   and (targetId >= '${param.minTargetIdField}'
      && targetId <= '${param.maxTargetIdField}')
   and (beamNumber in (${param.beamField}))
   group by format(rfFreq,${rfFreqNumDecimals})
" />

</c:when>
<%-- ---------------------------------------- --%>
<c:when test="${param.queryTypeField == queryTypeSignals}">

Signals:

<%--
handle very large negative pfa values by limiting them to this cutoff
value, to keep the output column from getting too wide.
--%>
<c:set var="pfaCutoff" value="-99999"/>
<c:set var="queryText"
   value="select activityId as ActId, 
   activityStartTime as '${timeColName}', targetId as TargetId, 
   beamNumber as Beam,
   dxNumber as DX, signalIdNumber as SigId, type as Type, 
   rfFreq as 'RF Freq MHz',
   format(power,1) as Power,
   format(drift,${driftNumDecimals}) as Drift,
   subchanNumber as Subb,
   pol as Pol, 
   sigClass as Class, 
   reason as Reason,
   format(if (pfa < ${pfaCutoff},${pfaCutoff},pfa),1) as PFA,
   format(snr,3) as SNR
   from Signals where
   (activityId >= ${param.minActIdField} 
      && activityId <= ${param.maxActIdField}) 
   and (activityStartTime >= '${param.minDateField}'
      && activityStartTime <= '${param.maxDateField}')
   and (rfFreq >= '${param.minRfFreqMhzField}'
      && rfFreq <= '${param.maxRfFreqMhzField}')
   and (targetId >= '${param.minTargetIdField}'
      && targetId <= '${param.maxTargetIdField}')
   order by activityId asc
" />

</c:when>
<%-- ---------------------------------------- --%>
<c:when test="${param.queryTypeField == queryTypeObsHist}">
Observation History:

   <c:set var="queryText"
   value="select Activities.id as ActId, 
   Activities.ts as '${timeColName}',
   Activities.type as Type,
   ActParameters.targetbeam1 as Beam1Id,
   ActParameters.targetbeam2 as Beam2Id,
   ActParameters.targetbeam3 as Beam3Id,
   ActParameters.targetbeam4 as Beam4Id,
   minDxSkyFreqMhz as MinMhz, maxDxSkyFreqMhz as MaxMhz,
   cwSignals + pulseSignals as 'TotSig',
   zeroDriftSignals as ZeroDft,
   recentRfiDatabaseMatches as RctRfi,	
   allCwCandidates + allPulseCandidates as 'TotCand',
   confirmedCwCandidates + confirmedPulseCandidates as TotConf 
   from Activities, ActParameters where
   Activities.actParametersId = ActParameters.id
   and (Activities.id >= ${param.minActIdField} 
      && Activities.id <= ${param.maxActIdField}) 
   and (Activities.ts >= '${param.minDateField}'
      && Activities.ts <= '${param.maxDateField}')
   and (minDxSkyFreqMhz >= '${param.minRfFreqMhzField}'
      && minDxSkyFreqMhz <= '${param.maxRfFreqMhzField}')
   and (maxDxSkyFreqMhz >= '${param.minRfFreqMhzField}'
      && maxDxSkyFreqMhz <= '${param.maxRfFreqMhzField}')
   order by ActId asc
   "/>

</c:when>
<%-- ---------------------------------------- --%>
<c:when test="${param.queryTypeField == queryTypeActUnits}">
Act Units (Dxs):

   <c:set var="queryText"
   value="select ActivityUnits.activityId as ActId,
   ActivityUnits.startOfDataCollection as '${timeColName}',
   Activities.type as Type,
   dxNumber as Dx,
   dxTuneFreq as DxMhz,
   targetId as Target,
   beamNumber as beam,
   ActivityUnits.cwSignals as 'CWSig',
   ActivityUnits.pulseSignals as 'PulSig',
   ActivityUnits.cwSignals + ActivityUnits.pulseSignals as TotSig,
   ActivityUnits.allCwCandidates as CWCand,
   ActivityUnits.allPulseCandidates as PulCand,
   ActivityUnits.allCwCandidates + 
   ActivityUnits.allPulseCandidates as TotCand,
   ActivityUnits.confirmedCwCandidates as 'CWConf',  
   ActivityUnits.confirmedPulseCandidates as 'PulConf', 
   ActivityUnits.confirmedCwCandidates + 
   ActivityUnits.confirmedPulseCandidates as 'TotConf'
   from Activities, ActivityUnits
   where
   Activities.id = ActivityUnits.activityId
   and
   (Activities.id >= '${param.minActIdField}' 
      && Activities.id <= '${param.maxActIdField}') 
   and (Activities.ts >= '${param.minDateField}'
      && Activities.ts <= '${param.maxDateField}')
   and (Activities.minDxSkyFreqMhz >= '${param.minRfFreqMhzField}'
      && Activities.minDxSkyFreqMhz <= '${param.maxRfFreqMhzField}')
   and (Activities.maxDxSkyFreqMhz >= '${param.minRfFreqMhzField}'
      && Activities.maxDxSkyFreqMhz <= '${param.maxRfFreqMhzField}')
   order by ActivityUnits.activityId, dxTuneFreq
   "/>

</c:when>
<%-- ---------------------------------------- --%>
<c:when test="${param.queryTypeField == queryTypeIfcStatus}">
ifc Status:

   <c:set var="queryText"
   value="select actId as ActId, 
   Activities.ts as '${timeColName}',
   ifc as Ifc,
   Activities.minDxSkyFreqMhz as MinMhz, 
   Activities.maxDxSkyFreqMhz as MaxMhz, 
   attnL as AtnL,
   attnR as AtnR, 
   format(stxMeanL,4) as MeanL,
   format(stxMeanR,4) as MeanR,
   format(stxVarL,4) as VarL,
   format(stxVarR,4) as VarR,
   hex(stxStatus) as StxStatus
   from ifcStatus, Activities 
   where Activities.id = actId 
   and (Activities.id >= ${param.minActIdField} 
      && Activities.id <= ${param.maxActIdField}) 
   and (Activities.ts >= '${param.minDateField}'
      && Activities.ts <= '${param.maxDateField}')
   "/>

</c:when>
<%-- ---------------------------------------- --%>
<c:when test="${param.queryTypeField == queryTypeFailedActs}">

Failed Activities:
   <c:set var="queryText" 
   value="select id as ActId, ts as '${timeColName}',
   type as Type,
   minDxSkyFreqMhz as MinMhz, maxDxSkyFreqMhz as MaxMhz,
   comments from Activities where 
   validObservation = 'No'
   and (id >= ${param.minActIdField} 
      && id <= ${param.maxActIdField}) 
   and (ts >= '${param.minDateField}'
      && ts <= '${param.maxDateField}')
   "/>

</c:when>
<%-- ---------------------------------------- --%>
<c:when test="${param.queryTypeField == queryTypeFailedActUnits}">

Failed Act Units:
   <c:set var="queryText" 
   value="select activityId as ActId, ts as '${timeColName}',
   dxTuneFreq as DxMHz,
   comments from ActivityUnits where 
   validObservation = 'No'
   and (activityId >= ${param.minActIdField} 
      && activityId <= ${param.maxActIdField}) 
   and (ts >= '${param.minDateField}'
      && ts <= '${param.maxDateField}')
   "/>

</c:when>
<%-- ---------------------------------------- --%>
<c:when test="${param.queryTypeField == queryTypeBirdieMask}">

Birdie Mask:

<c:set var="tuningFreqCol" value="DxActivityParameters.rcvrSkyFrequency"/>
<c:set var="queryText" 
   value="select 
   FORMAT(rfFreq - ${tuningFreqCol},${rfFreqNumDecimals}) as BirdieFreqMHz,
   count(distinct Signals.activityId) as Count,
   MAX(power) as MaxPower 
   from Signals, ActivityUnits, DxActivityParameters 
   where 
   Signals.dbActivityUnitId = ActivityUnits.id
   and ActivityUnits.dxActivityParametersId = DxActivityParameters.id
   and ActivityUnits.validObservation = 'Yes'
   and (ActivityUnits.activityId >= ${param.minActIdField} 
      && ActivityUnits.activityId <= ${param.maxActIdField}) 
   and (ActivityUnits.ts >= '${param.minDateField}'
      && ActivityUnits.ts <= '${param.maxDateField}')
   and (Signals.beamNumber in (${param.beamField}))
   group by BirdieFreqMHz having count >= ${param.maskHitsField}
   order by (rfFreq - ${tuningFreqCol})"
/>

</c:when>
<%-- ---------------------------------------- --%>
<c:when test="${param.queryTypeField == queryTypeBirdieMaskByBeam}">

Birdie Mask By Beam:

<c:set var="tuningFreqCol" value="DxActivityParameters.rcvrSkyFrequency"/>
<c:set var="queryText" 
   value="select 
   Signals.beamNumber as beam,
   FORMAT(rfFreq - ${tuningFreqCol},${rfFreqNumDecimals}) as BirdieFreqMHz,
   count(distinct Signals.activityId) as Count,
   MAX(power) as MaxPower 
   from Signals, ActivityUnits, DxActivityParameters
   where
   Signals.dbActivityUnitId = ActivityUnits.id
   and ActivityUnits.dxActivityParametersId = DxActivityParameters.id
   and ActivityUnits.validObservation = 'Yes'
   and (ActivityUnits.activityId >= ${param.minActIdField} 
      && ActivityUnits.activityId <= ${param.maxActIdField}) 
   and (ActivityUnits.ts >= '${param.minDateField}'
      && ActivityUnits.ts <= '${param.maxDateField}')
   and (Signals.beamNumber in (${param.beamField}))
   group by Signals.beamNumber,BirdieFreqMHz 
   having count >= ${param.maskHitsField}
   order by beam,(rfFreq - ${tuningFreqCol})"
/>

</c:when>
<%-- ---------------------------------------- --%>
<c:when test="${param.queryTypeField == queryTypePermRfiMask}">

Permanent RFI Mask:

<c:set var="queryText" 
   value="select 
   ActivityUnits.activityId as ActId, 
   ActivityUnits.startOfDataCollection as '${timeColName}',
   Activities.type as Type, 
   dxTuneFreq as DxMhz, 
   ActivityUnits.cwSignals CwSig,
   ActivityUnits.pulseSignals as PulSig,
   ActivityUnits.cwSignals + ActivityUnits.pulseSignals as TotSig
   from Activities, ActivityUnits 
   where Activities.id = ActivityUnits.activityId 
   and (Activities.type is not null)
   and (ActivityUnits.cwSignals + ActivityUnits.pulseSignals >= 
      ${param.maskHitsField}) 
   and (ActivityUnits.activityId >= ${param.minActIdField} 
      && ActivityUnits.activityId <= ${param.maxActIdField}) 
   and (ActivityUnits.ts >= '${param.minDateField}'
      && ActivityUnits.ts <= '${param.maxDateField}')
   and (dxTuneFreq >= '${param.minRfFreqMhzField}'
      && dxTuneFreq <= '${param.maxRfFreqMhzField}')
   order by dxTuneFreq"
/>

</c:when>
<%-- ---------------------------------------- --%>
<c:when test="${param.queryTypeField == queryTypePermRfiMaskPlot}">

Permanent RFI Mask:

<%-- Note that commas are removed from the dx freq format so that 
sql decimal conversion in the plot servlet does not throw an exception
 --%>

   <c:set var="outputFormat" value="plot"/>
   <c:set var="plotTitle" value="Perm RFI Mask"/>
   <c:set var="queryText"
   value="select
   replace(format(dxTuneFreq,${rfFreqNumDecimals}),',','') as 'RF MHz',
   ActivityUnits.cwSignals + ActivityUnits.pulseSignals as 'Total Signals Per DX'
   from Activities, ActivityUnits 
   where Activities.id = ActivityUnits.activityId 
   and (Activities.type is not null)
   and (ActivityUnits.cwSignals + ActivityUnits.pulseSignals >= 
      ${param.maskHitsField}) 
   and (ActivityUnits.activityId >= ${param.minActIdField} 
      && ActivityUnits.activityId <= ${param.maxActIdField}) 
   and (ActivityUnits.ts >= '${param.minDateField}'
      && ActivityUnits.ts <= '${param.maxDateField}')
   and (dxTuneFreq >= '${param.minRfFreqMhzField}'
      && dxTuneFreq <= '${param.maxRfFreqMhzField}')
   order by dxTuneFreq"
/>

</c:when>

<%-- ---------------------------------------- --%>
<c:when test="${param.queryTypeField == queryTypeShiftedSubbands}">

Possible shifted subbands:

<%--
Check for 'Shifted subbands' by looking for test signals
in 'iftest' activities that appear at the wrong frequency.
Ignore those signals that are below a minimum SNR
assuming that those are noise hits.
Assumes test signal is near 1420.8003 MHz, with a drift
of approx 0.1 Hz/s.
--%> 

<c:set var="expectedTestFreqMhz" value="1420.800300"/>
<c:set var="testFreqTolMhz" value="0.000200"/>

<%--
handle very large negative pfa values by limiting them to this cutoff
value, to keep the output column from getting too wide.
--%>
<c:set var="pfaCutoff" value="-99999"/>
<c:set var="queryText"
   value="select activityId as ActId, 
   activityStartTime as '${timeColName}', 
   Activities.type as 'ActType',
   targetId as TargetId, 
   beamNumber as Beam,
   dxNumber as DX, signalIdNumber as SigId,
   CandidateSignals.type as Type, 
   rfFreq as 'RF Freq MHz',
   format(power,1) as Power,
   format(drift,${driftNumDecimals}) as Drift,
   subchanNumber as Subb,
   pol as Pol, 
   sigClass as Class, 
   reason as Reason,
   format(if (pfa < ${pfaCutoff},${pfaCutoff},pfa),1) as PFA,
   format(snr,3) as SNR
   from CandidateSignals, Activities
   where
   (CandidateSignals.type = 'CwC')
   and (rfFreq not between ${expectedTestFreqMhz - testFreqTolMhz} and
   ${expectedTestFreqMhz + testFreqTolMhz} )
   and (Activities.type = 'iftest')
   and (snr > 1.0)
   and (drift between 0.098 and 0.100) 
   and (reason='Confrm')
   and (Activities.id = activityId)
   and (activityId >= ${param.minActIdField} 
      && activityId <= ${param.maxActIdField}) 
   and (activityStartTime >= '${param.minDateField}'
      && activityStartTime <= '${param.maxDateField}')
   and (rfFreq >= '${param.minRfFreqMhzField}'
      && rfFreq <= '${param.maxRfFreqMhzField}')
   and (targetId >= '${param.minTargetIdField}'
      && targetId <= '${param.maxTargetIdField}')
   order by activityId asc
" />

</c:when>
<%-- ---------------------------------------- --%>
<c:when test="${param.queryTypeField == queryTypeTestSignal}">

<%--
Count confirmed signals within the expected test signal
frequency range.
--%>

Test Signals:

<c:set var="expectedTestFreqMhz" value="1420.800300"/>
<c:set var="testFreqTolMhz" value="0.000200"/>

<c:set var="queryText"
   value="select activityId as ActId, 
   activityStartTime as '${timeColName}', 
   Activities.type as 'ActType',
   count(*)
   from CandidateSignals, Activities where
   (reason='Confrm')
   and (pol = 'both')
   and (rfFreq between ${expectedTestFreqMhz - testFreqTolMhz}
   and ${expectedTestFreqMhz + testFreqTolMhz} )
   and (Activities.id = activityId)
   and (activityId >= ${param.minActIdField} 
      && activityId <= ${param.maxActIdField}) 
   and (activityStartTime >= '${param.minDateField}'
      && activityStartTime <= '${param.maxDateField}')
   group by activityId
   order by activityId asc
" />

</c:when>
<%-- ---------------------------------------- --%>
<c:when test="${param.queryTypeField == queryTypeTargetCatalog}">

Target Catalog:
   <c:set var="queryText" 
   value="select 
   targetId as TargetId,
   ra2000Hours as Ra2000Hrs,
   dec2000Deg as Dec2000Deg,
   catalog as Catalog,
   aliases
   from TargetCat
   where 
   (targetId >= ${param.minTargetIdField} 
      && targetId <= ${param.maxTargetIdField}) 
   "/>
</c:when>
<%-- ---------------------------------------- --%>
<c:when test="${param.queryTypeField == queryTypeTargetCatPlot}">

Target Catalog Plot:
   <c:set var="outputFormat" value="plot"/>
   <c:set var="plotTitle" value="Target Catalog"/>
   <c:set var="queryText" 
   value="select 
   ra2000Hours as 'RA (Hours)',
   dec2000Deg as 'Dec (Deg)'
   from TargetCat
   where 
   (targetId >= ${param.minTargetIdField} 
      && targetId <= ${param.maxTargetIdField}) 
   "/>
</c:when>
<%-- ---------------------------------------- --%>
<c:when test="${param.queryTypeField == queryTypeTargetHistory}">

Target History:
   <c:set var="queryText" 
   value="select
   Activities.id as ActId, 
   Activities.ts as 'StartTime (UTC)',
   ActParameters.targetprimary as 'PrimaryId',
   TargetCat.ra2000Hours as 'RA (hours)',
   TargetCat.dec2000Deg as 'Dec (deg)',
   ActParameters.targetbeam1 as Beam1Id,
   ActParameters.targetbeam2 as Beam2Id,
   ActParameters.targetbeam3 as Beam3Id,
   ActParameters.targetbeam4 as Beam4Id,
   minDxSkyFreqMhz as MinMhz, 
   maxDxSkyFreqMhz as MaxMhz
   from Activities, ActParameters, TargetCat
   where
   validObservation = 'Yes'
   and Activities.actParametersId = ActParameters.id
   and ActParameters.targetprimary = TargetCat.targetId
   and (Activities.id >= ${param.minActIdField} 
      && Activities.id <= ${param.maxActIdField}) 
   and (Activities.ts >= '${param.minDateField}'
      && Activities.ts <= '${param.maxDateField}')
   and
   (
   (ActParameters.targetprimary >= ${param.minTargetIdField} 
      && ActParameters.targetprimary <= ${param.maxTargetIdField}) 
    or
   (ActParameters.targetbeam1 >= ${param.minTargetIdField} 
      && ActParameters.targetbeam1 <= ${param.maxTargetIdField}) 
    or
   (ActParameters.targetbeam2 >= ${param.minTargetIdField} 
      && ActParameters.targetbeam2 <= ${param.maxTargetIdField}) 
    or
   (ActParameters.targetbeam3 >= ${param.minTargetIdField} 
      && ActParameters.targetbeam3 <= ${param.maxTargetIdField}) 
   )
   "/>
</c:when>
<%-- ---------------------------------------- --%>
<c:when test="${param.queryTypeField == queryTypeTargetHistPlot}">

Target History Plot:
   <c:set var="outputFormat" value="plot"/>
   <c:set var="plotTitle" value="Target History"/>
   <c:set var="queryText" 
   value="select distinct
   TargetCat.ra2000Hours as 'RA (hours)',
   TargetCat.dec2000Deg as 'Dec (deg)'
   from ActivityUnits, TargetCat
   where
   ActivityUnits.validObservation = 'Yes'
   and ActivityUnits.targetId = TargetCat.targetId
   and (ActivityUnits.activityId >= ${param.minActIdField} 
      && ActivityUnits.activityId <= ${param.maxActIdField}) 
   and (ActivityUnits.ts >= '${param.minDateField}'
      && ActivityUnits.ts <= '${param.maxDateField}')
   and (TargetCat.targetId >= ${param.minTargetIdField} 
      && TargetCat.targetId <= ${param.maxTargetIdField}) 
   "/>
</c:when>
<%-- ---------------------------------------- --%>
<c:when test="${param.queryTypeField == queryTypeTargetHistFreqPlot}">

Target History Plot:
   <c:set var="outputFormat" value="plot"/>
   <c:set var="plotTitle" value="Target History (ID vs Freq)"/>
   <c:set var="queryText" 
   value="select
   ActivityUnits.targetId as 'Target ID',
   ActivityUnits.dxTuneFreq as 'RF Freq (MHz)'
   from ActivityUnits
   where
   ActivityUnits.validObservation = 'Yes'
   and (ActivityUnits.activityId >= ${param.minActIdField} 
      && ActivityUnits.activityId <= ${param.maxActIdField}) 
   and (ActivityUnits.ts >= '${param.minDateField}'
      && ActivityUnits.ts <= '${param.maxDateField}')
   and (ActivityUnits.targetId >= ${param.minTargetIdField} 
      && ActivityUnits.targetId <= ${param.maxTargetIdField}) 
   and (dxTuneFreq >= '${param.minRfFreqMhzField}'
      && dxTuneFreq <= '${param.maxRfFreqMhzField}')
   "/>
</c:when>
<c:when test="${param.queryTypeField == queryTypeUserQuery}">

User Query:

<c:set var='queryText' value='<%= request.getParameter("queryTextEntered") %>' />
</c:when>


<%-- ---------------------------------------- --%>
<c:otherwise>
   Query type: '${param.queryTypeField}' not yet implemented.
   <c:set var="queryText" value="" />
</c:otherwise>
<%-- ---------------------------------------- --%>
</c:choose>



<%-- run the query, using the selected output format --%>

<%--
<c:choose>
--%>
<%-- the request parameters are made available in the implicit object param --%>
<c:if test="${param.hiddenQueryStatus != 'UserQueryError'}">
   
   <c:choose>

      <%-- Plot --%>
      <c:when test="${outputFormat == 'plot'}">

      ${queryText}<br>

         <img src="
         <c:url value="servlet/DatabaseScatterPlot">
            <c:param name="dbHost" value="${param.dbHostField}"/>
            <c:param name="dbName" value="${param.dbNameField}"/>
            <c:param name="query" value="${queryText}"/>
            <c:param name="title" value="${plotTitle}"/>
            <c:param name="showQuery" value=""/>
         </c:url>
         ">
      </c:when>

      <%-- Table --%>
      <c:otherwise>
         <c:choose>
         <c:when test="${outputFormat == tableFormatHtml}">
            <c:set var="servletToRun" value="DatabaseQueryTable"/>
            <c:set var="separator" value=""/>
         </c:when>
         <c:when test="${outputFormat == tableFormatTabs}">
            <c:set var="servletToRun" value="DatabaseQueryAsciiTable"/>
            <% String tab="\t"; pageContext.setAttribute("tab", tab); %>
            <c:set var="separator" value="${tab}"/>
         </c:when>
         <c:when test="${outputFormat == tableFormatCommas}">
            <c:set var="servletToRun" value="DatabaseQueryAsciiTable"/>
            <c:set var="separator" value=","/>
         </c:when>
         <c:otherwise>
            Table Format: '${outputFormat} is not yet implemented.
            <c:set var="servletToRun" value=""/>
            <c:set var="separator" value=""/>
         </c:otherwise>
         </c:choose>

         <c:import url="servlet/${servletToRun}">
            <c:param name="dbHost" value="${param.dbHostField}"/>
            <c:param name="dbName" value="${param.dbNameField}"/>
            <c:param name="showQuery" value=""/>
            <c:param name="showRowCount" value=""/> 
            <c:param name="query" value="${queryText}"/>
            <c:param name="separator" value="${separator}"/>
         </c:import>

      </c:otherwise>
   </c:choose>

</c:if>


<%--
<c:catch var="queryException">
</c:catch>
<c:if test="${queryException != null}">
<br><br>Error executing database query:<br>
${queryException}<br><br>
Please check form values above and try again.<br>
</c:if>
--%>

</c:if> <%-- ok to run query --%>

<div id="contentBottom"></div>
</div>

</body>
</html>

