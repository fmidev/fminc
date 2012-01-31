#include "NFmiNetCDF.h"
#include <boost/lexical_cast.hpp>
#include "boost/date_time/gregorian/gregorian.hpp"
#include "boost/date_time/posix_time/posix_time.hpp"
#include <stdexcept>
#include <sstream>
#include <iomanip>
#include <ctime>
#include <algorithm>

using namespace std;

#define kFloatMissing 32700.f

NFmiNetCDF::NFmiNetCDF()
 : t(0)
 , x(0)
 , y(0)
 , z(0)
 , itsProjection("latitude_longitude")
 , itsProcess(0)
 , itsCentre(86)
 , itsAnalysisTime("")
 , itsStep(0)
{
}

NFmiNetCDF::NFmiNetCDF(const string &theInfile)
 : t(0)
 , x(0)
 , y(0)
 , z(0)
 , itsProjection("latitude_longitude")
 , itsProcess(0)
 , itsCentre(86)
 , itsAnalysisTime("")
 , itsStep(0)
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

  if (itsParameters.size()) {
    return false;
  }

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
      x = dim;
    }

    // NetCDF conventions suggest y coordinate to be named "lat"

    if (name == "y" || name == "Y" || name == "lat" || name == "latitude") {
      y = dim;
    }

    if (name == "time" || name == "rec" || dim->is_unlimited()) {
      t = dim;
    }

    if (name == "level" || name == "lev" || name == "depth") {
      z = dim;
    }
  }

  // Level and time dimensions must be present

  if (!z || !z->is_valid()) {
    return false;
  }

  if (!t || !t->is_valid()) {
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

    if (static_cast<string> (var->name()) == static_cast<string> (z->name())) {

      /*
       * Assume level variable name equals to level dimension name. If it does not, how
       * do we know which variable is level variable?
       *
       * ( maybe when var dimension name == level dimension name and number of variable dimension is one ??
       * should be tested )
       */

      // This is level variable, read values

      itsZ.Init(var);

      for (unsigned short k = 0; k < var->num_vals(); k++) {
        //itsLevels.push_back(var->as_double(k)); // Note! No data type check (will segfault if level data type is not double)
      }

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
    else if (static_cast<string> (var->name()) == static_cast<string> (x->name())) {
/*
      // X-coordinate

      // Make sure unit is 'degrees_north'

      string units;

      for (short k=0; k<var->num_atts(); k++) {
        att = var->get_att(k);

        if (static_cast<string> (att->name()) == "units")
          units = att->as_string(0);
      }

      if (units.empty() || units != "degrees_east") {
        cerr << "Unit for variable " << static_cast<string> (var->name()) << " is not 'degrees_east'" << endl;
        return false;
      }

      for (unsigned short k = 0; k < var->num_vals(); k++) {
        itsXs.push_back(var->as_double(k));
      }
*/
      itsX.Init(var);

      vector<float> tmp = itsX.Values();

      sort (tmp.begin(), tmp.end());

      itsX0 = tmp[0];
      itsX1 = tmp[tmp.size()-1];

      continue;
    }
    else if (static_cast<string> (var->name()) == static_cast<string> (y->name())) {
/*
      // Y-coordinate

      // Make sure unit is 'degrees_north'

      string units;

      for (short k=0; k<var->num_atts(); k++) {
        att = var->get_att(k);

        if (static_cast<string> (att->name()) == "units")
          units = att->as_string(0);
      }

      if (units.empty() || units != "degrees_north") {
        cerr << "Unit for variable " << static_cast<string> (var->name()) << " is not 'degrees_north'" << endl;
        return false;
      }

      for (unsigned short k = 0; k < var->num_vals(); k++) {
        itsYs.push_back(var->as_double(k));
      }

      if (itsYs.size() > 0) {
        vector<double> tmp = itsYs;

        sort (tmp.begin(), tmp.end());

        itsBottomLeftLatitude = tmp[0];
        itsTopRightLatitude = tmp[tmp.size()-1];
      }
*/
      itsY.Init(var);

      vector<float> tmp = itsY.Values();

      sort (tmp.begin(), tmp.end());

      itsY0 = tmp[0];
      itsY1 = tmp[tmp.size()-1];

      continue;
    }
    else if (static_cast<string> (var->name()) == static_cast<string> (t->name())) {

      /*
       * time
       *
       * Time in NetCDF is annoingly stupid: instead of using full timestamps or analysistime
       * + offset, an arbitrary point of time in the past is chosen and all time is an offset
       * to that time.        *
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

/*
 * Split()
 *
 * This function will split one NetCDF file to multiple files, one per
 * time/parameter/level combination.
 *
 * This function will also modify, if necessary, NetCDF variable, dimension and/or
 * attribute names so that they are consistent with in FMI production system.
 *
 */

vector<string> NFmiNetCDF::Split() {

  vector<string> files;

  NcVar *var;

  for (unsigned int h = 0; h < itsT.Size(); h++) {

  //  double time = itsTimes[h];

    //info.fcst_per = time;
/*
    for (unsigned int i = 0; i < itsParameters.size(); i++) {

      NFmiNetCDFVariable p = itsParameters[i];

      string param = p.Name();
      var = dataFile->get_var(param.c_str());

      float theMissingValue = 0;

      NcAtt *att = var->get_att("missing_value");

      if (att == 0)
        theMissingValue = kFloatMissing;

      else
        theMissingValue = att->as_float(0);

      //unsigned long univ_id = itsNewbaseParameters[i];

      // Slurp all data from one time step

      NcValues *vals = var->get_rec();

      // Get number of levels for this parameter

      unsigned int num_levels = 1;

      for (unsigned short d = 0; d < var->num_dims(); d++) {
        NcDim *dim = dataFile->get_dim(d);

        if (dim->name() == z->name()) {
          num_levels = itsZ.Size();
        }
      }

      for (unsigned int j = 0; j < num_levels; j++) {

        // Slice contains all data for one level

        vector<float> slice;

        // Flatten 3D array to 1D array

        for (unsigned int x = 0; x < itsX.Size(); x++) {
          for (unsigned int y = 0; y < itsY.Size(); y++) {
            float value = vals->as_float(j*(itsX.Size()*itsY.Size()) + y * itsX.Size() + x);

            if (value == theMissingValue)
              value = kFloatMissing;

            slice.push_back(value);
          }
        }

        //double level = itsLevels[j];

        NcFile nfile(info.filename.c_str(), NcFile::Replace);

        if (!(Write(slice, &nfile, info)))
          throw runtime_error("Error splitting data");

        files.push_back(info.filename);

      }
    }*/
  }

  return files;

}

/*
 * Write()
 *
 * Write data from memory to NetCDF file.
 *
 * Dimension names are specific and must not be be renamed.
 * - x dimension (and variable) = lon
 * - y dimension (and variable) = lat
 * - z dimension (and variable) = level
 *   * level is counted upwards from mean sea level
 *   * unit is ... ?
 * - time dimension (and variable) = time
 *   * time should be seconds since 1970-01-01 00:00:00 (epoch)
 *
 */

bool NFmiNetCDF::Write(const vector<float> &data, NcFile *theOutFile) {

  NcDim *x, *y, *z, *t;
  NcVar *xVar, *yVar, *zVar, *tVar, *parVar;

  // Dimensions

  if (!(x = theOutFile->add_dim("lon", itsX.Size())))
    return false;

  if (!(y = theOutFile->add_dim("lat", itsY.Size())))
    return false;

  if (!(z = theOutFile->add_dim("level", 1)))
    return false;

  if (!(t = theOutFile->add_dim("time")))
    return false;

  // Variables

  // x

  if (!(xVar = theOutFile->add_var("lon", ncFloat, x)))
    return false;

  if (!xVar->add_att("units", "degrees_north"))
    return false;

  if (!xVar->add_att("long_name", "longitude"))
    return false;

  if (!xVar->add_att("standard_name", "longitude"))
    return false;

  if (!xVar->add_att("axis", "x"))
    return false;

 /* if (!xVar->put(&itsXs[0], itsX.Size()))
    return false;
*/
  // y

  if (!(yVar = theOutFile->add_var("lat", ncFloat, y)))
    return false;

  if (!yVar->add_att("units", "degrees_east"))
    return false;

  if (!yVar->add_att("long_name", "latitude"))
      return false;

  if (!yVar->add_att("standard_name", "latitude"))
      return false;

  if (!yVar->add_att("axis", "y"))
    return false;
/*
  if (!yVar->put(&itsYs[0], itsY.Size()))
    return false;
*/
  // z

  if (!(zVar = theOutFile->add_var("level", ncFloat, z)))
    return false;

  if (!zVar->add_att("axis", "z"))
    return false;

  /*if (!zVar->put_rec(&info.level1))
    return false;*/

  string direction = itsZIsPositive ? "up" : "down";

  if (!zVar->add_att("positive", direction.c_str()))
    return false;

  if (!itsZUnit.empty()) {
    if (!zVar->add_att("units", itsZUnit.c_str()))
     return false;
  }

  // t

  if (!(tVar = theOutFile->add_var("time", ncFloat, t)))
    return false;

  if (!tVar->add_att("axis", "t"))
    return false;

  if (!tVar->add_att("units", "seconds since 1970-01-01 00:00:00"))
    return false;

  /*if (!tVar->put_rec(&info.year))
    return false;*/

  // parameter

  /*if (!(parVar = theOutFile->add_var(info.parname.c_str(), ncFloat, x, y, z)))
    return false;*/

  if (!parVar->add_att("_FillValue", kFloatMissing))
    return false;

  if (!parVar->add_att("missing_value", kFloatMissing))
    return false;

  // First level is one

  if (!parVar->put(&data[0], itsX.Size(), itsY.Size(), 1))
    return false;

  // Global attributes

  if (!theOutFile->add_att("generating_process_identifier", static_cast<long> (itsProcess)))
    return false;

  if (!theOutFile->add_att("Conventions", "CF-1.5"))
    return false;

  time_t now;
  time(&now);

  string datetime = ctime(&now);

  // ctime adds implicit newline

  datetime.erase(remove(datetime.begin(), datetime.end(), '\n'), datetime.end());

  if (!theOutFile->add_att("file_creation_time", datetime.c_str()))
    return false;

  if (!theOutFile->add_att("institution", "Finnish Meteorological Institute"))
    return false;

  if (!theOutFile->add_att("analysis_time", itsAnalysisTime.c_str()))
    return false;

  theOutFile->sync();
  theOutFile->close();

  return true;
}


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

float NFmiNetCDF::X0() {

  if (itsX0 == kFloatMissing) {

    float min = kFloatMissing;

    vector<float> values = itsX.Values();

    for (unsigned int i=0; i < values.size(); i++) {
      if (min == kFloatMissing || values[i] < min)
        min = values[0];
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
        min = values[0];
    }

    itsY0 = min;
  }

  return itsY0;

}

float NFmiNetCDF::ValueT(int num) {
  return itsT.Value(num);
}

float NFmiNetCDF::ValueZ(int num) {
  return itsZ.Value(num);
}

std::string NFmiNetCDF::Projection() {
  return itsProjection;
}

std::vector<float> NFmiNetCDF::Values(std::string theParameter) {

  std::vector<float> values;

  for (unsigned int i = 0; i  < itsParameters.size(); i++) {
    if (itsParameters[i].Name() == theParameter)
      values = itsParameters[i].Values();
  }

  return values;
}

std::vector<float> NFmiNetCDF::Values(int numOfParameter) {

  std::vector<float> values;

  values = itsParameters[numOfParameter].Values();

  return values;
}

bool NFmiNetCDF::HasDimension(const NFmiNetCDFVariable &var, const string &dim) {

  bool ret = false;

  if (dim == "z") {
    ret = var.HasDimension(z);
  }

  return ret;
}
