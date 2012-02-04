#include "NFmiNetCDFVariable.h"
#include <boost/lexical_cast.hpp>

using namespace std;

#define kFloatMissing 32700.f

NFmiNetCDFVariable::NFmiNetCDFVariable()
 : itsValuePointer(-1)
{
}

NFmiNetCDFVariable::NFmiNetCDFVariable(NcVar *theParam) {
  Init(theParam);
}

bool NFmiNetCDFVariable::Init(NcVar *theVariable) {
  itsParam = theVariable;
  return true;
}

NFmiNetCDFVariable::~NFmiNetCDFVariable() {
  itsParam->sync();
}

string NFmiNetCDFVariable::Name() {
  return static_cast<string> (itsParam->name());
}

void NFmiNetCDFVariable::Name(string theName) {
  itsParam->rename(theName.c_str());
  itsParam->sync();
}

string NFmiNetCDFVariable::Unit() {
  return Att("units");
}

string NFmiNetCDFVariable::LongName() {
  return Att("long_name");
}

string NFmiNetCDFVariable::StandardName() {
  return Att("standard_name");
}

string NFmiNetCDFVariable::Axis() {
  return Att("axis");
}

string NFmiNetCDFVariable::Positive() {
  return Att("positive");
}

float NFmiNetCDFVariable::FillValue() {
  string fv = Att("_FillValue");

  float ret = kFloatMissing;

  if (!fv.empty())
    ret = boost::lexical_cast<float> (fv);

  return ret;
}

float NFmiNetCDFVariable::MissingValue() {
  string mv = Att("missing_value");

  float ret = kFloatMissing;

  if (!mv.empty())
    ret = boost::lexical_cast<float> (mv);

  return ret;
}


void NFmiNetCDFVariable::Unit(string theUnit) {
  itsParam->add_att("units", theUnit.c_str());
}

long NFmiNetCDFVariable::Size() {
  return itsParam->num_vals();
}

string NFmiNetCDFVariable::Att(string attName) {
  string ret("");

  NcAtt *att;

  for (unsigned short k = 0; k < itsParam->num_atts(); k++) {
    att = itsParam->get_att(k);

    if (static_cast<string> (att->name()) == attName) {
      ret = static_cast<string> (att->as_string(0));
      break;
    }
  }

  return ret;
}

vector<float> NFmiNetCDFVariable::Values(long timeIndex, long levelIndex) {

  vector<float> values;

  // Get ALL data

  if (timeIndex == -1 && levelIndex == -1) {
  	
  	values.resize(Size());
  	
  	if (itsParam->num_dims() == 1)
  	  itsParam->get(&values[0], Size());
  	  
  	else {
  	  itsParam->set_cur(0, 0, 0, 0);
      itsParam->get(&values[0], 1, 1, 256, 258);
  	}
  		  
    return values;
  }

  long xRec = 0;
  long yRec = 0;

  itsParam->set_cur(timeIndex, levelIndex, xRec, yRec);

  // Hard coded size

  values.resize(256*258);

  itsParam->get(&values[0], 1, 1, 256, 258);

  return values;
}

float NFmiNetCDFVariable::Value(int num) {
  return itsParam->as_float(num);
}

bool NFmiNetCDFVariable::HasDimension(NcDim *dim) const {
  bool ret = false;

  for (unsigned short i=0; i<itsParam->num_dims(); i++) {
    NcDim *tmpdim = itsParam->get_dim(i);

    if (static_cast<string> (tmpdim->name()) == dim->name())
      ret = true;
  }

  return ret;
}

void NFmiNetCDFVariable::ResetValue() {
  itsValuePointer = -1;
}

bool NFmiNetCDFVariable::NextValue() {
  return (++itsValuePointer < itsParam->num_vals());
}

float NFmiNetCDFVariable::Value() {
  return itsParam->as_float(itsValuePointer);
}

long NFmiNetCDFVariable::Index() {
  return itsValuePointer;
}

