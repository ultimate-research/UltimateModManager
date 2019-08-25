#include <map>

class offsetFile
{
private:
  std::map<std::string, u64> offsetMap;
public:
  offsetFile(std::string offsetDBPath)
  {
    std::ifstream offsets(offsetDBPath);
    std::string line;
    std::string filename;
    u64 offset;
    int splitpos;
    while(getline(offsets, line)) {
      splitpos = line.find(',');
      filename = line.substr(0, splitpos);
      offset = strtoul(line.substr(splitpos + 1).c_str(), NULL, 16);
      offsetMap.insert({filename, offset});
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
