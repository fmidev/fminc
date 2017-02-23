/*
 * class NFmiNetCDF.cpp
 *
 * General library to access NetCDF files.
 */

#include <memory>
#include <netcdfcpp.h>
#include <string>
#include <vector>

class NFmiNetCDF
{
   public:
	NFmiNetCDF();
	NFmiNetCDF(const std::string& theInfile);
	~NFmiNetCDF();

	static const float kFloatMissing;

	bool Read(const std::string& theInfile);

	unsigned int Process();
	void Process(unsigned int theProcess);

	long int SizeX();
	long int SizeY();
	long int SizeZ();
	long int SizeT();
	long int SizeParams();

	bool IsConvention();
	std::string Convention();

	void ResetTime();
	bool NextTime();
	template <typename T>
	T Time();
	long TimeIndex();
	std::string TimeUnit();

	void ResetLevel();
	bool NextLevel();
	float Level();
	long LevelIndex();

	void FirstParam();
	bool NextParam();
	NcVar* Param();

	template <typename T>
	T X0();
	template <typename T>
	T Y0();

	template <typename T>
	T X1();
	template <typename T>
	T Y1();

	float Orientation();
	float TrueLatitude();

	float ValueT(long num);
	float ValueZ(long num);

	std::string Projection();

	template <typename T>
	std::vector<T> Values(std::string theParameter);

	template <typename T>
	std::vector<T> Values();

	bool WriteSlice(const std::string& theFileName);

	bool WriteSliceToCSV(const std::string& theFileName);

	bool FlipX();
	void FlipX(bool theXFlip);

	bool FlipY();
	void FlipY(bool theYFlip);

	float XResolution();
	float YResolution();

	NcVar* GetVariable(const std::string& varName);
	bool CoordinatesInRowMajorOrder(const NcVar* var);

	bool HasDimension(const std::string& dimName);
	std::string Att(const std::string& attName);

   private:
	bool HasDimension(const NcVar* var, const std::string& dim);
	std::string Att(NcVar* var, const std::string& attName);

	template <typename T>
	std::vector<T> Values(NcVar* var);

	template <typename T>
	std::vector<T> Values(NcVar* var, long timeIndex, long levelIndex = -1);

	bool ReadDimensions();
	bool ReadVariables();
	bool ReadAttributes();

	bool Write(const std::vector<float>& data, NcFile* theOutFile);

	NcDim* itsTDim;
	NcDim* itsXDim;
	NcDim* itsYDim;
	NcDim* itsZDim;

	std::unique_ptr<NcFile> itsDataFile;

	std::string itsConvention;
	std::string itsProjection;
	std::string itsInstitution;

	std::vector<NcVar*> itsParameters;
	std::vector<NcVar*>::iterator itsParamIterator;
	NcVar* itsZVar;
	NcVar* itsXVar;
	NcVar* itsYVar;
	NcVar* itsTVar;
	NcVar* itsProjectionVar;

	bool itsXFlip;
	bool itsYFlip;

	long itsParamIndex;
	long itsTimeIndex;
	long itsLevelIndex;
};

template <typename T>
T NFmiNetCDF::X1()
{
	T ret = kFloatMissing;

	if (Projection() == "polar_stereographic")
	{
		auto lonVar = itsDataFile->get_var("longitude");
		if (lonVar) ret = lonVar->as_float(lonVar->num_vals() - 1);
	}
	else
	{
		ret = static_cast<T>(itsXVar->as_float(itsXVar->num_vals() - 1));
	}
	return ret;
}

template <typename T>
T NFmiNetCDF::Y1()
{
	T ret = kFloatMissing;

	if (Projection() == "polar_stereographic")
	{
		auto latVar = itsDataFile->get_var("latitude");
		if (latVar) ret = static_cast<T>(latVar->as_float(latVar->num_vals() - 1));
	}
	else
	{
		ret = static_cast<T>(itsYVar->as_float(itsYVar->num_vals() - 1));
	}
	return ret;
}

template float NFmiNetCDF::X1<float>();
template double NFmiNetCDF::X1<double>();
