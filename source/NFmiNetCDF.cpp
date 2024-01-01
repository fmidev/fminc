#include "NFmiNetCDF.h"
#include <algorithm>
#include <boost/filesystem.hpp>
#include <cmath>
#include <ctime>
#include <fmt/format.h>
#include <fstream>
#include <iomanip>
#include <numeric>
#include <sstream>
#include <stdexcept>
#include <atomic>

const float MAX_COORDINATE_RESOLUTION_ERROR = 1e-4f;
const float NFmiNetCDF::kFloatMissing = 32700.0f;

static std::atomic<bool> xCoordinateWarning(true);
static std::atomic<bool> yCoordinateWarning(true);

const bool USE_IMPROVED_PRECISION =
    getenv("FMINC_USE_IMPROVED_PRECISION") != nullptr && getenv("FMINC_USE_IMPROVED_PRECISION")[0] == '1';

using namespace std;

bool CopyAtts(NcVar* newvar, const NcVar* oldvar);
bool CopyVar(NcVar** newvar, NcVar* oldvar, NcFile* theOutFile, long* dimension_position, long* dimension_length);
vector<pair<string, string>> ReadGlobalAttributes(NcFile* theFile);

int NumberOfDecimalDigits(std::string str)
{
	// - remove all trailing zeros
	// - count number of digits
	str.erase(str.find_last_not_of('0') + 1, std::string::npos);
	const auto dot = str.find('.');
	size_t num_digits = (dot == std::string::npos) ? 0 : str.length() - dot - 1;
	num_digits = std::min(num_digits, static_cast<size_t>(8));  // prevent neverending floats to inflate digitcount
	return static_cast<int>(num_digits);
}

template <typename T, typename U>
T ToPrecision(U val, int num_digits)
{
	if (USE_IMPROVED_PRECISION == false)
	{
		return static_cast<T>(val);
	}

	std::string val_str = fmt::format("{:.{}f}", val, num_digits);

	if (std::is_same<T, float>::value)
	{
		return std::stof(val_str);
	}
	else if (std::is_same<T, double>::value)
	{
		return std::stod(val_str);
	}
}
template <typename T, typename U>
T ToSamePrecision(U val)
{
	// If casting from float to double, we might get
	// some unwanted extra decimal digits.
	// Make sure that after casting we have the same
	// number of digits.

	int num_digits = NumberOfDecimalDigits(std::to_string(val));

	return ToPrecision<T>(val, num_digits);
}

template <typename T>
vector<T> Values(const NcVar* var, long* lengths = 0)
{
	bool allocated = false;
	if (lengths == 0)
	{
		allocated = true;
		lengths = var->edges();
	}

	const size_t N = std::accumulate(lengths, lengths + var->num_dims(), 1, [](long a, long b) { return a * b; });

	vector<T> values(N, NFmiNetCDF::kFloatMissing);

#ifdef DEBUG
	NcBool ret =
#endif
	    var->get(values.data(), lengths);

	if (allocated)
	{
		delete[] lengths;
	}

#ifdef DEBUG
	assert(ret);
#endif

	return values;
}

NFmiNetCDF::NFmiNetCDF()
    : itsTDim(0),
      itsXDim(0),
      itsYDim(0),
      itsZDim(0),
      itsMDim(0),
      itsProjection("latitude_longitude"),
      itsZVar(0),
      itsXVar(0),
      itsYVar(0),
      itsTVar(0),
      itsMVar(0),
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
      itsMDim(0),
      itsProjection("latitude_longitude"),
      itsZVar(0),
      itsXVar(0),
      itsYVar(0),
      itsTVar(0),
      itsMVar(0),
      itsProjectionVar(0),
      itsXFlip(false),
      itsYFlip(false)
{
	Read(theInfile);
}

