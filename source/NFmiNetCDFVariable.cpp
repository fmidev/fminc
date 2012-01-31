#include "NFmiNetCDFVariable.h"

using namespace std;

NFmiNetCDFVariable::NFmiNetCDFVariable() {
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

vector<float> NFmiNetCDFVariable::Values() {
  vector<float> values;

  for (long i = 0; i < itsParam->num_vals(); i++) {
    values.push_back(itsParam->as_double(i));
  }

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
