/*******************************************************************************

 File:    ParameterImpl.h
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


#ifndef ParameterImpl_H
#define ParameterImpl_H

#include "Parameter.h"
#include <sstream>
#include <iomanip>

template<typename T>
class ParameterImpl : public Parameter
{
 public:
   ParameterImpl(const string &paramName, const string &paramUnits,
		 const string &paramHelp, T defaultValue)
      :
      name_(paramName), units_(paramUnits), help_(paramHelp), default_(defaultValue),
      current_(defaultValue)
      {}

   virtual ~ParameterImpl() {}

   virtual string getName() const
      {return name_;}

   virtual string getUnits() const
      {return units_;}

   virtual string getHelp() const 
      {return help_;}

   virtual const T& getValue(Category category = CURRENT) const = 0;
   virtual bool setValue(T value, string & errorText, Category category = CURRENT) = 0;
    
   virtual bool convertFromString(const string &value, Category category, string & errorText);
   virtual string convertToString(Category category) const;
    
   virtual const T& getDefault() const   {return default_;}
   virtual bool setDefault()
   {
      string errorText;
      return(setValue(default_, errorText, CURRENT));
   }
   virtual bool setDefault(T value)
   {
      string errorText;
      return(setValue(value, errorText, DEFAULT));
   }
    
   virtual const T&  getCurrent() const   {return current_;}
   virtual bool setCurrent(T value)
   {
      string errorText;
      return(setValue(value, errorText, CURRENT));
   }
    
   virtual bool isValid() const = 0;
   virtual bool isValid(T value) const = 0;

   virtual void printHeaderForInvalidParam(ostream & errorStrm) const;

   virtual void writeValues(const string &command, ostream &os) const;

   virtual void printValidSelections(ostream &strm, int width) const
   {
      // let subclasses override to print min/max ranges, choices, etc.
   }
    
   friend ostream& operator << (ostream &strm, const ParameterImpl<T>& param)
   {
      const int width = 10;
      
      // TBD width hard coded, use list to find max strlen
      strm << std::resetiosflags(std::ios::adjustfield)
	 
	 // print param name
	   << std::setiosflags(std::ios::left) 
	   << std::setw(width+10) << param.getName() << " "
	 
	 // print current & default param values
	   << std::resetiosflags(std::ios::adjustfield)
	   << std::setiosflags(std::ios::right)
	   << std::setw(width) << param.getCurrent() << " "
	   << std::setw(width) << param.getDefault() << " ";
      
      param.printValidSelections(strm, width);
      
      // print units
      strm << std::resetiosflags(std::ios::adjustfield)
	   << std::setiosflags(std::ios::left) 
	   << " "
	   << std::setw(width) <<  param.getUnits()
	   << std::resetiosflags(std::ios::adjustfield);
      
      return(strm);
   }

 protected:
   void setInternalDefault(T value) {default_ = value;};
   void setInternalCurrent(T value) {current_ = value;};

 private:

   string  name_;
   string  units_;
   string  help_;
   T default_;
   T current_;


/* default copy constructor and assignment operator should be safe */

};


template<class T> 
void ParameterImpl<T>::printHeaderForInvalidParam(ostream & errorStrm) const
{
   errorStrm << "Invalid setting for parameter: \""
	     << getName() << "\": " << getHelp() 
	     << " {" << getUnits() << "}" << std::endl;
}


template<class T> 
bool ParameterImpl<T>::convertFromString(const string &value, Category category,
					 string & errorText)
{
   std::stringstream errorStrm;
   
   std::istringstream convertStrm(value);
   T convertedValue;
   convertStrm >> convertedValue;  
   if (! convertStrm)
   {
      printHeaderForInvalidParam(errorStrm);
      errorStrm << "  value: " << value << std::endl;
      errorText = errorStrm.str();

      return false;
   }

   return setValue(convertedValue, errorText, category);

}

template<class T>
string ParameterImpl<T>::convertToString(Category category) const
{
   std::stringstream strm;

   const int PrintPrecision = 9;   // show N places after the decimal
   strm.precision(PrintPrecision); 
   strm.setf(std::ios::fixed);  // show all decimal places up to precision

   strm << getValue(category);

   return(strm.str());
}

template<class T>
void  ParameterImpl<T>::writeValues(const string &command, ostream &os) const
{
   os << command << " set " << getName() << " " << getDefault() 
      << " default " << std::endl;
   os << command << " set " << getName() << " " << getCurrent() 
      << " current " << std::endl;
}


template<>
bool ParameterImpl<string>::convertFromString(const string &val, Category category,
					      string & errorText);

template<>
void ParameterImpl<string>::writeValues(const string &command, ostream &os) const;

#endif // ParameterImpl_H
