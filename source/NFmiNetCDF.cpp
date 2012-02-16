#include "NFmiNetCDF.h"
#include <boost/lexical_cast.hpp>
#include <boost/filesystem.hpp>
/*#include "boost/date_time/gregorian/gregorian.hpp"
#include "boost/date_time/posix_time/posix_time.hpp"*/
#include <stdexcept>
#include <sstream>
#include <fstream>
#include <iomanip>
#include <ctime>
#include <algorithm>
#include <cmath>

using namespace std;

const float kFloatMissing = 32700.f;
const float MAX_COORDINATE_RESOLUTION_ERROR = 1e-4;

NFmiNetCDF::NFmiNetCDF()
 : itsTDim(0)
 , itsXDim(0)
 , itsYDim(0)
 , itsZDim(0)
 , itsProjection("latitude_longitude")
 , itsProcess(0)
 , itsCentre(86)
 , itsAnalysisTime("")
 , itsX0(kFloatMissing)
 , itsY0(kFloatMissing)
 , itsStep(0)
 , itsXFlip(false)
 , itsYFlip(false)
 , itsXResolution(0)
 , itsYResolution(0)
{
}

NFmiNetCDF::NFmiNetCDF(const string &theInfile)
 : itsTDim(0)
 , itsXDim(0)
 , itsYDim(0)
 , itsZDim(0)
 , itsProjection("latitude_longitude")
 , itsProcess(0)
 , itsCentre(86)
 , itsAnalysisTime("")
 , itsX0(kFloatMissing)
 , itsY0(kFloatMissing)
 , itsStep(0)
 , itsXFlip(false)
 , itsYFlip(false)
 , itsXResolution(0)
 , itsYResolution(0)
{
  Read(theInfile);
}


NFmiNetCDF::~NFmiNetCDF() {
}

bool NFmiNetCDF::Read(const string &theInfile) {

  dataFile = new NcFile(theInfile.c_str(), NcFile::ReadOnly);

  if (!dataFile->is_valid()) {
    return false;
  }

  /*
   * Read metadata from file.
   */

  if (!ReadAttributes())
    return false;

  if (!ReadDimensions())
    return false;

  if (!ReadVariables())
    return false;

  if (itsParameters.size() == 0) {
    return false;
  }

  // Set initial time and level values since they are easily forgotten
  
  ResetTime();
  NextTime();
  
  ResetLevel();
  NextLevel();

  return true;
}

bool NFmiNetCDF::ReadDimensions() {

  /*
   * Read dimensions from netcdf
   *
   * Usually we have three to four dimensions:
   * - x coordinate (grid point x or longitude)
   * - y coordinate (grid point y or latitude)
   * - time (often referred as unlimited dimension in netcdf terms)
   * - level
   *
   * No support for record-based dimensions (yet).
   */

  for (int i = 0; i < dataFile->num_dims() ; i++) {

    NcDim *dim = dataFile->get_dim(i);

    string name = dim->name();

    // NetCDF conventions suggest x coordinate to be named "lon"

    if (name == "x" || name == "X" || name == "lon" || name == "longitude") {
      itsXDim = dim;
    }

    // NetCDF conventions suggest y coordinate to be named "lat"

    if (name == "y" || name == "Y" || name == "lat" || name == "latitude") {
      itsYDim = dim;
    }

    if (name == "time" || name == "rec" || dim->is_unlimited()) {
      itsTDim = dim;
    }

    if (name == "level" || name == "lev" || name == "depth") {
      itsZDim = dim;
    }
  }

  // Level and time dimensions must be present

  if (!itsZDim || !itsZDim->is_valid()) {
    return false;
  }

  if (!itsTDim || !itsTDim->is_valid()) {
    return false;
  }

  return true;
}

