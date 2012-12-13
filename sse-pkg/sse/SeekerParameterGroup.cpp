/*******************************************************************************

 File:    SeekerParameterGroup.cpp
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



#include "SeekerParameterGroup.h" 
#include <sstream>
#include <mysql.h>
#include "AnyValueParameter.h"
#include "ChoiceParameter.h"
#include "Assert.h"
#include "RecordInDatabase.h"
#include "MysqlQuery.h"
#include "SseException.h"

using namespace std;

static const char *ChoiceOn = "on";
static const char *ChoiceOff = "off";

SeekerParameterGroup::SeekerParameterGroup(const string &command)
   :	ParameterGroup(command),
	dbTableName_(""),
	idColNameInActsTable_(""),
	site_(0)
{
}

SeekerParameterGroup::SeekerParameterGroup(
   const string &command, 
   const string &dbTableName,
   const string &idColNameInActsTable)
   :
   ParameterGroup(command),
   dbTableName_(dbTableName),
   idColNameInActsTable_(idColNameInActsTable),
   site_(0)
{
}

void SeekerParameterGroup::setSite(Site *site)
{
   site_ = site;
}

Site *SeekerParameterGroup::getSite() const
{
   return site_;
}


string SeekerParameterGroup::getDbTableName() const
{
   return dbTableName_;
}

string SeekerParameterGroup::getIdColNameInActsTable() const
{
   return idColNameInActsTable_;
}


void SeekerParameterGroup::addImmedCmdHelp(const string &cmdHelp)
{
   immedCmdHelp_.push_back(cmdHelp);
}

void SeekerParameterGroup::printAllImmedCmdHelp(ostream& os, 
						int indentLevel) const
{
   vector<string>::iterator it;
   for (it = immedCmdHelp_.begin(); it != immedCmdHelp_.end(); ++it)
   {
      indent(os, indentLevel);
      os << *it << endl;
   }
}

void SeekerParameterGroup::help(ostream& os) const {

   ParameterGroup::help(os);

   int indentLevel = 2;
   indent(os, indentLevel);
   os << "\nImmediate Commands:" << endl;

   indentLevel=3;
   printAllImmedCmdHelp(os, indentLevel);
}


SeekerParameterGroup::~SeekerParameterGroup()
{
}



// -----------------------------
// ----- database commands -----
// -----------------------------



class RecordParameters : public RecordInDatabase
{
public:
   RecordParameters(const string & tableName, 
		    const list<Parameter*> &paramList);
protected:
   virtual string stmt(MYSQL* db, const string & conjunction);
private:
   const list<Parameter*> & paramList_;
};

RecordParameters::RecordParameters(const string & tableName,
				   const list<Parameter*> &paramList)
   : RecordInDatabase(tableName),
     paramList_(paramList)
{
   AssertMsg(tableName != "", 
	     "Must set database table name to record parameters");
}



string RecordParameters::stmt(MYSQL* db,
			      const string & conjunction)
{
   // specify each parameter by name

   stringstream strm;

   for (list<Parameter*>::const_iterator params = paramList_.begin();
	params != paramList_.end(); ++params)
   {
      Parameter *param = *(params);
	
      strm << param->getName() << " = ";
	
      // put quotes around the string parameters
      if (dynamic_cast<AnyValueParameter<string>*>(param))
      {
	 strm << "'"
	      << param->convertToString(Parameter::CURRENT)
	      << "'";
      }
      else
      {
	 strm << param->convertToString(Parameter::CURRENT);
      }
      strm << " " << conjunction << " ";
	
   }

   // erase the last conjunction
   string statement(strm.str());
   statement.erase(statement.rfind(conjunction), conjunction.length());

   statement += ";";

   // cout << "SeekerParameterGroup statement is " << statement << endl;

   return(statement);

}


unsigned int SeekerParameterGroup::record(MYSQL* db)
{
   RecordParameters recordParameters(dbTableName_, getList());

   return recordParameters.record(db);
}



void SeekerParameterGroup::restoreGroup(MYSQL* db,
					const string &tableName,
					const string &tableIdName,
					uint32_t activityId) const
{
   const string methodName("SeekerParameterGroup::restoreGroup");


   if (!db)
   {
      throw SseException(methodName + " MySQL error: NULL db",
			 __FILE__, __LINE__, SSE_MSG_DBERR,
			 SEVERITY_ERROR);
   }
  
   stringstream sqlstmt;

   sqlstmt << "SELECT ";
   int count = 0;
   for (list<Parameter*>::const_iterator params = getList().begin();
	params != getList().end(); params++, count++ )
   {
      if (count > 0)
      {
	 sqlstmt << ", ";
      }
      sqlstmt << tableName << "." << (*params)->getName();
   }
  
   sqlstmt << " from Activities, " << tableName;
   sqlstmt << " where " << tableName << ".id = Activities."
	   << tableIdName 
	   << " and Activities.id = " << activityId << ";";

   MysqlQuery query(db);
   query.execute(sqlstmt.str(), count, __FILE__, __LINE__);

   MYSQL_ROW row(mysql_fetch_row(query.getResultSet()));
   if (row == 0)
   {
      stringstream strm;
      strm << methodName << " MySQL error: "
	   << "no parameters found for activity id " << activityId
	   << " for table: " << tableName
	   << endl;
      throw SseException(strm.str(), __FILE__, __LINE__, 
			 SSE_MSG_DBERR, SEVERITY_ERROR);
   }
    
   list<Parameter*>::const_iterator params;
   int column;
   string errorText;
   for (params = getList().begin(), column = 0;
	params != getList().end(); ++params, ++column)
   {
      const string valueString(MysqlQuery::getString(
	 row, column, __FILE__, __LINE__));
       
      // convert the value, check for errors
      if (! (*params)->convertFromString(
	 valueString, Parameter::CURRENT, errorText))
      {
	 stringstream strm;
	 strm << methodName << " MySQL error: "
	      << "incorrect parameter value '" << valueString 
	      << "' found in database: " 
	      << errorText << endl;
	 throw SseException(strm.str(), __FILE__, __LINE__, 
			    SSE_MSG_DBERR, SEVERITY_ERROR);
      }
   }

}

void SeekerParameterGroup::restore(MYSQL* db,
				   uint32_t activityId)
{
   Assert(dbTableName_ != "");
   Assert(idColNameInActsTable_ != "");

   restoreGroup(db, dbTableName_, idColNameInActsTable_, activityId);
}


// convert from parameter to signal type value 
Polarization SeekerParameterGroup::
convertPolarization(ChoiceParameter<string> &param)
{
    AssertMsg(param.isValid("right"), param.getName());
    AssertMsg(param.isValid("left"), param.getName());
    AssertMsg(param.isValid("both"), param.getName());
    AssertMsg(param.getNumberOfChoices() == 3, param.getName());

    Polarization pol = POL_BOTH; 
    if (param.getCurrent() == "left")
    {
	pol = POL_LEFTCIRCULAR;
    }
    else if (param.getCurrent() == "right")
    {
	pol = POL_RIGHTCIRCULAR;
    }

    return pol;
}

bool SeekerParameterGroup::
convertOnOffToBool(ChoiceParameter<string> &param)
{
    AssertMsg(param.getNumberOfChoices() == 2, param.getName());
    AssertMsg(param.isValid(ChoiceOn), param.getName());
    AssertMsg(param.isValid(ChoiceOff), param.getName());

    bool flag = false;
    if (param.getCurrent() == ChoiceOn)
    {
	flag = true;
    }

    return flag;
}
