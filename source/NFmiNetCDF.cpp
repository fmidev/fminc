#include "NFmiNetCDF.h"
#include <boost/lexical_cast.hpp>
#include <boost/filesystem.hpp>
#include <stdexcept>
#include <sstream>
#include <fstream>
#include <iomanip>
#include <ctime>
#include <algorithm>
#include <cmath>

using namespace std;

const float kFloatMissing = 32700.f;
const float MAX_COORDINATE_RESOLUTION_ERROR = 1e-4f;

std::string NFmiNetCDF::Att(const std::string& attName) {
  return Att(Param(), attName);
}

std::string NFmiNetCDF::Att(NcVar* var, const std::string& attName)
{
  string ret("");
  assert(var);

  for (unsigned short i = 0; i < var->num_atts(); i++) {
    auto att = unique_ptr<NcAtt> (var->get_att(i));

    if (static_cast<string> (att->name()) == attName) {
      char *s = att->as_string(0);
      ret = static_cast<string> (s);
	  delete [] s;	
      break;
    }
  }

  return ret;

}

vector<float> NFmiNetCDF::Values(NcVar* var) {
  vector<float> values(var->num_vals(), kFloatMissing);
  var->get(&values[0], var->num_vals());
  return values;
}

vector<float> NFmiNetCDF::Values(NcVar* var, long timeIndex, long levelIndex) {
  
  long num_dims = var->num_dims();
  
  vector<long> cursor_position(num_dims), dimsizes(num_dims);

  for (unsigned short i = 0; i < num_dims; i++) {
    NcDim *dim = var->get_dim(i);
    string dimname = static_cast<string> (dim->name());

    long index = 0;
    long dimsize = dim->size();

    if (itsTDim && dimname == itsTDim->name())
    {
      index = timeIndex;
      dimsize = 1;
    }
    else if (levelIndex != -1 && itsZDim && dimname == itsZDim->name())
    {
      index = levelIndex;
      dimsize = 1;
    }

    cursor_position[i] = index;
    dimsizes[i] = dimsize;
  }

  var->set_cur(&cursor_position[0]);

  vector<float> values(itsXDim->size()*itsYDim->size(), kFloatMissing);
  var->get(&values[0], &dimsizes[0]);
  
  return values;
}

NFmiNetCDF::NFmiNetCDF()
 : itsTDim(0)
 , itsXDim(0)
 , itsYDim(0)
 , itsZDim(0)
 , itsProjection("latitude_longitude")
 , itsZVar(0)
 , itsXVar(0)
 , itsYVar(0)
 , itsTVar(0)
 , itsXFlip(false)
 , itsYFlip(false)
{
}

NFmiNetCDF::NFmiNetCDF(const string &theInfile)
 : itsTDim(0)
 , itsXDim(0)
 , itsYDim(0)
 , itsZDim(0)
 , itsProjection("latitude_longitude")
 , itsZVar(0)
 , itsXVar(0)
 , itsYVar(0)
 , itsTVar(0) 
 , itsXFlip(false)
 , itsYFlip(false)
{
  Read(theInfile);
}


NFmiNetCDF::~NFmiNetCDF() {
  itsDataFile->close();    
}