bool NFmiNetCDF::ReadVariables() {

  /*
   * Read variables from netcdf
   *
   * Each dimension in the file has a corresponding variable,
   * also each actual data parameter is presented as a variable.
   */

  for (int i = 0; i < dataFile->num_vars() ; i++) {

    NcVar *var = dataFile->get_var(i);
    NcAtt *att;

    string name = var->name();

    if (static_cast<string> (var->name()) == static_cast<string> (itsZDim->name())) {

      /*
       * Assume level variable name equals to level dimension name. If it does not, how
       * do we know which variable is level variable?
       *
       * ( maybe when var dimension name == level dimension name and number of variable dimension is one ??
       * should be tested )
       */

      // This is level variable, read values

      itsZ.Init(var);

      // Check for attributes

      for (unsigned short k = 0; k< var->num_atts(); k++) {
        att = var->get_att(k);

        string name = static_cast<string> (att->name());

        if (name == "positive") {
          if (static_cast<string> (att->as_string(0)) == "down") {
            itsZIsPositive = false;
          } else {
            itsZIsPositive = true;
          }
        } else if (name == "units") {
          itsZUnit = att->as_string(0);
        }
      }

      continue;
    }
    else if (static_cast<string> (var->name()) == static_cast<string> (itsXDim->name())) {

      // X-coordinate

      itsX.Init(var);

      vector<float> tmp = itsX.Values();

      if (itsXFlip)
        reverse(tmp.begin(), tmp.end());

      if (tmp.size() > 1) {
      	
      	// Check resolution 
      	
      	float resolution = 0;
      	float prevResolution = 0;
      	
        float prevX = tmp[0];
        
      	for (unsigned int k = 1; k < tmp.size(); k++) {
          resolution = tmp[k] - prevX;
          
          if (k == 1)
          	prevResolution = resolution;
          
          if (abs(resolution-prevResolution) > MAX_COORDINATE_RESOLUTION_ERROR) {
            cerr << "X dimension resolution is not constant, diff: " << (prevResolution-resolution) << endl;
            return false;
          }
          
          prevResolution = resolution;
          prevX = tmp[k]; 
      	}
      	
      	itsXResolution = prevResolution;
      }
      
      continue;
    }
    else if (static_cast<string> (var->name()) == static_cast<string> (itsYDim->name())) {

      // Y-coordinate

      itsY.Init(var);

      vector<float> tmp = itsY.Values();

      if (itsYFlip)
        reverse(tmp.begin(), tmp.end());

      if (tmp.size() > 1) {
      	
      	// Check resolution 
      	
      	float resolution = 0;
      	float prevResolution = 0;
      	
        float prevY = tmp[0];
        
      	for (unsigned int k = 1; k < tmp.size(); k++) {
          resolution = tmp[k] - prevY;
          
          if (k == 1)
          	prevResolution = resolution;
          
          if (abs(resolution-prevResolution) > MAX_COORDINATE_RESOLUTION_ERROR) {
            cerr << "Y dimension resolution is not constant, diff: " << (prevResolution-resolution) << endl;
            return false;
          }
          
          prevResolution = resolution;
          prevY = tmp[k]; 
      	}
      	
      	itsYResolution = prevResolution;
      }
      
      continue;
    }
    else if (static_cast<string> (var->name()) == static_cast<string> (itsTDim->name())) {

      /*
       * time
       *
       * Time in NetCDF is annoingly stupid: instead of using full timestamps or analysistime
       * + offset, an arbitrary point of time in the past is chosen and all time is an offset
       * to that time.
       */
/*
      for (short k=0; k<var->num_atts(); k++) {
        att = var->get_att(k);

        if (static_cast<string> (att->name()) == "units")
          itsTUnit = att->as_string(0);
      }

      if (itsTUnit.empty() || itsTUnit != "hours since 1900-01-01 00:00:00") {

        return false;
      }

      for (unsigned short k = 0; k < var->num_vals(); k++) {

        double t = var->as_double(k);

        itsTimes.push_back(t);

       // itsTimes.push_back(basehours.hours() - t);
      }

      if (itsTimes.size() > 1)
        itsStep = itsTimes[1] - itsTimes[0];
*/

      itsT.Init(var);

      continue;
    }

    /*
     *  Search for projection information. CF conforming file has a variable
     *  that has attribute "grid_mapping_name".
     */

    bool foundProjection = false;

    for (short k=0; k < var->num_atts() && !foundProjection; k++) {
      att = var->get_att(k);

      if (static_cast<string> (att->name()) == "grid_mapping_name") {
        itsProjection = att->as_string(0);
        foundProjection = true;
        continue;
      }
    }

    NFmiNetCDFVariable param (var);

    itsParameters.push_back(param);

  }

  return true;
}

