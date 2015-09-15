/*
 * class NFmiNetCDF.cpp
 *
 * General library to access NetCDF files.
 */

#include <string>
#include <vector>
#include <netcdfcpp.h>
#include <memory>

class NFmiNetCDF {

  public:
    NFmiNetCDF();
    NFmiNetCDF(const std::string &theInfile);
    ~NFmiNetCDF();

    bool Read(const std::string &theInfile);

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

    // Different syntax

    void FirstParam();
    bool NextParam();
    NcVar* Param();

    template <typename T>
    T X0();

    template <typename T>
    T Y0();

    float Orientation();

    float ValueT(long num);
    float ValueZ(long num);

    std::string Projection();

    template <typename T>
    std::vector<T> Values(std::string theParameter);

    template <typename T>
    std::vector<T> Values();

    bool WriteSlice(const std::string &theFileName);

    bool WriteSliceToCSV(const std::string &theFileName);

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

    bool HasDimension(const NcVar* var, const std::string &dim);
    std::string Att(NcVar* var, const std::string& attName);

    template <typename T>
    std::vector<T> Values(NcVar* var);

    template <typename T>
    std::vector<T> Values(NcVar* var, long timeIndex, long levelIndex = -1);

    bool ReadDimensions();
    bool ReadVariables();
    bool ReadAttributes();
 
    bool Write(const std::vector<float> &data, NcFile *theOutFile);

    NcDim *itsTDim;
    NcDim *itsXDim;
    NcDim *itsYDim;
    NcDim *itsZDim;

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

    bool itsXFlip;
    bool itsYFlip;
    
    long itsParamIndex;
    long itsTimeIndex;
    long itsLevelIndex;
};
