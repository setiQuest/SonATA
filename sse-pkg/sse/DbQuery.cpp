/*******************************************************************************

 File:    DbQuery.cpp
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



#include "DbQuery.h" 
#include "SseMessage.h"
#include "SseException.h"
#include "MsgSender.h"
#include "sseInterface.h"
#include "Assert.h"
#include "DebugLog.h"
#include <sstream>

using namespace std;

DbQuery::DbQuery(MYSQL *db, int actId, int verboseLevel)
   :
   subclassName_("<subclass name not set>"),
   context_("<context not set>"),
   db_(db),
   actId_(actId),
   verboseLevel_(verboseLevel),
   resultSet_(0),
   nRequestedCols_(0)
{

}

DbQuery::~DbQuery()
{
    if (resultSet_ != 0)
    {
        mysql_free_result(resultSet_);
    }
}

void DbQuery::setNumberOfRequestedCols(int nRequestedCols)
{
   nRequestedCols_ = nRequestedCols;   
}

void DbQuery::setSubclassName(const string & subclassName)
{
    subclassName_ = subclassName;
}

string DbQuery::getSubclassName()
{
    return subclassName_;
}

void DbQuery::setContext(const string & context)
{
    context_ = context;
}

MYSQL_RES * DbQuery::getResultSet()
{
   return resultSet_;
}

int DbQuery::getVerboseLevel()
{
   return verboseLevel_;
}

void DbQuery::execute()
{
   string methodName(subclassName_ + "::execute()");

   VERBOSE2(verboseLevel_, methodName << " for "
	    << context_ << endl;);
   try
   {
      string queryStmt = prepareQuery();

      submitQuery(queryStmt);

      processQueryResults();
   }
   catch (SseException &except)
   {
     stringstream strm;
     strm << methodName << " " << except 
	  << " for " << context_ << endl;
     SseMessage::log(MsgSender,
                     actId_, SSE_MSG_EXCEPTION,
                     SEVERITY_ERROR, strm.str(),
                     __FILE__, __LINE__);
   }	
   catch (...) 
   { 
     stringstream strm;
     strm << " caught unexpected exception in " << methodName 
	  << " for " << context_ << endl;
     SseMessage::log(MsgSender,
                     actId_, SSE_MSG_EXCEPTION,
                     SEVERITY_ERROR, strm.str(),
                     __FILE__, __LINE__);
   }

}

void DbQuery::submitQuery(const string & queryStmt)
{
   const string methodName(subclassName_ + "::submitQuery");

   AssertMsg(db_, "database connection not set");

   if (mysql_query(db_, queryStmt.c_str()) != 0)
   {	
      stringstream strm;
      strm << methodName 
	   << " submitDbQuery: MySQL error: " 
	   << mysql_error(db_) << endl;
      
      throw SseException(strm.str(), __FILE__, __LINE__,
			 SSE_MSG_DBERR, SEVERITY_ERROR);
   }
   
   resultSet_ = mysql_store_result(db_);
   if (resultSet_ == 0)
   {
      stringstream strm;
      strm << methodName << " : Empty result set.\n";
      throw SseException(strm.str(), __FILE__, __LINE__,
			 SSE_MSG_DBERR, SEVERITY_ERROR);
   }
   
   // check the number of columns received
   unsigned int nColsReceived = mysql_field_count(db_);

   if (nColsReceived != nRequestedCols_)
   {
	   cout << "Col recd " << nColsReceived << " nColRequested "
		   << nRequestedCols_ << endl;
	   cout << "Number of rows " << mysql_num_rows(resultSet_) << endl;
	   cout << queryStmt << endl;
   // TBD change this to a throw
  // AssertMsg(nColsReceived == nRequestedCols_,
//	     "Did not get the expected number of columns");
    throw SseException("Did not get expected number of columns",
		    __FILE__, __LINE__, SSE_MSG_DBERR, SEVERITY_ERROR);
   }

   // log the number of rows found
   my_ulonglong nRows = mysql_num_rows(resultSet_); 
   VERBOSE2(verboseLevel_, "found " << nRows << " rows" << endl;);
   
}