bool NFmiNetCDF::ReadAttributes() {

  /*
   * Read global attributes from netcdf
   *
   * Attributes are global metadata definitions in the netcdf file.
   *
   * Two attributes are critical: producer id and analysis time. Both can
   * be overwritten by command line options.
   */

  for (int i = 0; i < dataFile->num_atts() ; i++) {

    NcAtt *att = dataFile->get_att(i);

    string name = att->name();

    if (itsProcess != 0 && name == "producer" && att->type() == ncInt) {
      itsProcess = static_cast<unsigned int> (att->as_long(0));
    }
    else if (name == "Conventions" && att->type() == ncChar) {
      itsConvention = att->as_string(0);
    }
    else if (!itsAnalysisTime.empty() && name == "AnalysisTime" && att->type() == ncChar) {
      itsAnalysisTime = att->as_string(0);
    }
  }

  return true;
}

        // Flatten 3D array to 1D array
/*
        for (unsigned int x = 0; x < itsX.Size(); x++) {
          for (unsigned int y = 0; y < itsY.Size(); y++) {
            float value = vals->as_float(j*(itsX.Size()*itsY.Size()) + y * itsX.Size() + x);

            if (value == theMissingValue)
              value = kFloatMissing;

            slice.push_back(value);
          }
        }
*/
  
string NFmiNetCDF::AnalysisTime() {
  return itsAnalysisTime;
}

void NFmiNetCDF::AnalysisTime(string theAnalysisTime) {
  itsAnalysisTime = theAnalysisTime;
}


unsigned int NFmiNetCDF::Process() {
  return itsProcess;
}

void NFmiNetCDF::Process(unsigned int theProcess) {
  itsProcess = theProcess;
}

long int NFmiNetCDF::SizeX() {
  return itsX.Size();
}

long int NFmiNetCDF::SizeY() {
  return itsY.Size();
}

long int NFmiNetCDF::SizeZ() {
  return itsZ.Size();
}

long int NFmiNetCDF::SizeT() {
  return itsT.Size();
}

long int NFmiNetCDF::SizeParams() {
  return itsParameters.size();
}

bool NFmiNetCDF::IsConvention() {
  return (!itsConvention.empty());
}

string NFmiNetCDF::Convention() {
  return itsConvention;
}

void NFmiNetCDF::FirstParam() {
  itsParamIterator = itsParameters.begin();
}

bool NFmiNetCDF::NextParam() {
  return (++itsParamIterator < itsParameters.end());
}

NFmiNetCDFVariable NFmiNetCDF::Param() {
  return *itsParamIterator;
}

void NFmiNetCDF::ResetTime() {
  itsT.ResetValue();
}

bool NFmiNetCDF::NextTime() {
  return itsT.NextValue();
}

float NFmiNetCDF::Time() {
  return itsT.Value();
}

void NFmiNetCDF::ResetLevel() {
  itsZ.ResetValue();
}

bool NFmiNetCDF::NextLevel() {
  return itsZ.NextValue();
}

float NFmiNetCDF::Level() {
  return itsZ.Value();
}

long NFmiNetCDF::TimeIndex() {
  return itsT.Index();
}

long NFmiNetCDF::LevelIndex() {
  return itsZ.Index();
}

void NFmiNetCDF::ResetX() {
  itsX.ResetValue();
}

