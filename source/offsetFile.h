#include <map>

class offsetFile
{
private:
  std::map<std::string, std::array<u64, 2>> offsetMap;
public:
  offsetFile(std::string offsetDBPath)
  {
    std::ifstream offsets(offsetDBPath);
    std::string filename;
    std::string offset;
    std::string compSize;
    while(getline(offsets, filename, ',')) {
      getline(offsets, offset, ',');
      getline(offsets, compSize);
      std::array<u64, 2> data = {strtoul(offset.c_str(), NULL, 16), strtoul(compSize.c_str(), NULL, 16)};
      offsetMap.try_emplace(filename, data);
    }
  }
  u64 getOffset(std::string arcFilePath)
  {
    auto fileIt = offsetMap.find(arcFilePath);
    if (fileIt != offsetMap.end()) {
      return fileIt->second[0];
    }
    else
      return 0;
  }
  u64 getCompSize(std::string arcFilePath)
  {
    auto fileIt = offsetMap.find(arcFilePath);
    if (fileIt != offsetMap.end()) {
      return fileIt->second[1];
    }
    else
      return 0;
  }
};