bool NFmiNetCDF::Read(const string &theInfile) {

  itsDataFile = unique_ptr<NcFile> (new NcFile(theInfile.c_str(), NcFile::ReadOnly));

  if (!itsDataFile->is_valid()) {
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
 
  if (itsZVar) {
    NextLevel();
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

  for (int i = 0; i < itsDataFile->num_dims() ; i++) {

    NcDim *dim = itsDataFile->get_dim(i);

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

    if (name == "level" || name == "lev" || name == "depth" || name == "pressure" || name == "height") {
      itsZDim = dim;
    }
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
  for (int i = 0; i < itsDataFile->num_vars() ; i++) {

    NcVar *var = itsDataFile->get_var(i);

    string varname = var->name();

    if (itsZDim && varname == static_cast<string> (itsZDim->name())) {

      /*
       * Assume level variable name equals to level dimension name. If it does not, how
       * do we know which variable is level variable?
       *
       * ( maybe when var dimension name == level dimension name and number of variable dimension is one ??
       * should be tested )
       */

      itsZVar = var;

      continue;
    }
    else if (varname == static_cast<string> (itsXDim->name())) {
      // X-coordinate

      itsXVar = var;

      vector<float> tmp = Values(itsXVar);

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
      }
      
      continue;
    }
    else if (varname == static_cast<string> (itsYDim->name())) {

      // Y-coordinate

      itsYVar = var;

      vector<float> tmp = Values(itsYVar);

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
      }
      
      continue;
    }
    else if (varname == static_cast<string> (itsTDim->name())) {

      /*
       * time
       *
       * Time in NetCDF is annoingly stupid: instead of using full timestamps or analysistime
       * + offset, an arbitrary point of time in the past is chosen and all time is an offset
       * to that time.
       */

      itsTVar = var;

      continue;
    }

    /*
     *  Search for projection information. CF conforming file has a variable
     *  that has attribute "grid_mapping_name".
     */

    for (short k=0; k < var->num_atts(); k++) {
      auto att = unique_ptr<NcAtt> (var->get_att(k));

      if (static_cast<string> (att->name()) == "grid_mapping_name") {
        const char* s = att->as_string(0);
        itsProjection = string(s);
		delete [] s;
		
	    break;
      }
    }

    itsParameters.push_back(var);

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

  for (int i = 0; i < itsDataFile->num_atts() ; i++) {

    auto att = unique_ptr<NcAtt> (itsDataFile->get_att(i));

    string name = att->name();

    if (name == "Conventions" && att->type() == ncChar) {
      const char *s = att->as_string(0);
      itsConvention = string(s);
      delete [] s;
    }
  }

  return true;
}
  
long int NFmiNetCDF::SizeX() {
  long ret = 0;
  if (itsXVar) ret = itsXVar->num_vals();
  return ret;
}

long int NFmiNetCDF::SizeY() {
  long ret = 0;
  if (itsYVar) ret =  itsYVar->num_vals();
  return ret;
}

long int NFmiNetCDF::SizeZ() {
  long ret = 0;
  if (itsZVar) ret = itsZVar->num_vals();
  return ret;
}

long int NFmiNetCDF::SizeT() {
  long ret = 0;
  if (itsTVar) ret = itsTVar->num_vals();
  return ret;
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

NcVar* NFmiNetCDF::Param() {
  return *itsParamIterator;
}

void NFmiNetCDF::ResetTime() {
  itsTimeIndex = -1;
}

bool NFmiNetCDF::NextTime() {
  itsTimeIndex++;
  if (itsTimeIndex < SizeT()) return true;
  
  return false;
}

float NFmiNetCDF::Time() {
  float val = kFloatMissing;
  if (itsTVar) {
    val = itsTVar->as_float(itsTimeIndex);  
  }
  return val;
}

void NFmiNetCDF::ResetLevel() {
  itsLevelIndex = -1;
}

bool NFmiNetCDF::NextLevel() {
  itsLevelIndex++;
  
  if (itsLevelIndex < SizeZ()) return true;
  
  return false;
}

float NFmiNetCDF::Level() {
  float val = kFloatMissing;
  if (itsZVar) {
    val = itsZVar->as_float(itsLevelIndex);  
  }
  return val;
}


long NFmiNetCDF::TimeIndex() {
  return itsTimeIndex;
}

string NFmiNetCDF::TimeUnit() {
  return Att(itsTVar, "units");
}

long NFmiNetCDF::LevelIndex() {
  return itsLevelIndex;
}


float NFmiNetCDF::X0() {
  float ret = kFloatMissing;
  
  if (Projection() == "polar_stereographic") {
    auto lonVar = itsDataFile->get_var("longitude");
    if (lonVar) ret = lonVar->as_float(0);
  }
  else {

    float min = kFloatMissing;

    vector<float> values = Values(itsXVar);

    for (unsigned int i=0; i < values.size(); i++) {
      if (min == kFloatMissing || values[i] < min) min = values[i];
    }
    
    ret = min;
  }

  return ret;
}

float NFmiNetCDF::Y0() {
  float ret = kFloatMissing;

  if (Projection() == "polar_stereographic") {
    auto latVar = itsDataFile->get_var("latitude");
    if (latVar) ret = latVar->as_float(0);
  }
  else {

    float min = kFloatMissing;

    vector<float> values = Values(itsYVar);

    for (unsigned int i=0; i < values.size(); i++) {
      if (min == kFloatMissing || values[i] < min) min = values[i];
    }
    ret = min;
  }

  return ret;

}

float NFmiNetCDF::Orientation() {
  float ret = kFloatMissing;
  
  NcVar* var = itsDataFile->get_var("stereographic");
  
  if (!var) return ret;
  
  auto att = unique_ptr<NcAtt> (var->get_att("longitude_of_projection_origin"));
  
  if (!att) return ret;
  
  return att->as_float(0);
}

std::string NFmiNetCDF::Projection() {
  return itsProjection;
}

std::vector<float> NFmiNetCDF::Values(std::string theParameter) {

  std::vector<float> values;

  for (unsigned int i = 0; i  < itsParameters.size(); i++) {
    if (itsParameters[i]->name() == theParameter) {
      values = Values(itsParameters[i], TimeIndex(), LevelIndex());
    break;
  }
  }

  return values;
}

std::vector<float> NFmiNetCDF::Values() {
  return Values(Param(), TimeIndex(), LevelIndex());
}

NcVar* NFmiNetCDF::GetVariable(const string& varName)
{
  for (unsigned int i = 0; i  < itsParameters.size(); i++) {
    if (string(itsParameters[i]->name()) == varName) return itsParameters[i];
  }
  throw out_of_range("Variable '" + varName + "' does not exist");
}

bool NFmiNetCDF::WriteSliceToCSV(const string &theFileName) {
  
  ofstream theOutFile;
  theOutFile.open(theFileName.c_str());
  
  vector<float> Xs = Values(itsXVar);

  if (itsXFlip)
    reverse(Xs.begin(), Xs.end());

  vector<float> Ys = Values(itsYVar);

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

bool CopyAtts(NcVar* newvar, NcVar* oldvar)
{
  for (unsigned short i = 0; i < oldvar->num_atts(); i++) {
    auto att = unique_ptr<NcAtt> (oldvar->get_att(i));
    if (!newvar->add_att(att->name(), att->as_string(0))) {
      return false;
    }
  }
    
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

  NcDim *theXDim = 0, *theYDim = 0, *theZDim = 0, *theTDim = 0;
  NcVar *theXVar = 0, *theYVar = 0, *theZVar = 0, *theTVar = 0, *outvar = 0;

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
   * Add z dimension if it exists in the original data
   */

  if (itsZDim)
  {
  if (!(theZDim = theOutFile.add_dim(itsZDim->name(), 1)))
      return false;
  }

  // Our unlimited dimension

  if (itsTDim)
    if (!(theTDim = theOutFile.add_dim(itsTDim->name())))
      return false;

  // Variables

  // x

  if (!(theXVar = theOutFile.add_var(itsXVar->name(), ncFloat, theXDim)))
    return false;

  CopyAtts(theXVar, itsXVar);
  
  // Put coordinate data

  // Flip x values if requested

  assert(itsXVar);
  vector<float> xValues = Values(itsXVar);

  if (itsXFlip)
    reverse(xValues.begin(), xValues.end());

  if (!theXVar->put(&xValues[0], SizeX()))
    return false;

  // y

  if (!(theYVar = theOutFile.add_var(itsYVar->name(), ncFloat, theYDim)))
    return false;

  CopyAtts(theXVar, itsXVar);


  // Put coordinate data

  // Flip y values if requested

  assert(itsYVar);
  vector<float> yValues = Values(itsYVar);

  if (itsYFlip)
    reverse(yValues.begin(), yValues.end());

  if (!theYVar->put(&yValues[0], SizeY()))
    return false;

  // z
  
  if (itsZDim && itsZVar)
  {
    if (!(theZVar = theOutFile.add_var(theZDim->name(), ncFloat, theZDim)))
  {
      return false;
    }

    CopyAtts(theZVar, itsZVar);

    /*
     * Set z value. If current variable has no z dimension, Level() will return
     * kFloatMissing. In that case we set level = 0.
     */
  
  float zValue = Level();

    if (zValue == kFloatMissing)
      zValue = 0;

    if (!theZVar->put(&zValue, 1))
      return false;

  }



  // t

  if (!(theTVar = theOutFile.add_var(itsTDim->name(), ncFloat, theTDim)))
    return false;

  CopyAtts(theZVar, itsZVar);

  float tValue = Time();

  if (!theTVar->put(&tValue, 1))
    return false;

  // parameter

  // Add dimensions to parameter. Note! Order must be the same!

  NcVar* var = Param();
  int num_dims = var->num_dims();

  vector<NcDim*> dims(num_dims);
  vector<long> cursor_position(num_dims);

  for (int i = 0; i < num_dims; i++)
  {
    string dimname = var->get_dim(i)->name();

  if (dimname == itsTDim->name())
  {
    dims[i] = theTDim;
    cursor_position[i] = 1;
  }
  else if (theZDim && dimname == itsZDim->name())
  {
    dims[i] = theZDim;
    cursor_position[i] = 1;
  }
  else if (dimname == itsXDim->name())
  {
    dims[i] = theXDim;
    cursor_position[i] = itsXDim->size();
  }
  else if (dimname == itsYDim->name())
  {
    dims[i] = theYDim;
    cursor_position[i] = itsYDim->size();
  }
  }

  if (!(outvar = theOutFile.add_var(var->name(), ncFloat, static_cast<int> (num_dims), const_cast<const NcDim**> (&dims[0]))))
  {
    return false;
  }

  CopyAtts(outvar, var);

  vector<float> data = Values();

  if (!outvar->put(&data[0], &cursor_position[0]))
  {
    return false;
  }

  // Global attributes

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
  float x1 = itsXVar->as_float(0);
  float x2 = itsXVar->as_float(1);
  
  float delta = x2 - x1;
  string units = Att(itsXVar, "units");
  if (!units.empty()) {
    if (units == "100  km") delta *= 100;
  }
  
  return fabs(delta);
}

float NFmiNetCDF::YResolution() {
  float y1 = itsYVar->as_float(0);
  float y2 = itsYVar->as_float(1);
  
  float delta = y2 - y1;
  string units = Att(itsYVar, "units");
  if (!units.empty()) {
    if (units == "100  km") delta *= 100;
  }
  
  return fabs(delta);
}

bool NFmiNetCDF::CoordinatesInRowMajorOrder(const NcVar* var)
{
  int num_dims = var->num_dims();

  int xCoordNum = -1, yCoordNum = -1;

  assert(HasDimension(var, itsXDim->name()));
  assert(HasDimension(var, itsYDim->name()));
  
  for (int i = 0; i < num_dims; i++) {
    string dimname = var->get_dim(i)->name();
    if (dimname == string(itsXDim->name())) {
      xCoordNum = i;
    }    
    else if (dimname == string(itsYDim->name())) {
      yCoordNum = i;
    }
  }
  assert(xCoordNum != yCoordNum);

  return (xCoordNum > yCoordNum);
}

bool NFmiNetCDF::HasDimension(const std::string& dimName)
{
  return HasDimension(Param(), dimName);
}

bool NFmiNetCDF::HasDimension(const NcVar* var, const std::string& dimName)
{
  long num_dims = var->num_dims();
  
  for (int i = 0; i < num_dims; i++) {
	  NcDim* dim = var->get_dim(i);
	  if (dim->name() == dimName) return true;
  }
  return false;
}