#include "NFmiNetCDF.h"
#include <algorithm>
#include <boost/filesystem.hpp>
#include <cmath>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>

const float MAX_COORDINATE_RESOLUTION_ERROR = 1e-4f;
const float NFmiNetCDF::kFloatMissing = 32700.0f;

bool CopyAtts(NcVar* newvar, NcVar* oldvar);

using namespace std;

NFmiNetCDF::NFmiNetCDF()
    : itsTDim(0),
      itsXDim(0),
      itsYDim(0),
      itsZDim(0),
      itsProjection("latitude_longitude"),
      itsZVar(0),
      itsXVar(0),
      itsYVar(0),
      itsTVar(0),
      itsProjectionVar(0),
      itsXFlip(false),
      itsYFlip(false)
{
}

NFmiNetCDF::NFmiNetCDF(const string& theInfile)
    : itsTDim(0),
      itsXDim(0),
      itsYDim(0),
      itsZDim(0),
      itsProjection("latitude_longitude"),
      itsZVar(0),
      itsXVar(0),
      itsYVar(0),
      itsTVar(0),
      itsProjectionVar(0),
      itsXFlip(false),
      itsYFlip(false)
{
	Read(theInfile);
}

NFmiNetCDF::~NFmiNetCDF() { itsDataFile->close(); }
bool NFmiNetCDF::Read(const string& theInfile)
{
	itsDataFile = unique_ptr<NcFile>(new NcFile(theInfile.c_str(), NcFile::ReadOnly));

	if (!itsDataFile->is_valid())
	{
		return false;
	}

	// Read metadata from file.

	if (!ReadAttributes()) return false;

	if (!ReadDimensions()) return false;

	if (!ReadVariables()) return false;

	if (itsParameters.size() == 0)
	{
		return false;
	}

	// Set initial time and level values since they are easily forgotten.

	ResetTime();
	NextTime();
	ResetLevel();

	if (itsZVar)
	{
		NextLevel();
	}

	return true;
}

// Sizes
long int NFmiNetCDF::SizeX() const
{
	long ret = 0;
	if (itsXDim) ret = itsXDim->size();
	return ret;
}

long int NFmiNetCDF::SizeY() const
{
	long ret = 0;
	if (itsYDim) ret = itsYDim->size();
	return ret;
}

long int NFmiNetCDF::SizeZ() const
{
	long ret = 0;
	if (itsZDim) ret = itsZDim->size();
	return ret;
}

long int NFmiNetCDF::SizeT() const
{
	long ret = 0;
	if (itsTDim) ret = itsTDim->size();
	return ret;
}

long int NFmiNetCDF::SizeParams() const { return itsParameters.size(); }
// Types
nc_type NFmiNetCDF::TypeX() const { return itsXVar->type(); }
nc_type NFmiNetCDF::TypeY() const { return itsYVar->type(); }
nc_type NFmiNetCDF::TypeZ() const { return itsZVar->type(); }
nc_type NFmiNetCDF::TypeT() const { return itsTVar->type(); }
// Metadata
bool NFmiNetCDF::IsConvention() const { return (!itsConvention.empty()); }
string NFmiNetCDF::Convention() const { return itsConvention; }
float NFmiNetCDF::Orientation() const
{
	float ret = kFloatMissing;

	NcVar* var = itsDataFile->get_var("stereographic");

	if (!var) return ret;

	auto att = unique_ptr<NcAtt>(var->get_att("longitude_of_projection_origin"));

	if (!att) return ret;

	return att->as_float(0);
}

std::string NFmiNetCDF::Projection() const { return itsProjection; }

float NFmiNetCDF::TrueLatitude()
{
        float ret = kFloatMissing;

        NcVar* var = itsDataFile->get_var("stereographic");

        if (!var) return ret;

        auto att = unique_ptr<NcAtt>(var->get_att("latitude_of_projection_origin"));

        if (!att) return ret;

        return att->as_float(0);
}

// Params
void NFmiNetCDF::FirstParam() { itsParamIterator = itsParameters.begin(); }
bool NFmiNetCDF::NextParam() { return (++itsParamIterator < itsParameters.end()); }
NcVar* NFmiNetCDF::Param() { return *itsParamIterator; }
// Time
void NFmiNetCDF::ResetTime() { itsTimeIndex = -1; }
bool NFmiNetCDF::NextTime()
{
	itsTimeIndex++;
	if (itsTimeIndex < SizeT()) return true;

	return false;
}