NFmiNetCDF::~NFmiNetCDF()
{
	itsDataFile->close();
}
bool NFmiNetCDF::Read(const string& theInfile)
{
	itsDataFile = unique_ptr<NcFile>(new NcFile(theInfile.c_str(), NcFile::ReadOnly));

	if (!itsDataFile->is_valid())
	{
		return false;
	}

	// Read metadata from file.

	if (!ReadAttributes())
		return false;

	if (!ReadDimensions())
		return false;

	if (!ReadVariables())
		return false;

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
	if (itsXDim)
		ret = itsXDim->size();
	return ret;
}

long int NFmiNetCDF::SizeY() const
{
	long ret = 0;
	if (itsYDim)
		ret = itsYDim->size();
	return ret;
}

long int NFmiNetCDF::SizeZ() const
{
	long ret = 0;
	if (itsZDim)
		ret = itsZDim->size();
	return ret;
}

long int NFmiNetCDF::SizeT() const
{
	long ret = 0;
	if (itsTDim)
		ret = itsTDim->size();
	return ret;
}

long int NFmiNetCDF::SizeParams() const
{
	return itsParameters.size();
}
// Types
nc_type NFmiNetCDF::TypeX() const
{
	return itsXVar->type();
}
nc_type NFmiNetCDF::TypeY() const
{
	return itsYVar->type();
}
nc_type NFmiNetCDF::TypeZ() const
{
	return itsZVar->type();
}
nc_type NFmiNetCDF::TypeT() const
{
	return itsTVar->type();
}
// Metadata
bool NFmiNetCDF::IsConvention() const
{
	return (!itsConvention.empty());
}
string NFmiNetCDF::Convention() const
{
	return itsConvention;
}
double NFmiNetCDF::Orientation() const
{
	double ret = kFloatMissing;

	NcVar* var = itsDataFile->get_var("stereographic");

	if (!var)
		return ret;

	auto att = unique_ptr<NcAtt>(var->get_att("longitude_of_projection_origin"));

	if (!att)
		return ret;

	auto type = att->type();
	auto val = (type == ncDouble) ? att->as_double(0) : att->as_float(0);
	return ToSamePrecision<double>(val);
}

std::string NFmiNetCDF::Projection() const
{
	return itsProjection;
}

double NFmiNetCDF::TrueLatitude() const
{
	double ret = kFloatMissing;

	NcVar* var = itsDataFile->get_var("stereographic");

	if (!var)
		return ret;

	auto att = unique_ptr<NcAtt>(var->get_att("latitude_of_projection_origin"));

	if (!att)
		return ret;

	auto type = att->type();
	auto val = (type == ncDouble) ? att->as_double(0) : att->as_float(0);
	return ToSamePrecision<double>(val);
}

