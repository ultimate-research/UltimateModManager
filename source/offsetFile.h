#include <map>

class offsetFile
{
private:
  std::map<std::string, u64> offsetMap;
public:
  offsetFile(std::string offsetDBPath)
  {
    std::ifstream offsets(offsetDBPath);
    std::string filename;
    std::string offset;
    while(getline(offsets, filename, ',')) {
      getline(offsets, offset);
      offsetMap.emplace(filename, strtoul(offset.c_str(), NULL, 16));
    }
  }
  u64 getOffset(std::string arcFilePath)
  {
    auto fileIt = offsetMap.find(arcFilePath);
    if (fileIt != offsetMap.end()) {
      return fileIt->second;
    }
    else
      return 0;
  }
};
