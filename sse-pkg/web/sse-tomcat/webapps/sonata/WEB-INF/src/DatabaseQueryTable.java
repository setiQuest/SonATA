/*******************************************************************************

 File:    DatabaseQueryTable.java
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

*******************************************************************************/

// based on oreilly  "java servlet & jsp cookbook"

import java.io.*;
import java.io.IOException;
import java.io.PrintWriter;
import java.text.*;
import java.sql.*;
import javax.servlet.*;
import javax.servlet.http.*;

/*
   Execute a database query, putting the output in an html table.
   Input parameters:
      dbHost: database host
      dbName: database name
      query: query text
      showQuery: echo the query (optional, use any value to enable)
      showRowCount: print the number of rows (optional, use any value to enable) 
*/

public class DatabaseQueryTable extends DatabaseAccess {
   
   public void doGet(HttpServletRequest request, HttpServletResponse response)
      //throws ServletException, java.io.IOException  {
      throws java.io.IOException  {
      
      Connection conn = null;
      Statement stmt = null;
      ResultSet rs = null;
      ResultSetMetaData rsm = null;

      response.setContentType("text/html");
      java.io.PrintWriter out = response.getWriter();

      try {
      
	 String dbHost = request.getParameter("dbHost");
	 String dbName = request.getParameter("dbName");
	 String showQuery = request.getParameter("showQuery");
	 String showRowCount = request.getParameter("showRowCount");

         conn = connect(dbHost, dbName);

	 String query = request.getParameter("query");
	 stmt = conn.createStatement();
	 
	 rs = stmt.executeQuery(query);
	 
	 rsm = rs.getMetaData();
	 int colCount = rsm.getColumnCount();
	 
 
         if (showQuery != null) {
            out.println("<hr>");
            out.println(query);
            out.println("<hr>");
         }

	 // print column names
	 out.println("<table border=1 cellspacing=1 cellpadding=2><tr>");
	 for (int i = 1; i <=colCount; ++i) {
	    out.println("<th>" + rsm.getColumnLabel(i) + "</th>");
	 }
	 out.println("</tr>");

	 // print values 
         int rowCount = 0;
	 while(rs.next()) {
            rowCount++;
	    out.println("<tr>");
	    
	    for (int i = 1;  i <=colCount; ++i) {
	       out.println("<td>" + rs.getString(i) + "</td>");
	    }
	    out.println("</tr>");
	 }
	 out.println("</table>");

         if (showRowCount != null) {
            out.println(rowCount + " rows.<br>");
         }
      }
      catch (Exception e) {
         
         out.println("<hr>");
         out.println("<p><b>ERROR executing the query</b></p>");
         //System.out.println("Error in DatabaseQueryTable.java");  // writes to catalina.out
         
	 //throw new ServletException(e);

      } finally {

         try {

            // closing the statement also closes the result set
            if (stmt != null) {
               stmt.close();
            }

            if (conn != null) {
               conn.close();
            }

         }
         catch (SQLException sql) {}
         
      }

   } //doGet

}