// Params
void NFmiNetCDF::FirstParam()
{
	itsParamIterator = itsParameters.begin();
}
bool NFmiNetCDF::NextParam()
{
	return (++itsParamIterator < itsParameters.end());
}
NcVar* NFmiNetCDF::Param()
{
	return *itsParamIterator;
}
// Time
void NFmiNetCDF::ResetTime()
{
	itsTimeIndex = -1;
}
bool NFmiNetCDF::NextTime()
{
	itsTimeIndex++;
	if (itsTimeIndex < SizeT())
		return true;

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

long NFmiNetCDF::TimeIndex()
{
	return itsTimeIndex;
}
string NFmiNetCDF::TimeUnit()
{
	return NFmiNetCDF::Att(itsTVar, "units");
}
// Level
void NFmiNetCDF::ResetLevel()
{
	itsLevelIndex = -1;
}
bool NFmiNetCDF::NextLevel()
{
	itsLevelIndex++;

	if (itsLevelIndex < SizeZ())
		return true;

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

long NFmiNetCDF::LevelIndex()
{
	return itsLevelIndex;
}
template <typename T>
vector<T> NFmiNetCDF::Values(const std::string& theParameter)
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

template vector<float> NFmiNetCDF::Values(const std::string&);
template vector<double> NFmiNetCDF::Values(const std::string&);

template <typename T>
vector<T> NFmiNetCDF::Values()
{
	return Values<T>(Param(), TimeIndex(), LevelIndex());
}

template vector<float> NFmiNetCDF::Values();
template vector<double> NFmiNetCDF::Values();

NcVar* NFmiNetCDF::GetVariable(const string& varName) const
{
	for (unsigned int i = 0; i < itsParameters.size(); i++)
	{
		if (string(itsParameters[i]->name()) == varName)
			return itsParameters[i];
	}
	throw out_of_range("Variable '" + varName + "' does not exist");
}

NcVar* NFmiNetCDF::GetProjectionVariable() const
{
	if (!itsProjectionVar)
	{
		throw out_of_range("Projection variable does not exist");
	}

	return itsProjectionVar;
}

bool NFmiNetCDF::HasVariable(const string& name) const
{
	NcVar* var = itsDataFile->get_var(name.c_str());
	return var == nullptr ? false : true;
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
	NcDim *theXDim = 0, *theYDim = 0, *theZDim = 0, *theTDim = 0, *theMDim = 0;
	NcVar *theXVar = 0, *theYVar = 0, *theZVar = 0, *theTVar = 0, *theMVar = 0, *outvar = 0;

	boost::filesystem::path f(theFileName);
	string dir = f.parent_path().string();

	if (!boost::filesystem::exists(dir))
	{
		if (!boost::filesystem::create_directories(dir))
		{
			fmt::print("Unable to create directory {}\n", dir);
			return false;
		}
	}

	NcFile theOutFile(theFileName.c_str(), NcFile::Replace);

	if (!theOutFile.is_valid())
	{
		fmt::print("Unable to create file {}\n", theFileName);
		return false;
	}

	// Dimensions

	if (!(theXDim = theOutFile.add_dim(itsXDim->name(), SizeX())))
	{
		return false;
	}

	if (!(theYDim = theOutFile.add_dim(itsYDim->name(), SizeY())))
	{
		return false;
	}

	// Add z dimension if it exists in the original data

	if (itsZDim)
	{
		if (!(theZDim = theOutFile.add_dim(itsZDim->name(), 1)))
		{
			return false;
		}
	}

	// Our unlimited dimension

	if (itsTDim)
	{
		if (!(theTDim = theOutFile.add_dim(itsTDim->name())))
		{
			return false;
		}
	}

	// ensemble member dimension
	if (itsMDim)
	{
		const size_t sizeM = itsMDim->size();
		if (!(theMDim = theOutFile.add_dim(itsMDim->name(), sizeM)))
		{
			return false;
		}
	}

	// Variables

	// x

	if (!CopyVar(&theXVar, itsXVar, &theOutFile, nullptr, nullptr))
	{
		return false;
	}

	/*
	    if (itsXFlip) reverse(xValues.begin(), xValues.end());

	*/
	// y

	if (!CopyVar(&theYVar, itsYVar, &theOutFile, nullptr, nullptr))
	{
		return false;
	}

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

		if (zValue == kFloatMissing)
			zValue = 0;

		if (!theZVar->put(&zValue, 1))
			return false;
	}

	// t

	long time_index = TimeIndex();
	long time_size = 1;
	if (!CopyVar(&theTVar, itsTVar, &theOutFile, &time_index, &time_size))
	{
		return false;
	}

	// ensemble member variable

	if (itsMDim && theMDim)
	{
		if (!CopyVar(&theMVar, itsMVar, &theOutFile, nullptr, nullptr))
		{
			return false;
		}
#if 0
		if (!(theMVar = theOutFile.add_var(itsMDim->name(), ncShort, theMDim)))
			return false;

		CopyAtts(theMVar, itsMVar);

		const long sizeM = itsMDim->size();
		auto mValues = ::Values<short>(itsMVar);
		if (!theMVar->put(mValues.data(), sizeM))
		{
			return false;
		}
#endif
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
				fmt::print("NcType {} is not supported for variable {}\n", itsProjectionVar->type(),
				           itsProjectionVar->name());
				break;
		}
		CopyAtts(outprojvar, itsProjectionVar);

		// Check if "longitude" and "latitude" variables exist
		NcVar* lon = itsDataFile->get_var("longitude");
		NcVar* lat = itsDataFile->get_var("latitude");

		if (itsProjection == "polar_stereographic" && lon && lat)
		{
			NcVar *outlon = 0, *outlat = 0;

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
			for (size_t i = 0; i < cursors.size(); i++)
				totalSize *= cursors[i];

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
	vector<long> cursor_position(num_dims), dimension_length(num_dims);

	for (int i = 0; i < num_dims; i++)
	{
		string dimname = var->get_dim(i)->name();

		if (dimname == itsTDim->name())
		{
			dims[i] = theTDim;
			cursor_position[i] = TimeIndex();
			dimension_length[i] = 1;
		}
		else if (theZDim && dimname == itsZDim->name())
		{
			dims[i] = theZDim;
			cursor_position[i] = LevelIndex();
			dimension_length[i] = 1;
		}
		else if (dimname == itsXDim->name())
		{
			dims[i] = theXDim;
			cursor_position[i] = 0;
			dimension_length[i] = itsXDim->size();
		}
		else if (dimname == itsYDim->name())
		{
			dims[i] = theYDim;
			cursor_position[i] = 0;
			dimension_length[i] = itsYDim->size();
		}
		else if (theMDim && dimname == itsMDim->name())
		{
			dims[i] = theMDim;
			cursor_position[i] = 1;
			dimension_length[i] = 1;
		}
	}

	CopyVar(&outvar, var, &theOutFile, cursor_position.data(), dimension_length.data());

	// Global attributes

	const auto atts = ReadGlobalAttributes(itsDataFile.get());

	for (const auto& p : atts)
	{
		theOutFile.add_att(p.first.c_str(), p.second.c_str());
	}

	if (!itsConvention.empty())
	{
		if (!theOutFile.add_att("Conventions", itsConvention.c_str()))
		{
			return false;
		}
	}

	time_t now;
	time(&now);

	string datetime = ctime(&now);

	// ctime adds implicit newline

	datetime.erase(remove(datetime.begin(), datetime.end(), '\n'), datetime.end());

	if (!theOutFile.add_att("file_creation_time", datetime.c_str()))
	{
		return false;
	}

	if (!itsInstitution.empty())
	{
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
 * If itsXFlip is set, when writing slice to NetCDF of CSV file the values of X axis
 * are flipped, ie. from 1 2 3 we have 3 2 1.
 *
 * Same applies for Y axis.
 */

bool NFmiNetCDF::FlipX()
{
	return itsXFlip;
}
void NFmiNetCDF::FlipX(bool theXFlip)
{
	itsXFlip = theXFlip;
}
bool NFmiNetCDF::FlipY()
{
	return itsYFlip;
}
void NFmiNetCDF::FlipY(bool theYFlip)
{
	itsYFlip = theYFlip;
}

double Resolution(NcVar* var, long size)
{
	float a = var->as_float(0);
	float b = var->as_float(size - 1);
	long range = size;
	float delta;

	const std::string missing = NFmiNetCDF::Att(var, "missing_value");

	if (missing.empty() == false && (a == std::stod(missing) || b == std::stod(missing)))
	{
		// case nemo
		// only sea points have latitude and longitude defined
		int i = -1;
		float fmissing = std::stof(missing);

		do
		{
			a = var->as_float(++i);
		} while (a == fmissing && i < range * 2);

		do
		{
			b = var->as_float(++i);
		} while ((b == fmissing || a == b) && i < range * 3);

		if (a == fmissing || b == fmissing)
		{
			// exploding head
			fmt::print("Found only invalid or constant coordinates for {}\n", var->name());
		}
		delta = fabs(b - a);
		range = 2;
	}
	else
	{
		delta = fabs(b - a);
	}

	string units = NFmiNetCDF::Att(var, "units");
	if (!units.empty())
	{
		if (units == "100  km")
			delta *= 100;
	}

	float reso = delta / static_cast<float>(range - 1);
	int num_digits = NumberOfDecimalDigits(std::to_string(a));
	return ToPrecision<double>(reso, num_digits);
}

double NFmiNetCDF::XResolution()
{
	return Resolution(itsXVar, SizeX());
}

double NFmiNetCDF::YResolution()
{
	return Resolution(itsYVar, SizeY());
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

bool NFmiNetCDF::HasDimension(const std::string& dimName)
{
	return HasDimension(Param(), dimName);
}
std::string NFmiNetCDF::Att(const std::string& attName)
{
	return NFmiNetCDF::Att(Param(), attName);
}

// private functions
bool NFmiNetCDF::HasDimension(const NcVar* var, const std::string& dimName)
{
	long num_dims = var->num_dims();
	if (dimName == "z" && !itsZDim)
		return false;

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
		else if (dimName == "ensemble_member" || dimName == "member")
			dimname = itsMDim->name();

		if (static_cast<string>(dim->name()) == dimname)
			return true;
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
			switch (att->type())
			{
				case ncFloat:
					ret = std::to_string(att->as_float(0));
					break;
				case ncDouble:
					ret = std::to_string(att->as_double(0));
					break;
				case ncByte:
					ret = std::to_string(att->as_ncbyte(0));
					break;
				case ncShort:
					ret = std::to_string(att->as_short(0));
					break;
				case ncInt:
					ret = std::to_string(att->as_int(0));
					break;
				case ncChar:
				{
					const auto ptr = unique_ptr<char[]>(att->as_string(0));
					ret = static_cast<string>(ptr.get());
				}
				break;
				default:
					break;
			}
		}
	}

	return ret;
}

template <typename T>
vector<T> NFmiNetCDF::Values(NcVar* var, long timeIndex, long levelIndex)
{
	int num_dims = static_cast<size_t>(var->num_dims());

	vector<long> cursor_position(num_dims), dimsizes(num_dims);

	for (int i = 0; i < num_dims; i++)
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
			dimsize = 1;  // XXX METAN has dimsize == 2, (y, x)
		}

		cursor_position[i] = index;
		dimsizes[i] = dimsize;
	}

	var->set_cur(&cursor_position[0]);

	size_t dims = std::accumulate(dimsizes.begin(), dimsizes.end(), 1, [](long a, long b) { return a * b; });
	vector<T> values(dims, static_cast<T>(kFloatMissing));
	var->get(values.data(), dimsizes.data());

	return values;
}

template vector<float> NFmiNetCDF::Values(NcVar*, long, long);
template vector<double> NFmiNetCDF::Values(NcVar*, long, long);

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

		if (name == "level" || name == "lev" || name.find("depth") != string::npos || name == "pressure" ||
		    name == "height" || name == "hybrid")
		{
			itsZDim = dim;
		}

		if (name == "ensemble_member" || name == "member")
		{
			itsMDim = dim;
		}
	}

	if (!itsYDim || !itsXDim || (!itsTDim || !itsTDim->is_valid()))
	{
		fmt::print("All required dimensions not found\n");
		fmt::print("x dim: {}\n", (itsXDim ? "found" : "not found"));
		fmt::print("y dim: {}\n", (itsYDim ? "found" : "not found"));
		fmt::print("t dim: {}\n", (itsTDim ? "found" : "not found"));
		fmt::print("t dim is valid: {}\n", (itsTDim && itsTDim->is_valid() ? "yes" : "no"));
		fmt::print("z dim (optional): {}\n", (itsZDim ? "found" : "not found"));
		fmt::print("m dim (optional): {}\n", (itsMDim ? "found" : "not found"));
		return false;
	}

	return true;
}