template <typename T>
T NFmiNetCDF::Time()
{
	T val = static_cast<T>(kFloatMissing);

	itsTVar->set_cur(&itsTimeIndex);
	itsTVar->get(&val, 1);

	return val;
}

template float NFmiNetCDF::Time<float>();
template double NFmiNetCDF::Time<double>();
template int NFmiNetCDF::Time<int>();
template short NFmiNetCDF::Time<short>();
template char NFmiNetCDF::Time<char>();
template int8_t NFmiNetCDF::Time<int8_t>();

long NFmiNetCDF::TimeIndex() { return itsTimeIndex; }
string NFmiNetCDF::TimeUnit() { return Att(itsTVar, "units"); }
// Level
void NFmiNetCDF::ResetLevel() { itsLevelIndex = -1; }
bool NFmiNetCDF::NextLevel()
{
	itsLevelIndex++;

	if (itsLevelIndex < SizeZ()) return true;

	return false;
}

float NFmiNetCDF::Level()
{
	float val = kFloatMissing;
	if (itsZVar)
	{
		val = itsZVar->as_float(itsLevelIndex);
	}
	return val;
}

long NFmiNetCDF::LevelIndex() { return itsLevelIndex; }
template <typename T>
vector<T> NFmiNetCDF::Values(std::string theParameter)
{
	vector<T> values;

	for (unsigned int i = 0; i < itsParameters.size(); i++)
	{
		if (itsParameters[i]->name() == theParameter)
		{
			values = Values<T>(itsParameters[i], TimeIndex(), LevelIndex());
			break;
		}
	}

	return values;
}

template <typename T>
vector<T> NFmiNetCDF::Values()
{
	return Values<T>(Param(), TimeIndex(), LevelIndex());
}

NcVar* NFmiNetCDF::GetVariable(const string& varName) const
{
	for (unsigned int i = 0; i < itsParameters.size(); i++)
	{
		if (string(itsParameters[i]->name()) == varName) return itsParameters[i];
	}
	throw out_of_range("Variable '" + varName + "' does not exist");
}

bool NFmiNetCDF::WriteSliceToCSV(const string& theFileName)
{
	ofstream theOutFile;
	theOutFile.open(theFileName.c_str());

	auto Xs = Values<float>(itsXVar);

	if (itsXFlip) reverse(Xs.begin(), Xs.end());

	auto Ys = Values<float>(itsYVar);

	if (itsYFlip) reverse(Ys.begin(), Ys.end());

	auto values = Values<float>();

	for (unsigned int y = 0; y < Ys.size(); y++)
	{
		for (unsigned int x = 0; x < Xs.size(); x++)
		{
			theOutFile << Xs[x] << "," << Ys[y] << "," << values[y * Xs.size() + x] << endl;
		}
	}

	theOutFile.close();

	return true;
}

/*
 * WriteSlice(string)
 *
 * Write the current slice to file. Slice means the level/parameter combination that
 * is defined by the iterators.
 *
 * If iterators are not valid, WriteSlice() returns false.
 * If a parameter has no z dimension, WriteSlice() will ignore z iterator value.
 *
 */

