/*
 * class NFmiNetCDF.cpp
 *
 * General library to access NetCDF files.
 */

#include <cassert>
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

	long int SizeX() const;
	long int SizeY() const;
	long int SizeZ() const;
	long int SizeT() const;
	long int SizeParams() const;

	nc_type TypeX() const;
	nc_type TypeY() const;
	nc_type TypeZ() const;
	nc_type TypeT() const;

	bool IsConvention() const;
	std::string Convention() const;

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

	template <typename T>
	T Lat0();

	template <typename T>
	T Lon0();

	float Orientation() const;
	std::string Projection() const;
	float TrueLatitude() const;

	template <typename T>
	std::vector<T> Values(const std::string& theParameter);

	template <typename T>
	std::vector<T> Values();

	bool WriteSlice(const std::string& theFileName);

	bool FlipX();
	void FlipX(bool theXFlip);

	bool FlipY();
	void FlipY(bool theYFlip);

	float XResolution();
	float YResolution();

	NcVar* GetVariable(const std::string& varName) const;
	bool HasVariable(const std::string& name) const;
	bool CoordinatesInRowMajorOrder(const NcVar* var);

	bool HasDimension(const std::string& dimName);
	std::string Att(const std::string& attName);

	NcVar* GetProjectionVariable() const;
	static std::string Att(NcVar* var, const std::string& attName);

   private:
	bool HasDimension(const NcVar* var, const std::string& dim);

	template <typename T>
	std::vector<T> Values(NcVar* var, long timeIndex, long levelIndex = -1);

	bool ReadDimensions();
	bool ReadVariables();
	bool ReadAttributes();

	NcDim* itsTDim;
	NcDim* itsXDim;
	NcDim* itsYDim;
	NcDim* itsZDim;
	NcDim* itsMDim;

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
	NcVar* itsMVar;
	NcVar* itsProjectionVar;

	bool itsXFlip;
	bool itsYFlip;

	long itsParamIndex;
	long itsTimeIndex;
	long itsLevelIndex;
};
