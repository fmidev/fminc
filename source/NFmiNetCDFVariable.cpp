#include "NFmiNetCDFVariable.h"
#include <boost/lexical_cast.hpp>
#include <assert.h>

using namespace std;

#define kFloatMissing 32700.f

NFmiNetCDFVariable::NFmiNetCDFVariable()
 : itsValuePointer(-1),itsParam(0)
{
}

NFmiNetCDFVariable::NFmiNetCDFVariable(NcVar *theParam) : itsValuePointer(-1),itsParam(0) {
  Init(theParam);
}

bool NFmiNetCDFVariable::Init(NcVar *theVariable) {
  itsParam = theVariable;
  for (unsigned short i = 0; i < itsParam->num_dims(); i++) {
    NcDim *dim = itsParam->get_dim(i);

    string name = static_cast<string> (dim->name());

    if (name == "x" || name == "X" || name == "lon" || name == "longitude")
      itsXDim = dim;

    else if (name == "y" || name == "Y" || name == "lat" || name == "latitude")
      itsYDim = dim;

    else if (name == "time" || name == "rec" || dim->is_unlimited())
      itsTDim = dim;

    else if (name == "level" || name == "lev" || name == "depth" || name == "height")
      itsZDim = dim;

    itsDims.push_back(dim);
  }

  return true;
}

bool NFmiNetCDFVariable::Initialized() {
  return itsParam;
}

NFmiNetCDFVariable::~NFmiNetCDFVariable() {
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
  if (Initialized()){
    return itsParam->num_vals();
  }
  else {
    return 1;
  }
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

/*
 * Values()
 */

vector<float> NFmiNetCDFVariable::Values(long timeIndex, long levelIndex) {

  vector<float> values;
  vector<long> c;
  vector<long> d;

  long tDimSize = 1;
  long zDimSize = 1;

  // Get ALL data

  if (timeIndex == -1 && levelIndex == -1) {
  	
  	values.resize(Size());
  	
  	// Support only for one-dimension and four-dimension parameters

  	if (itsDims.size() == 1) {
  	  itsParam->get(&values[0], Size());
  	}
  	else {

  	  itsParam->set_cur(0, 0, 0, 0);

  	  vector<long> c;

  	  c.resize(itsDims.size()-2);

      for (unsigned short i = 0; i < itsDims.size(); i++) {
        if (itsDims[i] == itsTDim || itsDims[i] == itsZDim) {
          d[i] = 1;
          continue;
        }

        d[i] = itsDims[i]->size();
      }

  	  itsParam->get(&values[0], &d[0]);
  	}
  		  
    return values;
  }

  assert(timeIndex != -1);
  assert(levelIndex != -1);

  long xRec = 0;
  long yRec = 0;

  // Hard-coded order of dimensions: time, level, x, y

  /*
   * Set cursor to correct place. We need to have correct dimension value AND
   * correct ordering of dimensions.
   */

  c.resize(itsDims.size());
  d.resize(itsDims.size());

  for (unsigned short i = 0; i < itsDims.size(); i++) {

    if (itsDims[i] == itsTDim) {// || itsDims[i] == itsZDim) {
      c[i] = timeIndex;
      d[i] = tDimSize;
      continue;
    }
    else if (itsZDim->is_valid() && itsDims[i] == itsZDim) {
      c[i] = levelIndex;
      d[i] = zDimSize;
      continue;
    }
    else if (itsDims[i] == itsXDim) {
      c[i] = xRec;
      d[i] = itsDims[i]->size();
      continue;
    }
    else if (itsDims[i] == itsYDim) {
      c[i] = yRec;
      d[i] = itsDims[i]->size();
      continue;
    }
    else {
      cerr << "Invalid dimension: " << itsDims[i]->name() << endl;
      return values;
    }
  }

  itsParam->set_cur(&c[0]);

  /*
   * Get values.
   *
   * We always get only one slice, ie data from one time, one level and one x*y-area.
   */

  values.resize(itsXDim->size()*itsYDim->size());

  itsParam->get(&values[0], &d[0]);

  return values;
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

  float ret = kFloatMissing;

  if (itsValuePointer >= 0 && itsValuePointer < itsParam->num_vals())
    ret = itsParam->as_float(itsValuePointer);

  return ret;
}

long NFmiNetCDFVariable::Index() {
  return itsValuePointer;
}

void NFmiNetCDFVariable::Index(long theValuePointer) {
  itsValuePointer = theValuePointer;
}