bool NFmiNetCDF::WriteSlice(const std::string& theFileName)
{
	NcDim *theXDim = 0, *theYDim = 0, *theZDim = 0, *theTDim = 0;
	NcVar *theXVar = 0, *theYVar = 0, *theZVar = 0, *theTVar = 0, *outvar = 0;

	boost::filesystem::path f(theFileName);
	string dir = f.parent_path().string();

	if (!boost::filesystem::exists(dir))
	{
		if (!boost::filesystem::create_directories(dir))
		{
			cerr << "Unable to create directory " << dir << endl;
			return false;
		}
	}

	NcFile theOutFile(theFileName.c_str(), NcFile::Replace);

	if (!theOutFile.is_valid())
	{
		cerr << "Unable to create file " << theFileName << endl;
		return false;
	}

	// Dimensions

	if (!(theXDim = theOutFile.add_dim(itsXDim->name(), SizeX()))) return false;

	if (!(theYDim = theOutFile.add_dim(itsYDim->name(), SizeY()))) return false;

	// Add z dimension if it exists in the original data

	if (itsZDim)
	{
		if (!(theZDim = theOutFile.add_dim(itsZDim->name(), 1))) return false;
	}

	// Our unlimited dimension

	if (itsTDim)
		if (!(theTDim = theOutFile.add_dim(itsTDim->name()))) return false;

	// Variables

	// x

	if (!(theXVar = theOutFile.add_var(itsXVar->name(), ncFloat, theXDim))) return false;

	CopyAtts(theXVar, itsXVar);

	// Put coordinate data

	// Flip x values if requested

	assert(itsXVar);
	auto xValues = Values<float>(itsXVar);

	if (itsXFlip) reverse(xValues.begin(), xValues.end());

	if (!theXVar->put(&xValues[0], SizeX())) return false;

	// y

	if (!(theYVar = theOutFile.add_var(itsYVar->name(), ncFloat, theYDim))) return false;

	CopyAtts(theYVar, itsYVar);

	// Put coordinate data

	// Flip y values if requested

	assert(itsYVar);
	auto yValues = Values<float>(itsYVar);

	if (itsYFlip) reverse(yValues.begin(), yValues.end());

	if (!theYVar->put(&yValues[0], SizeY())) return false;

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

		auto zValue = Level();

		if (zValue == kFloatMissing) zValue = 0;

		if (!theZVar->put(&zValue, 1)) return false;
	}

	// t

	switch (itsTVar->type())
	{
		case ncFloat:
			if (!(theTVar = theOutFile.add_var(itsTDim->name(), ncFloat, theTDim))) return false;
			break;
		case ncDouble:
			if (!(theTVar = theOutFile.add_var(itsTDim->name(), ncDouble, theTDim))) return false;
			break;
		case ncShort:
			if (!(theTVar = theOutFile.add_var(itsTDim->name(), ncShort, theTDim))) return false;
			break;
		case ncInt:
			if (!(theTVar = theOutFile.add_var(itsTDim->name(), ncInt, theTDim))) return false;
			break;
		case ncChar:
                        if (!(theTVar = theOutFile.add_var(itsTDim->name(), ncChar, theTDim))) return false;
                        break;
		case ncByte:
                        if (!(theTVar = theOutFile.add_var(itsTDim->name(), ncByte, theTDim))) return false;
                        break;
		case ncNoType:
		default:
			cout << "NcType not supported for Time" << endl;
	}

	CopyAtts(theTVar, itsTVar);

	switch (itsTVar->type())
	{
		case ncFloat:
		{
			auto tValue = Time<float>();
			if (!theTVar->put(&tValue, 1)) return false;
		}
		break;

		case ncDouble:
		{
			auto tValue = Time<double>();
			if (!theTVar->put(&tValue, 1)) return false;
		}
		break;

		case ncShort:
		{
			auto tValue = Time<short>();
			if (!theTVar->put(&tValue, 1)) return false;
		}
		break;

		case ncInt:
		{
			auto tValue = Time<int>();
			if (!theTVar->put(&tValue, 1)) return false;
		}
		break;

		case ncChar:
                {
                        auto tValue = Time<char>();
                        if (!theTVar->put(&tValue, 1)) return false;
                }
                break;

		case ncByte:
                {
                        auto tValue = Time<int8_t>();
                        if (!theTVar->put(&tValue, 1)) return false;
                }
                break;

		case ncNoType:
		default:
			cout << "NcType not supported for data" << endl;
	}

	// Add projection variable if it exists

	if (itsProjectionVar)
	{
		NcVar* outprojvar = 0;
		switch (itsProjectionVar->type())
		{
			case ncFloat:
				if (!(outprojvar = theOutFile.add_var(itsProjectionVar->name(), ncFloat)))
				{
					return false;
				}
				break;

			case ncDouble:
				if (!(outprojvar = theOutFile.add_var(itsProjectionVar->name(), ncDouble)))
				{
					return false;
				}
				break;

			case ncShort:
				if (!(outprojvar = theOutFile.add_var(itsProjectionVar->name(), ncShort)))
				{
					return false;
				}
				break;

			case ncInt:
				if (!(outprojvar = theOutFile.add_var(itsProjectionVar->name(), ncInt)))
				{
					return false;
				}
				break;
			case ncChar:
				if (!(outprojvar = theOutFile.add_var(itsProjectionVar->name(), ncChar)))
                                {
                                        return false;
                                }
                                break;
			case ncByte:
				if (!(outprojvar = theOutFile.add_var(itsProjectionVar->name(), ncByte)))
                                {
                                        return false;
                                }
                                break;

			case ncNoType:
			default:
				cout << "NcType not supported" << endl;
		}
		CopyAtts(outprojvar, itsProjectionVar);

		// Check if "longitude" and "latitude" variables exist

		NcVar *outlon = 0, *outlat = 0;

		NcVar* lon = itsDataFile->get_var("longitude");
		NcVar* lat = itsDataFile->get_var("latitude");

		if (itsProjection == "polar_stereographic" && lon && lat)
		{
			// get dims
			vector<NcDim*> dims;
			assert(lon->num_dims() == lat->num_dims());
			vector<long> cursors;

			for (int i = 0; i < lon->num_dims(); i++)
			{
				auto dim = lon->get_dim(i);
				assert(dim);

				if (static_cast<string>(dim->name()) == static_cast<string>(theXDim->name()))
				{
					dims.push_back(theXDim);
					cursors.push_back(itsXVar->num_vals());
				}
				else if (static_cast<string>(dim->name()) == static_cast<string>(theYDim->name()))
				{
					dims.push_back(theYDim);
					cursors.push_back(itsYVar->num_vals());
				}
			}

			assert(lon->num_dims() == static_cast<int>(dims.size()));

			outlon = theOutFile.add_var(lon->name(), ncFloat, static_cast<int>(dims.size()),
			                            const_cast<const NcDim**>(&dims[0]));
			outlat = theOutFile.add_var(lat->name(), ncFloat, static_cast<int>(dims.size()),
			                            const_cast<const NcDim**>(&dims[0]));

			assert(outlon);
			assert(outlat);

			CopyAtts(outlon, lon);
			CopyAtts(outlat, lat);

			// copy data
			size_t totalSize = 1;
			for (size_t i = 0; i < cursors.size(); i++) totalSize *= cursors[i];

			vector<float> vals(totalSize);
			lon->get(&vals[0], &cursors[0]);
			outlon->put(&vals[0], itsYVar->num_vals(), itsXVar->num_vals());

			lat->get(&vals[0], &cursors[0]);
			outlat->put(&vals[0], &cursors[0]);
		}
	}

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

	switch (var->type())
	{
		case ncFloat:
		{
			if (!(outvar = theOutFile.add_var(var->name(), ncFloat, static_cast<int>(num_dims),
			                                  const_cast<const NcDim**>(&dims[0]))))
			{
				return false;
			}

			auto float_data = Values<float>();
			if (!(outvar->put(&float_data[0], &cursor_position[0])))
			{
				return false;
			}
			break;
		}

		case ncDouble:
		{
			if (!(outvar = theOutFile.add_var(var->name(), ncDouble, static_cast<int>(num_dims),
			                                  const_cast<const NcDim**>(&dims[0]))))
			{
				return false;
			}

			auto double_data = Values<double>();
			if (!(outvar->put(&double_data[0], &cursor_position[0])))
			{
				return false;
			}
			break;
		}

		case ncShort:
		{
			if (!(outvar = theOutFile.add_var(var->name(), ncShort, static_cast<int>(num_dims),
			                                  const_cast<const NcDim**>(&dims[0]))))
			{
				return false;
			}

			auto short_data = Values<short>();
			if (!(outvar->put(&short_data[0], &cursor_position[0])))
			{
				return false;
			}
			break;
		}

		case ncInt:
		{
			if (!(outvar = theOutFile.add_var(var->name(), ncInt, static_cast<int>(num_dims),
			                                  const_cast<const NcDim**>(&dims[0]))))
			{
				return false;
			}

			auto int_data = Values<int>();
			if (!(outvar->put(&int_data[0], &cursor_position[0])))
			{
				return false;
			}

			break;
		}

		case ncChar:
		{
                        if (!(outvar = theOutFile.add_var(var->name(), ncChar, static_cast<char>(num_dims),
                                                          const_cast<const NcDim**>(&dims[0]))))
                        {
                                return false;
                        }

                        auto char_data = Values<char>();
                        if (!(outvar->put(&char_data[0], &cursor_position[0])))
                        {
                                return false;
                        }

                        break;
		}

		case ncByte:
		{
                        if (!(outvar = theOutFile.add_var(var->name(), ncByte, static_cast<int8_t>(num_dims),
                                                          const_cast<const NcDim**>(&dims[0]))))
                        {
                                return false;
                        }

                        auto byte_data = Values<int8_t>();
                        if (!(outvar->put(&byte_data[0], &cursor_position[0])))
                        {
                                return false;
                        }

                        break;
		}
		case ncNoType:
		default:
			cout << "NcType not supported" << endl;
			return false;
			break;
	}

	CopyAtts(outvar, var);