bool NFmiNetCDF::ReadVariables()
{
	const auto ResolutionDrift = [](const vector<float>& tmp) -> float
	{
		// Check resolution

		float resolution = 0;
		float prevResolution = resolution;

		float prevX = tmp[0];

		for (unsigned int k = 1; k < tmp.size(); k++)
		{
			resolution = tmp[k] - prevX;

			if (tmp[k] == -1 || prevX == -1)
			{
				prevX = tmp[k];
				continue;
			}
			if (k == 1)
				prevResolution = resolution;

			if (abs(resolution - prevResolution) > MAX_COORDINATE_RESOLUTION_ERROR)
			{
				return abs(resolution - prevResolution);
			}

			prevResolution = resolution;
			prevX = tmp[k];
		}

		return 0.0f;
	};

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

		if (itsZDim && (varname == static_cast<string>(itsZDim->name()) || NFmiNetCDF::Att(var, "axis") == "Z"))
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
		else if (varname == static_cast<string>(itsXDim->name()) ||
		         NFmiNetCDF::Att(var, "standard_name") == "longitude" ||
		         NFmiNetCDF::Att(var, "standard_name") == "projection_x_coordinate")
		{
			// X-coordinate
			// projected files might have multiple coordinate variables, for example
			// x&y for projected coordinates
			// lon&lat for geographic coordinates
			// If data is projected, then the evenly spaced grid is most likely
			// created using projected coordinates, and that is hopefully marked
			// by using attribute 'axis'.
			// Therefore if a variable has attribute axis set, do not override
			// with other coordinate variables.

			if (itsXVar && NFmiNetCDF::Att(itsXVar, "axis") == "X")
			{
				continue;
			}

			itsXVar = var;

			auto tmp = ::Values<float>(itsXVar);

			if (itsXFlip)
			{
				reverse(tmp.begin(), tmp.end());
			}

			if (tmp.size() > 1)
			{
				if (xCoordinateWarning && ResolutionDrift(tmp) > 0.0f)
				{
					fmt::print("Warning: X dimension resolution is not constant: {}\n", ResolutionDrift(tmp));
					xCoordinateWarning = false;
				}
			}

			continue;
		}
		else if (varname == static_cast<string>(itsYDim->name()) || NFmiNetCDF::Att(var, "standard_name") == "latitude")
		{
			// Y-coordinate

			if (itsYVar && NFmiNetCDF::Att(itsYVar, "axis") == "Y")
			{
				continue;
			}

			itsYVar = var;

			auto tmp = ::Values<float>(itsYVar);

			if (itsYFlip)
			{
				reverse(tmp.begin(), tmp.end());
			}

			if (tmp.size() > 1)
			{
				if (yCoordinateWarning && ResolutionDrift(tmp) > 0.0f)
				{
					fmt::print("Warning: Y dimension resolution is not constant: {}\n", ResolutionDrift(tmp));
					yCoordinateWarning = false;
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
		else if (itsMDim && varname == static_cast<string>(itsMDim->name()))
		{
			// METNO has ensemble member as dimension and variable.
			itsMVar = var;

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
				const auto ptr = unique_ptr<char[]>(att->as_string(0));
				itsProjection = string(ptr.get());

				itsProjectionVar = var;
				foundproj = true;
				break;
			}
		}

		if (foundproj || varname == "latitude" || varname == "longitude")
			continue;

		itsParameters.push_back(var);
	}

	assert(itsXVar && itsYVar && itsTVar);

	return true;
}

vector<pair<string, string>> ReadGlobalAttributes(NcFile* theFile)
{
	vector<pair<string, string>> ret;

	for (int i = 0; i < theFile->num_atts(); i++)
	{
		const auto att = unique_ptr<NcAtt>(theFile->get_att(i));
		const auto ptr = unique_ptr<char[]>(att->as_string(0));
		ret.emplace_back(att->name(), string(ptr.get()));
	}

	return ret;
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

	auto atts = ReadGlobalAttributes(itsDataFile.get());

	for (const auto& p : atts)
	{
		if (p.first == "Conventions")
		{
			itsConvention = p.second;
		}
	}

	return true;
}

// free functions
bool CopyAtts(NcVar* newvar, const NcVar* oldvar)
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
					if (!newvar->add_att(att->name(), att->as_float(0)))
						return false;
					break;

				case ncDouble:
					if (!newvar->add_att(att->name(), att->as_double(0)))
						return false;
					break;

				case ncShort:
					if (!newvar->add_att(att->name(), att->as_short(0)))
						return false;
					break;

				case ncInt:
					if (!newvar->add_att(att->name(), att->as_int(0)))
						return false;
					break;
				case ncChar:
					if (!newvar->add_att(att->name(), att->as_char(0)))
						return false;
					break;
				case ncByte:
					if (!newvar->add_att(att->name(), att->as_ncbyte(0)))
						return false;
					break;
				case ncNoType:
				default:
					fmt::print("NcType {} not supported for variable {}\n", oldvar->type(), oldvar->name());
					break;
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
			const auto ptr = unique_ptr<char[]>(att->as_string(0));
			if (!newvar->add_att(att->name(), ptr.get()))
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
			const auto ptr = unique_ptr<char[]>(att->as_string(0));
			if (!newvar->add_att(att->name(), ptr.get()))
			{
				return false;
			}
		}
	}

	return true;
}

