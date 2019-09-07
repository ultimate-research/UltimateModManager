#include <map>

class offsetFile
{
private:
  std::map<std::string, std::array<u64, 3>> offsetMap;
public:
  offsetFile(std::string offsetDBPath)
  {
    std::ifstream offsets(offsetDBPath);
    std::string filename;
    std::string offset;
    std::string compSize;
    std::string decompSize;
    getline(offsets, filename);  // first line has version info
    while(getline(offsets, filename, ',')) {
      getline(offsets, offset, ',');
      getline(offsets, compSize, ',');
      getline(offsets, decompSize);
      std::array<u64, 3> data = {strtoul(offset.c_str(), NULL, 16), strtoul(compSize.c_str(), NULL, 16), strtoul(decompSize.c_str(), NULL, 16)};
      offsetMap.try_emplace(filename, data);
    }
  }
  std::array<u64, 3> getKey(std::string arcFilePath)
  {
    auto fileIt = offsetMap.find(arcFilePath);
    if (fileIt != offsetMap.end()) {
      return fileIt->second;
    }
    else
      return std::array<u64, 3> {0,0,0};
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
  u64 getDecompSize(std::string arcFilePath)
  {
    auto fileIt = offsetMap.find(arcFilePath);
    if (fileIt != offsetMap.end()) {
      return fileIt->second[2];
    }
    else
      return 0;
  }
};