#ifndef NDEBUG
	auto att = unique_ptr<NcAtt>(outvar->get_att("_FillValue"));
	assert(att->type() == outvar->type());
#endif

	// Global attributes

	if (!itsConvention.empty())
	{
		if (!theOutFile.add_att("Conventions", itsConvention.c_str())) return false;
	}

	time_t now;
	time(&now);

	string datetime = ctime(&now);

	// ctime adds implicit newline

	datetime.erase(remove(datetime.begin(), datetime.end(), '\n'), datetime.end());

	if (!theOutFile.add_att("file_creation_time", datetime.c_str())) return false;

	if (!itsInstitution.empty())
	{
		if (!theOutFile.add_att("institution", itsInstitution.c_str())) return false;
	}

	if (!theOutFile.add_att("distributor", "Finnish Meteorological Institute")) return false;

	theOutFile.sync();
	theOutFile.close();

	return true;
}

/*
 * If itsXFlip is set, when writing slice to NetCDF of CSV file the values of X axis
 * are flipped, ie. from 1 2 3 we have 3 2 1.
 *
 * Same applies for Y axis.
 */

bool NFmiNetCDF::FlipX() { return itsXFlip; }
void NFmiNetCDF::FlipX(bool theXFlip) { itsXFlip = theXFlip; }
bool NFmiNetCDF::FlipY() { return itsYFlip; }
void NFmiNetCDF::FlipY(bool theYFlip) { itsYFlip = theYFlip; }
float NFmiNetCDF::XResolution()
{
	float x1 = itsXVar->as_float(0);
	float x2 = itsXVar->as_float(SizeX()-1);

	float delta = fabs(x2 - x1);
	string units = Att(itsXVar, "units");
	if (!units.empty())
	{
		if (units == "100  km") delta *= 100;
	}

	return delta/(SizeX()-1);
}