bool CopyVar(NcVar** newvar, NcVar* oldvar, NcFile* theOutFile, long* dimension_position, long* dimension_length)
{
	// dimension_position: where to start copying from
	// dimension_length: how large a chunk to copy
	//
	// if either are not set, default is to start from 0,0,0,..
	// and copy everything
	//
	// this is what we want for example for geographic coordinates
	//
	// for time dimensions we only want to copy the current time step

	const int ndims = oldvar->num_dims();

	bool allocated = false;

	if (!dimension_position)
	{
		allocated = true;
		dimension_position = new long[ndims];
		dimension_length = new long[ndims];
	}

	vector<NcDim*> dims;
	dims.reserve(ndims);

	for (int i = 0; i < ndims; i++)
	{
		const auto name = oldvar->get_dim(i)->name();
		for (int j = 0; j < theOutFile->num_dims(); j++)
		{
			NcDim* d = theOutFile->get_dim(j);

			if (strcmp(d->name(), name) == 0)
			{
				dims.push_back(d);

				if (allocated)
				{
					dimension_position[i] = 0;
					dimension_length[i] = dims[i]->size();
				}
			}
		}
	}

	if (dims.size() != static_cast<size_t>(ndims))
	{
		// the file did not have correct dimensions (the same what oldvar has)
		return false;
	}

	const NcDim** dimptr = const_cast<const NcDim**>(dims.data());

	switch (oldvar->type())
	{
		case ncFloat:
			(*newvar) = theOutFile->add_var(oldvar->name(), ncFloat, ndims, dimptr);
			break;
		case ncDouble:
			(*newvar) = theOutFile->add_var(oldvar->name(), ncDouble, ndims, dimptr);
			break;
		case ncByte:
			(*newvar) = theOutFile->add_var(oldvar->name(), ncByte, ndims, dimptr);
			break;
		case ncShort:
			(*newvar) = theOutFile->add_var(oldvar->name(), ncShort, ndims, dimptr);
			break;
		case ncChar:
			(*newvar) = theOutFile->add_var(oldvar->name(), ncChar, ndims, dimptr);
			break;
		case ncInt:
			(*newvar) = theOutFile->add_var(oldvar->name(), ncInt, ndims, dimptr);
			break;
		default:
			break;
	}

	if (!(*newvar))
	{
		return false;
	}

	CopyAtts(*newvar, oldvar);

	assert(*newvar);
	if (!oldvar->set_cur(dimension_position))
	{
		return false;
	}

	auto values = ::Values<float>(oldvar, dimension_length);

	if (!(*newvar)->put(values.data(), dimension_length))
	{
		return false;
	}

	if (allocated)
	{
		delete[] dimension_position;
		delete[] dimension_length;
	}

	return true;
}

