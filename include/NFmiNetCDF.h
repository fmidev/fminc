/*
 * class NetCDFReader.cpp
 *
 * Used to load NetCDF data into NEONS.
 */

#include <string>
#include <vector>
#include <netcdfcpp.h>
#include "NFmiNetCDFVariable.h"

class NFmiNetCDF {

  public:
    NFmiNetCDF();
    NFmiNetCDF(const std::string &theInfile);
    ~NFmiNetCDF();

    bool Read(const std::string &theInfile);

    std::string AnalysisTime();
    void AnalysisTime(std::string theAnalysisTime);

    unsigned int Process();
    void Process(unsigned int theProcess);

    std::vector<float> Slice(unsigned int theLevel,std::string theParameter);

    long int SizeX();
    long int SizeY();
    long int SizeZ();
    long int SizeT();
    long int SizeParams();

    bool IsConvention();
    std::string Convention();

    bool FirstTime();
    bool NextTime();

    bool FirstLocation();
    bool NextLocation();

    bool FirstLevel();
    bool NextLevel();

    void FirstParam();
    bool NextParam();
    NFmiNetCDFVariable Param();

    float X0();
    float Y0();

    float ValueT(int num);
    float ValueZ(int num);

    std::string Projection();
    std::vector<float> Values(std::string theParameter);
    std::vector<float> Values(int numOfParameter = 0);

    bool HasDimension(const NFmiNetCDFVariable &var, const std::string &dim);

  private:

    bool ReadDimensions();
    bool ReadVariables();
    bool ReadAttributes();
    std::vector<std::string> Split();
    bool Write(const std::vector<float> &data, NcFile *theOutFile);

    NcDim *t;
    NcDim *x;
    NcDim *y;
    NcDim *z;

    NcFile *dataFile;

    std::string itsConvention;
    std::string itsProjection;
    unsigned int itsProcess;

    std::vector<NFmiNetCDFVariable> itsParameters;
    NFmiNetCDFVariable itsZ;
    NFmiNetCDFVariable itsX;
    NFmiNetCDFVariable itsY;
    NFmiNetCDFVariable itsT;

    std::vector<double>::iterator itsZIterator;
    std::vector<double>::iterator itsXIterator;
    std::vector<double>::iterator itsYIterator;
    std::vector<double>::iterator itsTiterator;
    std::vector<NFmiNetCDFVariable>::iterator itsParamIterator;

    bool itsZIsPositive;

    std::string itsZUnit;
    std::string itsTUnit;

    long itsCentre;
    std::string itsAnalysisTime;

    /* Support only squares */

    float itsX0;
    float itsX1;
    float itsY0;
    float itsY1;

    short itsStep; // minutes


};
