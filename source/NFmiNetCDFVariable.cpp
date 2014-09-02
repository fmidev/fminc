#include "NFmiNetCDFVariable.h"
#include <boost/lexical_cast.hpp>
#include <assert.h>

using namespace std;

#define kFloatMissing 32700.f

NFmiNetCDFVariable::NFmiNetCDFVariable()
 : itsValuePointer(-1), itsParam(0), itsTDim(0), itsZDim(0), itsXDim(0), itsYDim(0)
{
}

NFmiNetCDFVariable::NFmiNetCDFVariable(NcVar *theParam) : itsValuePointer(-1),itsParam(0), itsTDim(0), itsZDim(0), itsXDim(0), itsYDim(0) {
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

  // Get ALL data

  if (timeIndex == -1 && levelIndex == -1) {
  	
  	values.resize(Size());
  	
  	// Support only for one-dimension and four-dimension parameters

	assert(itsParam->num_dims() == 1);

  	if (itsParam->num_dims() == 1) {
  	  itsParam->get(&values[0], Size());
  	}
	
    return values;
  }

  assert(timeIndex != -1);

  /*
   * First set cursor to correct place.
   * We need to have correct dimension sizes AND correct ordering of dimensions.
   * This means that time dimension and level dimension are taken from timeIndex
   * and levelIndex, and x & y dimension are full sizes since we take the
   * whole "slice".
   */

  long num_dims = itsParam->num_dims();
  
  vector<long> cursor_position(num_dims), dimsizes(num_dims);

  cursor_position.resize(num_dims);
  dimsizes.resize(num_dims);

  for (unsigned short i = 0; i < num_dims; i++) {
    NcDim *dim = itsParam->get_dim(i);

    string dimname = static_cast<string> (dim->name());

	long index = 0;
	long dimsize = dim->size();

	if (dimname == itsTDim->name())
	{
		index = timeIndex;
		dimsize = 1;
	}
	else if (itsZDim && itsZDim->is_valid() && dimname == itsZDim->name())
	{
		index = levelIndex;
	}

	cursor_position[i] = index;
	dimsizes[i] = dimsize;
  }


  itsParam->set_cur(&cursor_position[0]);

  /*
   * Get values.
   *
   * 
   */

  values.resize(itsXDim->size()*itsYDim->size());

#ifdef DEBUG
  for (size_t i = 0; i < cursor_position.size(); i++) cout << "cursor position " << i << ": " << cursor_position[i] << endl;
  for (size_t i = 0; i < dimsizes.size(); i++) cout << "dimension sizes " << i << ": " << dimsizes[i] << endl;
#endif
  
  itsParam->get(&values[0], &dimsizes[0]);

#ifdef DEBUG
  double min=1e38, max=-1e38, sum=0;
  long count = 0;
  for (size_t i = 0; i < values.size(); i++)
  {
	  if (values[i] < min) min = values[i];
	  if (values[i] > max) max = values[i];
	  sum += values[i];
	  count++;
  }
  cout << "min: " << min << " max: " << max << " mean: " << (sum/static_cast<double>(count)) << endl;
#endif
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

NcDim* NFmiNetCDFVariable::Dimension(int num)
{
  return itsParam->get_dim(num);
}

int NFmiNetCDFVariable::SizeDimensions()
{
	return itsParam->num_dims();
}