bool NFmiNetCDF::NextX() {
  return itsX.NextValue();
}

float NFmiNetCDF::X() {
  return itsX.Value();
}

long NFmiNetCDF::XIndex() {
  return itsX.Index();
}

void NFmiNetCDF::ResetY() {
  itsY.ResetValue();
}

bool NFmiNetCDF::NextY() {
  return itsY.NextValue();
}

float NFmiNetCDF::Y() {
  return itsY.Value();
}

long NFmiNetCDF::YIndex() {
  return itsY.Index();
}
float NFmiNetCDF::X0() {

  if (itsX0 == kFloatMissing) {
    float min = kFloatMissing;

    vector<float> values = itsX.Values();

    for (unsigned int i=0; i < values.size(); i++) {
      if (min == kFloatMissing || values[i] < min)
        min = values[i];
    }

    itsX0 = min;
  }

  return itsX0;

}

float NFmiNetCDF::Y0() {

  if (itsY0 == kFloatMissing) {
    float min = kFloatMissing;

    vector<float> values = itsY.Values();

    for (unsigned int i=0; i < values.size(); i++) {
      if (min == kFloatMissing || values[i] < min)
        min = values[i];
    }

    itsY0 = min;
  }

  return itsY0;

}

float NFmiNetCDF::ValueT(long num) {
  itsT.Index(num);
  return itsT.Value();
}

float NFmiNetCDF::ValueZ(long num) {
  itsZ.Index(num);
  return itsZ.Value();
}


std::string NFmiNetCDF::Projection() {
  return itsProjection;
}

std::vector<float> NFmiNetCDF::Values(std::string theParameter) {

  std::vector<float> values;

  for (unsigned int i = 0; i  < itsParameters.size(); i++) {
    if (itsParameters[i].Name() == theParameter)
      values = itsParameters[i].Values(TimeIndex(), LevelIndex());
  }

  return values;
}

std::vector<float> NFmiNetCDF::Values() {

  std::vector<float> values;
  
  values = Param().Values(TimeIndex(), LevelIndex());

  return values;
}



bool NFmiNetCDF::HasDimension(const NFmiNetCDFVariable &var, const string &dim) {

  bool ret = false;

  if (dim == "z") {
    ret = var.HasDimension(itsZDim);
  }
  else if (dim == "t") {
    ret = var.HasDimension(itsTDim);
  }
  else if (dim == "x") {
    ret = var.HasDimension(itsXDim);
  }
  else if (dim == "y") {
    ret = var.HasDimension(itsYDim);
  }

  return ret;
}

bool NFmiNetCDF::WriteSliceToCSV(const string &theFileName) {
	
	ofstream theOutFile;
  theOutFile.open(theFileName.c_str());
  
  vector<float> Xs = itsX.Values();

  if (itsXFlip)
    reverse(Xs.begin(), Xs.end());

  vector<float> Ys = itsY.Values();

  if (itsYFlip)
    reverse(Ys.begin(), Ys.end());

  vector<float> values = Values();

  for (unsigned int y = 0; y < Ys.size(); y++) {
   for (unsigned int x = 0; x < Xs.size(); x++) {
  
  		theOutFile << Xs[x] << "," << Ys[y] << "," << values[y*Xs.size()+x] << endl;
  	}
  }
  
  theOutFile.close();
	
	return true;
}

/*
 * WriteSlice(string)
 *
 * Write the current slice to file. Slice means the level/parameter combination thats
 * defined by the iterators.
 *
 * If iterators are not valid, WriteSlice() returns false.
 * If a parameter has no z dimension, WriteSlice() will ignore z iterator value.
 *
 */

