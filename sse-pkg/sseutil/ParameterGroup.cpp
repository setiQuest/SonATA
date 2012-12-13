/*******************************************************************************

 File:    ParameterGroup.cpp
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


#include "ParameterGroup.h" 
#include <fstream>
#include <sstream>
#include "Assert.h"

using namespace std;

ParameterGroup::ParameterGroup(const string &command)
   : cmdName_(command)
{
}

ParameterGroup::~ParameterGroup()
{
}

void ParameterGroup::addParam(Parameter &param)
{
   // make sure there's not already a parameter by this name
   for (list<Parameter*>::iterator params = paramList_.begin();
	params != paramList_.end(); params++)
   {
      AssertMsg((*params)->getName() != param.getName(),
		"Duplicate parameter name: " + cmdName_ + " " + param.getName());
   }

   paramList_.push_back(&param);  // note param is stored by address
}

string ParameterGroup::getCommand() const
{
   return cmdName_;
}

void ParameterGroup::setCommand(string command)
{
   cmdName_ = command;
}


// Save parameters to file. Return value is status feedback
// indicating error or success.
const char * ParameterGroup::save(const string& filename)
{
   stringstream feedbackStrm;
   ofstream os(filename.c_str(), ios::out|ios::app);
   if (!os)
   {
      feedbackStrm << "Error Parameters()::save() for " 
		   << cmdName_ << " failed to open file: "
		   << filename << endl;
      returnValue_ = feedbackStrm.str();
      return returnValue_.c_str();
   }

   const int PrintPrecision = 9;   // show N places after the decimal
   os.precision(PrintPrecision); 
   os.setf(std::ios::fixed);  // show all decimal places up to precision

   for (list<Parameter*>::iterator params = paramList_.begin();
	params != paramList_.end(); params++)
   {
      (*params)->writeValues(cmdName_, os);
   }
   os.close();

   feedbackStrm << cmdName_ << " parameters saved to file " << filename << endl;
   returnValue_ = feedbackStrm.str();

   return returnValue_.c_str();
}

// indent to the requested level
void ParameterGroup::indent(ostream &os, int level) const
{
   for (int i = 0; i < level; i++)
   {
      os << "  ";
   } 

}

void ParameterGroup::help(ostream& os) const
{
   indent(os, 1);
   os << "Command: \"" << cmdName_ << "\"" << endl;

   indent(os, 2);
   os <<"Parameters:    " << endl; 
     
   const int width = 26;
   for (list<Parameter*>::const_iterator params = paramList_.begin();
	params != paramList_.end(); params++)
   {
      indent(os, 3);
      os <<  left <<  std::setw(width) << (*params)->getName().c_str();
    
      os << (*params)->getHelp().c_str();

      const string units = (*params)->getUnits();
      if (units != "")
      {
	 os << " {";
	 os << units.c_str();
	 os << "}";
      } 
    
      os << endl;
   }
  
   indent(os, 2);
   os << "Parameter Commands:" << endl; 


   indent(os, 3);
   os << "default \t\t\t set all parameters to their default values" << endl; 
  
   indent(os, 3);
   os << "help \t\t\t print this help info" << endl; 

   indent(os, 3);
   os << "isvalid \t\t\t verify that all parameters are valid" << endl; 
  
   indent(os, 3);
   os << "save <filename> \t\t save parameters to file\n" << endl; 

   indent(os, 3);
   os << "set <parameter> <value> [<current, default, min, max>=current]" 
      << endl;

   indent(os, 3);
   os << "show [<parameter>=all] [<current, default, min, max>=current]" 
      << endl; 

}


const char * ParameterGroup::set(const string& paramName,
				 const string &value, 
				 const string& valueType)
{
   Parameter::Category category(Parameter::CURRENT);
   returnValue_ = "";
   string errorText;

   if (! convertValueTypeToCategory(valueType, category, errorText))
   {
      returnValue_ = errorText;
      return returnValue_.c_str();
   }

   bool found(false);
   returnValue_ = value;

   for (list<Parameter*>::iterator params = paramList_.begin();
	params != paramList_.end(); params++)
   {
      if ((*params)->getName() == paramName) 
      {
	 string errorText;
	 if (! (*params)->convertFromString(value, category, errorText))
	 {
	    returnValue_ = errorText;
	    return returnValue_.c_str();;
	 }

	 // if set fails, returns current value
	 returnValue_ = (*params)->convertToString(category);
	 found = true;
	 break;
      }
   }

   if (!found) 
   {
      stringstream strm;
      strm << "ParameterGroup::set() unknown parameter name: " << paramName 
	   << endl
	   << "\tfor supported parameters: see parameter help." << endl;
      returnValue_ = strm.str();
      return(returnValue_.c_str());
   }
  
   return(returnValue_.c_str());
  
}

const char * ParameterGroup::show(const string& paramName, 
				  const string& valueType)
{
   Parameter::Category category(Parameter::CURRENT);
   returnValue_ = "";
   string errorText;

   if (! convertValueTypeToCategory(valueType, category, errorText))
   {
      returnValue_ = errorText;
      return returnValue_.c_str();
   }

   if (paramName == "all")
   {
      stringstream strm;
      strm << *this;
      returnValue_ = strm.str();

      return returnValue_.c_str();
   }

   bool found(false);
   for (list<Parameter*>::const_iterator params = paramList_.begin();
	params != paramList_.end(); params++)
   {
      if ((*params)->getName() == paramName)
      {
	 returnValue_ = (*params)->convertToString(category);
	 found = true;
	 break;
      }
   }

   if (!found)
   {
      stringstream strm;
      strm << "ParameterGroup::show() unknown parameter name: " << paramName 
	   << endl
	   << "\t for supported parameters: see parameter help." << endl;
      returnValue_ = strm.str();
      return(returnValue_.c_str());
   }
  
   return(returnValue_.c_str());
}

// Returns true if conversion succeeds, and sets category accordingly.
// If the conversion fails, returns false, and stores an explanation in
// the errorText.
bool ParameterGroup::convertValueTypeToCategory(const string& valueType,
						Parameter::Category & category,
						string & errorText)
{
   errorText = "";

   if (valueType == "")
   {
      category = Parameter::CURRENT;
   }
   else if (valueType == "default")
   {
      category = Parameter::DEFAULT;
   }
   else if (valueType == "min")
   {
      category = Parameter::MIN;
   }
   else if (valueType == "max")
   {
      category = Parameter::MAX;
   }
   else if (valueType == "current")
   {
      category = Parameter::CURRENT;
   }
   else
   {
      stringstream strm;
      strm << "ParameterGroup:: unknown value type: " << valueType 
	   << endl
	   << "\tSupported types: default, min, max, current." << endl;
      errorText = strm.str();

      return false;
   }

   return true;
}

bool ParameterGroup::isValid() const 
{
   for (list<Parameter*>::const_iterator params = paramList_.begin();
	params != paramList_.end(); params++)
   {
      if (!(*params)->isValid())
      {
	 return(false);
      }
   }
   return(true);
}

const char * ParameterGroup::setDefault()
{
   for (list<Parameter*>::iterator params = paramList_.begin();
	params != paramList_.end(); params++)
   {
      (*params)->setDefault();
   }

   stringstream strm;
   strm << cmdName_ << " parameters set to default values" << endl;
   returnValue_ = strm.str();
   return returnValue_.c_str();
}



ostream& operator << (ostream &strm, const class ParameterGroup& paramGroup)
{
   strm << paramGroup.cmdName_ << " parameters: " << endl;

   const int width = 10;

   // TBD width hard coded, use list to find max strlen
   strm << "  "
	<< std::resetiosflags(std::ios::right)
	<< std::setw(width+10) << setiosflags(std::ios::left) << "Name"
	<< " "
	<< std::resetiosflags(std::ios::left)
	<< std::setw(width) << setiosflags(std::ios::right) << "Current" 
	<< " "
	<< std::setw(width) << setiosflags(std::ios::right) << "Default" 
	<< " "
	<< std::setw(width) << setiosflags(std::ios::right) << "Min" 
	<< " "
	<< std::setw(width) << setiosflags(std::ios::right) << "Max" 
	<< "  "
	<< std::resetiosflags(std::ios::right)
	<< std::setw(width) << setiosflags(std::ios::left) << "Units"
	<< endl;

   const int PrintPrecision = 3;    // show N places after the decimal
   strm.precision(PrintPrecision); 
   strm.setf(std::ios::fixed); // show all decimal places up to precision
   for (list<Parameter*>::const_iterator params = paramGroup.paramList_.begin();
	params != paramGroup.paramList_.end(); params++) 
   {
      strm << "  ";
      (*params)->print(strm);
   }
   return(strm);
}

// deletes all elements of paramList
void ParameterGroup::eraseParamList()
{
   paramList_.erase(paramList_.begin(), paramList_.end());
}

static bool compareParamsByName(Parameter *param1, Parameter *param2)
{
   return param1->getName() < param2->getName();
}

void ParameterGroup::sort()
{
   paramList_.sort(compareParamsByName);
}

const list<Parameter*>& ParameterGroup::getList() const
{
   return(paramList_);
}