template <typename T>
T NFmiNetCDF::Lat0()
{
	T ret = kFloatMissing;
	auto var = itsDataFile->get_var("latitude");

	if (var)
	{
		auto type = var->type();
		auto val = (type == ncDouble) ? var->as_double(0) : var->as_float(0);
		ret = ToSamePrecision<T>(val);
	}

	return ret;
}

template float NFmiNetCDF::Lat0<float>();
template double NFmiNetCDF::Lat0<double>();

template <typename T>
T NFmiNetCDF::Lon0()
{
	T ret = kFloatMissing;
	auto var = itsDataFile->get_var("longitude");
	if (var)
	{
		auto type = var->type();
		auto val = (type == ncDouble) ? var->as_double(0) : var->as_float(0);
		ret = ToSamePrecision<T>(val);
	}
	return ret;
}

template float NFmiNetCDF::Lon0<float>();
template double NFmiNetCDF::Lon0<double>();

template <typename T>
T NFmiNetCDF::X0()
{
	T ret = kFloatMissing;
	if (Projection() == "polar_stereographic")
	{
		auto var = itsDataFile->get_var("longitude");
		if (var)
		{
			auto type = var->type();
			auto val = (type == ncDouble) ? var->as_double(0) : var->as_float(0);
			ret = ToSamePrecision<T>(val);
		}
	}
	else
	{
		assert(itsXVar);
		auto type = itsXVar->type();
		auto val = (type == ncDouble) ? itsXVar->as_double(0) : itsXVar->as_float(0);
		ret = ToSamePrecision<T>(val);
	}
	return ret;
}