bool NFmiNetCDF::WriteSlice(const std::string &theFileName) {

  NcDim *theXDim, *theYDim, *theZDim, *theTDim;
  NcVar *theXVar, *theYVar, *theZVar, *theTVar, *theParVar;

  boost::filesystem::path f(theFileName);
  string dir  = f.parent_path().string();

  if (!boost::filesystem::exists(dir)) {
    if (!boost::filesystem::create_directories(dir)) {
      cerr << "Unable to create directory " << dir << endl;
      return false;
    }
  }

  NcFile theOutFile(theFileName.c_str(), NcFile::Replace);

  if(!theOutFile.is_valid()) {
    cerr << "Unable to create file " << theFileName << endl;
    return false;
  }

  // Dimensions

  if (!(theXDim = theOutFile.add_dim(itsXDim->name(), SizeX())))
    return false;

  if (!(theYDim = theOutFile.add_dim(itsYDim->name(), SizeY())))
    return false;

  /*
   * Add z dimension even if original data has no z dimension. In that
   * case set level value = 0.
   */

  if (!(theZDim = theOutFile.add_dim(itsZDim->name(), 1)))
    return false;

  // Our unlimited dimension

  if (!(theTDim = theOutFile.add_dim(itsTDim->name())))
    return false;

  // Variables

  // x

  if (!(theXVar = theOutFile.add_var(itsX.Name().c_str(), ncFloat, theXDim)))
    return false;

  if (!itsX.Unit().empty()) {
    if (!theXVar->add_att("units", itsX.Unit().c_str()))
      return false;
  }

  if (!itsX.LongName().empty()) {
    if (!theXVar->add_att("long_name", itsX.LongName().c_str()))
      return false;
  }

  if (!itsX.StandardName().empty()) {
    if (!theXVar->add_att("standard_name", itsX.StandardName().c_str()))
      return false;
  }

  if (!itsX.Axis().empty()) {
    if (!theXVar->add_att("axis", itsX.Axis().c_str()))
      return false;
  }

  // Put coordinate data

  // Flip x values if requested

  vector<float> xValues = itsX.Values();

  if (itsXFlip)
    reverse(xValues.begin(), xValues.end());

  if (!theXVar->put(&xValues[0], SizeX()))
    return false;

  // y

  if (!(theYVar = theOutFile.add_var(itsY.Name().c_str(), ncFloat, theYDim)))
    return false;

  if (!itsY.Unit().empty()) {
    if (!theYVar->add_att("units", itsY.Unit().c_str()))
      return false;
  }

  if (!itsY.LongName().empty()) {
    if (!theYVar->add_att("long_name", itsY.LongName().c_str()))
      return false;
  }

  if (!itsY.StandardName().empty()) {
    if (!theYVar->add_att("standard_name", itsY.StandardName().c_str()))
        return false;
  }

  if (!itsY.Axis().empty()) {
    if (!theYVar->add_att("axis", itsY.Axis().c_str()))
      return false;
  }

  // Put coordinate data

  // Flip y values if requested

  vector<float> yValues = itsY.Values();

  if (itsYFlip)
    reverse(yValues.begin(), yValues.end());

  if (!theYVar->put(&yValues[0], SizeY()))
    return false;

  // z

  if (!(theZVar = theOutFile.add_var(itsZDim->name(), ncFloat, theZDim)))
    return false;

  if (!itsZ.Unit().empty()) {
    if (!theZVar->add_att("units", itsZ.Unit().c_str()))
      return false;
  }

  if (!itsZ.LongName().empty()) {
    if (!theZVar->add_att("long_name", itsZ.LongName().c_str()))
      return false;
  }

  if (!itsZ.StandardName().empty()) {
    if (!theZVar->add_att("standard_name", itsZ.StandardName().c_str()))
        return false;
  }

  if (!itsZ.Axis().empty()) {
    if (!theZVar->add_att("axis", itsZ.Axis().c_str()))
      return false;
  }

  if (!itsZ.Positive().empty()) {
    if (!theZVar->add_att("positive", itsZ.Positive().c_str()))
      return false;
  }

  /*
   * Set z value. If current variable has no z dimension, Level() will return
   * kFloatMissing. In that case we set level = 0.
   */

  float zValue = Level();

  if (zValue == kFloatMissing)
    zValue = 0;

  if (!theZVar->put(&zValue, 1))
    return false;

  // t

  if (!(theTVar = theOutFile.add_var(itsTDim->name(), ncFloat, theTDim)))
    return false;

  if (!itsT.Unit().empty()) {
    if (!theTVar->add_att("units", itsT.Unit().c_str()))
      return false;
  }

  if (!itsT.LongName().empty()) {
    if (!theTVar->add_att("long_name", itsT.LongName().c_str()))
      return false;
  }

  if (!itsT.StandardName().empty()) {
    if (!theTVar->add_att("standard_name", itsT.StandardName().c_str()))
      return false;
  }

  if (!itsT.Axis().empty()) {
    if (!theTVar->add_att("axis", itsT.Axis().c_str()))
      return false;
  }

  float tValue = Time();

  if (!theTVar->put(&tValue, 1))
    return false;

  // parameter

  // TODO: Check which dimensions are actually defined for a variable and their ordering

  if (!(theParVar = theOutFile.add_var(Param().Name().c_str(), ncFloat, theTDim, theZDim, theXDim, theYDim)))
    return false;

  if (!Param().Unit().empty()) {
    if (!theParVar->add_att("units", Param().Unit().c_str()))
      return false;
  }

  if (!Param().LongName().empty()) {
    if (!theParVar->add_att("long_name", Param().LongName().c_str()))
      return false;
  }

  if (!Param().StandardName().empty()) {
    if (!theParVar->add_att("standard_name", Param().StandardName().c_str()))
      return false;
  }

  if (!Param().Axis().empty()) {
    if (!theParVar->add_att("axis", Param().Axis().c_str()))
      return false;
  }

  // Fill value should be the same as missing value

  if ((Param().FillValue() == Param().MissingValue()) != kFloatMissing) {
    if (!theParVar->add_att("_FillValue", Param().FillValue()))
        return false;
    if (!theParVar->add_att("missing_value", Param().MissingValue()))
      return false;
  }

  // First level is one (not zero)

  vector<float> data = Values();

  if (!theParVar->put(&data[0], 1, 1, SizeX(), SizeY()))
    return false;

  // Global attributes

  if (!theOutFile.add_att("generating_process_identifier", static_cast<long> (itsProcess)))
    return false;

  if (!itsConvention.empty()) {
    if (!theOutFile.add_att("Conventions", itsConvention.c_str()))
      return false;
  }

  time_t now;
  time(&now);

  string datetime = ctime(&now);

  // ctime adds implicit newline

  datetime.erase(remove(datetime.begin(), datetime.end(), '\n'), datetime.end());

  if (!theOutFile.add_att("file_creation_time", datetime.c_str()))
    return false;

  if (!itsInstitution.empty()) {
      if (!theOutFile.add_att("institution", itsInstitution.c_str()))
        return false;
  }

  if (!theOutFile.add_att("distributor", "Finnish Meteorological Institute"))
    return false;

  if (!theOutFile.add_att("analysis_time", itsAnalysisTime.c_str()))
    return false;

  theOutFile.sync();
  theOutFile.close();

  return true;
}

/*
 * FlipX() and FlipX(bool)
 * FlipY() and FlipY(bool)
 *
 * If itsXFlip is set, when writing slice to NetCDF of CSV file the values of X axis
 * are flipped, ie. from 1 2 3 we have 3 2 1.
 *
 * Same applies for Y axis.
 */

bool NFmiNetCDF::FlipX() {
  return itsXFlip;
}

void NFmiNetCDF::FlipX(bool theXFlip) {
  itsXFlip = theXFlip;
}

bool NFmiNetCDF::FlipY() {
  return itsYFlip;
}

void NFmiNetCDF::FlipY(bool theYFlip) {
  itsYFlip = theYFlip;
}

float NFmiNetCDF::XResolution() {
	return itsXResolution;
}

float NFmiNetCDF::YResolution() {
	return itsYResolution;
}