float NFmiNetCDF::YResolution()
{
	float y1 = itsYVar->as_float(0);
	float y2 = itsYVar->as_float(SizeY()-1);

	float delta = fabs(y2 - y1);
	string units = Att(itsYVar, "units");
	if (!units.empty())
	{
		if (units == "100  km") delta *= 100;
	}

	return delta/(SizeY()-1);
}

bool NFmiNetCDF::CoordinatesInRowMajorOrder(const NcVar* var)
{
	int num_dims = var->num_dims();

	int xCoordNum = -1, yCoordNum = -1;

	assert(HasDimension(var, itsXDim->name()));
	assert(HasDimension(var, itsYDim->name()));

	for (int i = 0; i < num_dims; i++)
	{
		string dimname = var->get_dim(i)->name();
		if (dimname == string(itsXDim->name()))
		{
			xCoordNum = i;
		}
		else if (dimname == string(itsYDim->name()))
		{
			yCoordNum = i;
		}
	}
	assert(xCoordNum != yCoordNum);

	return (xCoordNum > yCoordNum);
}

bool NFmiNetCDF::HasDimension(const std::string& dimName) { return HasDimension(Param(), dimName); }
std::string NFmiNetCDF::Att(const std::string& attName) { return Att(Param(), attName); }
// private functions
bool NFmiNetCDF::HasDimension(const NcVar* var, const std::string& dimName)
{
	long num_dims = var->num_dims();
	if (dimName == "z" && !itsZDim) return false;

	for (int i = 0; i < num_dims; i++)
	{
		NcDim* dim = var->get_dim(i);
		string dimname;
		if (dimName == "z")
			dimname = itsZDim->name();
		else if (dimName == "t")
			dimname = itsTDim->name();
		else if (dimName == "x")
			dimname = itsXDim->name();
		else if (dimName == "y")
			dimname = itsYDim->name();

		if (static_cast<string>(dim->name()) == dimname) return true;
	}
	return false;
}