template float NFmiNetCDF::X0<float>();
template double NFmiNetCDF::X0<double>();

template <typename T>
T NFmiNetCDF::Y0()
{
	T ret = kFloatMissing;

	if (Projection() == "polar_stereographic")
	{
		auto var = itsDataFile->get_var("latitude");
		if (var)
		{
			auto type = var->type();
			auto val = (type == ncDouble) ? var->as_double(0) : var->as_float(0);
			ret = ToSamePrecision<T>(val);
		}
	}
	else
	{
		assert(itsYVar);
		auto type = itsYVar->type();
		auto val = (type == ncDouble) ? itsYVar->as_double(0) : itsYVar->as_float(0);
		ret = ToSamePrecision<T>(val);
	}
	return ret;
}

template float NFmiNetCDF::Y0<float>();
template double NFmiNetCDF::Y0<double>();

template <typename T>
T NFmiNetCDF::X1()
{
	T ret = kFloatMissing;

	if (Projection() == "polar_stereographic")
	{
		auto var = itsDataFile->get_var("longitude");
		if (var)
		{
			auto type = var->type();
			auto val = (type == ncDouble) ? var->as_double(0) : var->as_float(0);
			ret = ToSamePrecision<T>(val);
		}
	}
	else
	{
		assert(itsXVar);
		auto type = itsXVar->type();
		size_t num = itsXVar->num_vals() - 1;
		auto val = (type == ncDouble) ? itsXVar->as_double(num) : itsXVar->as_float(num);
		ret = ToSamePrecision<T>(val);
	}
	return ret;
}
template float NFmiNetCDF::X1<float>();
template double NFmiNetCDF::X1<double>();

template <typename T>
T NFmiNetCDF::Y1()
{
	T ret = kFloatMissing;

	if (Projection() == "polar_stereographic")
	{
		auto var = itsDataFile->get_var("latitude");
		if (var)
		{
			auto type = var->type();
			auto val = (type == ncDouble) ? var->as_double(0) : var->as_float(0);
			ret = ToSamePrecision<T>(val);
		}
	}
	else
	{
		assert(itsYVar);
		auto type = itsYVar->type();
		size_t num = itsYVar->num_vals() - 1;
		auto val = (type == ncDouble) ? itsYVar->as_double(num) : itsYVar->as_float(num);
		ret = ToSamePrecision<T>(val);
	}
	return ret;
}

template float NFmiNetCDF::Y1<float>();
template double NFmiNetCDF::Y1<double>();
