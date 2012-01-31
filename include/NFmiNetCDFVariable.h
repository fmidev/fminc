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

    long Size();
    bool Init(NcVar *theVariable);
    std::vector<float> Values();

    float Value(int num);

    bool HasDimension(NcDim *dim) const;

  private:
    std::string Att(std::string attName);
    NcVar *itsParam;

};
