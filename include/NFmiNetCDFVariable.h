#include <string>
#include <netcdfcpp.h>
#include <vector>

class NFmiNetCDFVariable {
  public:
    NFmiNetCDFVariable();
    NFmiNetCDFVariable(NcVar *theParam);
    ~NFmiNetCDFVariable();

    std::string Name();
    void Name(std::string theName);

    std::string Unit();
    void Unit(std::string theUnit);

    std::string LongName();
    std::string StandardName();
    std::string Axis();
    std::string Positive();
    float FillValue();
    float MissingValue();

    long Size();
    bool Init(NcVar *theVariable);
    bool Initialized();

    std::vector<float> Values(long timeIndex = -1, long levelIndex = -1);

    bool HasDimension(NcDim *dim) const;

    void ResetValue();
    bool NextValue();
    float Value();
    long Index();
    void Index(long theValuePointer);

    NcDim* Dimension(int num);
    int SizeDimensions();

    /*
     * In netcdf we might have coordinate dimensions for a variable either in order
     * longitude,latitude
     * OR
     * latitude, longitude
     *
     * The former is column major style (fortran), and the latter is row major style (c).
     * The column major data must be transposed before it can be stored to a c-array.
     * (This function does not do the transpose)
     */
 
    bool CoordinatesInRowMajorOrder();
  private:

    std::string Att(std::string attName);
    long itsValuePointer;
    NcVar *itsParam;
   
    NcDim *itsTDim;
    NcDim *itsZDim;
    NcDim *itsXDim;
    NcDim *itsYDim;

};