std::string NFmiNetCDF::Att(NcVar* var, const std::string& attName)
{
	string ret("");
	assert(var);

	for (unsigned short i = 0; i < var->num_atts(); i++)
	{
		auto att = unique_ptr<NcAtt>(var->get_att(i));

		if (static_cast<string>(att->name()) == attName)
		{
			char* s = att->as_string(0);
			ret = static_cast<string>(s);
			delete[] s;
			break;
		}
	}

	return ret;
}

template <typename T>
vector<T> NFmiNetCDF::Values(NcVar* var)
{
	vector<T> values(var->num_vals(), kFloatMissing);
	var->get(&values[0], var->num_vals());
	return values;
}

template <typename T>
vector<T> NFmiNetCDF::Values(NcVar* var, long timeIndex, long levelIndex)
{
	long num_dims = var->num_dims();

	vector<long> cursor_position(num_dims), dimsizes(num_dims);

	for (unsigned short i = 0; i < num_dims; i++)
	{
		NcDim* dim = var->get_dim(i);
		string dimname = static_cast<string>(dim->name());

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

	vector<T> values(itsXDim->size() * itsYDim->size(), static_cast<T>(kFloatMissing));
	var->get(&values[0], &dimsizes[0]);

	return values;
}

bool NFmiNetCDF::ReadDimensions()
{
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

	for (int i = 0; i < itsDataFile->num_dims(); i++)
	{
		NcDim* dim = itsDataFile->get_dim(i);

		string name = dim->name();

		// NetCDF conventions suggest x coordinate to be named "lon"
		if (name == "x" || name == "X" || name == "lon" || name == "longitude")
		{
			itsXDim = dim;
		}

		// NetCDF conventions suggest y coordinate to be named "lat"

		if (name == "y" || name == "Y" || name == "lat" || name == "latitude")
		{
			itsYDim = dim;
		}

		if (name == "time" || name == "time_counter" || name == "rec" || dim->is_unlimited())
		{
			itsTDim = dim;
		}

		if (name == "level" || name == "lev" || name == "depth" || name == "pressure" || name == "height")
		{
			itsZDim = dim;
		}
	}

	if (!itsYDim || !itsXDim || (!itsTDim || !itsTDim->is_valid()))
	{
		cout << "Required dimensions not found" << endl
		     << "x dim: " << (itsXDim ? "found" : "not found") << endl
		     << "y dim: " << (itsYDim ? "found" : "not found") << endl
		     << "time dim: " << (itsTDim ? "found " : "not found ")
		     << "is valid: " << (itsTDim && itsTDim->is_valid() ? "yes" : "no") << endl;
		return false;
	}

	return true;
}

bool NFmiNetCDF::ReadVariables()
{
	/*
	 * Read variables from netcdf
	 *
	 * Each dimension in the file has a corresponding variable,
	 * also each actual data parameter is presented as a variable.
	 */

	for (int i = 0; i < itsDataFile->num_vars(); i++)
	{
		NcVar* var = itsDataFile->get_var(i);

		string varname = var->name();

		if (itsZDim && varname == static_cast<string>(itsZDim->name()))
		{
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
		else if (varname == static_cast<string>(itsXDim->name()))
		{
			// X-coordinate

			itsXVar = var;

			auto tmp = Values<float>(itsXVar);

			if (itsXFlip) reverse(tmp.begin(), tmp.end());

			if (tmp.size() > 1)
			{
				// Check resolution

				float resolution = 0;
				float prevResolution = resolution;

				float prevX = tmp[0];

				for (unsigned int k = 1; k < tmp.size(); k++)
				{
					resolution = tmp[k] - prevX;

					if (k == 1) prevResolution = resolution;

					if (abs(resolution - prevResolution) > MAX_COORDINATE_RESOLUTION_ERROR)
					{
						cerr << "X dimension resolution is not constant, diff: " << (prevResolution - resolution)
						     << endl;
						return false;
					}

					prevResolution = resolution;
					prevX = tmp[k];
				}
			}

			continue;
		}
		else if (varname == static_cast<string>(itsYDim->name()))
		{
			// Y-coordinate

			itsYVar = var;

			auto tmp = Values<float>(itsYVar);

			if (itsYFlip) reverse(tmp.begin(), tmp.end());

			if (tmp.size() > 1)
			{
				// Check resolution

				float resolution = 0;
				float prevResolution = resolution;

				float prevY = tmp[0];

				for (unsigned int k = 1; k < tmp.size(); k++)
				{
					resolution = tmp[k] - prevY;

					if (k == 1) prevResolution = resolution;

					if (abs(resolution - prevResolution) > MAX_COORDINATE_RESOLUTION_ERROR)
					{
						cerr << "Y dimension resolution is not constant, diff: " << (prevResolution - resolution)
						     << endl;
						return false;
					}

					prevResolution = resolution;
					prevY = tmp[k];
				}
			}

			continue;
		}
		else if (varname == static_cast<string>(itsTDim->name()))
		{
			/*
			 * time
			 *
			 * Time in NetCDF is an arbitrary point in time in the past and all time in the file
			 * is an offset to that time.
			 */

			itsTVar = var;

			continue;
		}

		/*
		 *  Search for projection information. CF conforming file has a variable
		 *  that has attribute "grid_mapping_name".
		 */

		bool foundproj = false;

		for (short k = 0; k < var->num_atts(); k++)
		{
			auto att = unique_ptr<NcAtt>(var->get_att(k));
			if (static_cast<string>(att->name()) == "grid_mapping_name")
			{
				const char* s = att->as_string(0);
				itsProjection = string(s);
				delete[] s;

				itsProjectionVar = var;
				foundproj = true;
				break;
			}
		}

		if (foundproj || varname == "latitude" || varname == "longitude") continue;

		itsParameters.push_back(var);
	}

	return true;
}

bool NFmiNetCDF::ReadAttributes()
{
	/*
	 * Read global attributes from netcdf
	 *
	 * Attributes are global metadata definitions in the netcdf file.
	 *
	 * Two attributes are critical: producer id and analysis time. Both can
	 * be overwritten by command line options.
	 */

	for (int i = 0; i < itsDataFile->num_atts(); i++)
	{
		auto att = unique_ptr<NcAtt>(itsDataFile->get_att(i));

		string name = att->name();

		if (name == "Conventions" && att->type() == ncChar)
		{
			const char* s = att->as_string(0);
			itsConvention = string(s);
			delete[] s;
		}
	}

	return true;
}

// free functions
bool CopyAtts(NcVar* newvar, NcVar* oldvar)
{
	assert(newvar);
	assert(oldvar);

	for (unsigned short i = 0; i < oldvar->num_atts(); i++)
	{
		auto att = unique_ptr<NcAtt>(oldvar->get_att(i));

		auto nctype = att->type();

		if (static_cast<string>(att->name()) == "_FillValue" || static_cast<string>(att->name()) == "missing_value")
		{
			switch (oldvar->type())
			{
				case ncFloat:
					if (!newvar->add_att(att->name(), att->as_float(0))) return false;
					break;

				case ncDouble:
					if (!newvar->add_att(att->name(), att->as_double(0))) return false;
					break;

				case ncShort:
					if (!newvar->add_att(att->name(), att->as_short(0))) return false;
					break;

				case ncInt:
					if (!newvar->add_att(att->name(), att->as_int(0))) return false;
					break;
				case ncChar:
					if (!newvar->add_att(att->name(), att->as_char(0))) return false;
					break;
				case ncByte:
					if (!newvar->add_att(att->name(), att->as_ncbyte(0))) return false;
					break;
				case ncNoType:
				default:
					cout << "NcType not supported for Var" << endl;
			}
		}

		else if (nctype == ncDouble)
		{
			if (!newvar->add_att(att->name(), att->as_double(0)))
			{
				return false;
			}
		}

		else if (nctype == ncFloat)
		{
			if (!newvar->add_att(att->name(), att->as_float(0)))
			{
				return false;
			}
		}

		else if (nctype == ncChar)
		{
			if (!newvar->add_att(att->name(), att->as_string(0)))
			{
				return false;
			}
		}

		else if (nctype == ncInt)
		{
			if (!newvar->add_att(att->name(), att->as_int(0)))
			{
				return false;
			}
		}

		else if (nctype == ncByte)
		{
			if (!newvar->add_att(att->name(), att->as_ncbyte(0)))
			{
				return false;
			}
		}

		else
		{
			if (!newvar->add_att(att->name(), att->as_string(0)))
			{
				return false;
			}
		}
	}

	return true;
}
