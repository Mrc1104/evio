/*
 *  evioBankIndex.cc
 *
 *   Author:  Elliott Wolin, JLab, 1-May-2012
*/


#include "evioBankIndex.hxx"
#include "evioUtil.hxx"

using namespace evio;
using namespace std;



//--------------------------------------------------------------
//--------------------------------------------------------------


// used internally to stream-parse the event and fill the evioDictEntry map
class myHandler : public evioStreamParserHandler {
  
public:
  explicit myHandler(int maxDepth) : maxDepth(maxDepth) {}

  void *containerNodeHandler(int bankLength, int containerType, int contentType, uint16_t tag, uint8_t num, 
                             int depth, const uint32_t *bankPointer, int dataLength, const uint32_t *data, void *userArg) {

    // don't index beyond specified depth
    // Note...maxDepth 0 means index all levels, 1 means just include children level, etc.
    // Note...current depth 1 means you are at children level, 2 means grandchildren level, etc.
    if((maxDepth>0)&&(depth>maxDepth))return(userArg);


    // adds bank index to map
    auto *bi = static_cast<evioBankIndex*>(userArg);
  
    bankIndex b;
    b.containerType=containerType;
    b.contentType=contentType;
    b.depth=depth;
    b.bankPointer=bankPointer;
    b.bankLength=bankLength;
    b.data=data;
    b.dataLength=dataLength;
    bi->tagNumMap.insert(bankIndexMap::value_type(evioDictEntry(tag,num),b));
    
    return(userArg);
  }
  
  
  //--------------------------------------------------------------
  
  
  void *leafNodeHandler(int bankLength, int containerType, int contentType, uint16_t tag, uint8_t num, 
                        int depth, const uint32_t *bankPointer, int dataLength, const void *data, void *userArg) {
    
    // don't index beyond specified depth
    // Note...maxDepth 0 means index all levels, 1 means just include children level, etc.
    // Note...current depth 1 means you are at children level, 2 means grandchildren level, etc.
    if((maxDepth>0)&&(depth>maxDepth))return(userArg);


    // adds bank index to map
    auto *bi = static_cast<evioBankIndex*>(userArg);

    bankIndex b;
    b.containerType=containerType;
    b.contentType=contentType;
    b.depth=depth;
    b.bankPointer=bankPointer;
    b.bankLength=bankLength;
    b.data=data;
    b.dataLength=dataLength;
    bi->tagNumMap.insert(bankIndexMap::value_type(evioDictEntry(tag,num),b));

    return(userArg);
  }

private:
   int maxDepth;

};


//-----------------------------------------------------------------------
//-----------------------------------------------------------------------


/**
 * Constructor.
 */
evioBankIndex::evioBankIndex(int maxDepth) : maxDepth(maxDepth) {
}


//-----------------------------------------------------------------------


/**
 * Constructor from buffer.
 *
 * @param buffer Event buffer to index
 */
evioBankIndex::evioBankIndex(const uint32_t *buffer, int maxDepth) : maxDepth(maxDepth) {
  parseBuffer(buffer,maxDepth);
}


//-----------------------------------------------------------------------


/**
 * Indexes buffer and fills map.
 * @param buffer Buffer containing serialized event
 * @return true if indexing succeeded
 */
bool evioBankIndex::parseBuffer(const uint32_t *buffer, int maxDepth) {

  // create parser and handler
  evioStreamParser p;
  myHandler h(maxDepth);
  p.parse(buffer,h,((void*)this));

  return(true);
}


//-----------------------------------------------------------------------


/**
 * True if evioDictEntry is in map at least once.
 * @param tn evioDictEntry
 * @return true if evioDictEntry is in map
 */
bool evioBankIndex::tagNumExists(const evioDictEntry & tn) const {
  auto iter = tagNumMap.find(tn);
  return(iter!=tagNumMap.end());
}


//-----------------------------------------------------------------------


/**
 * Returns count of evioDictEntry in map.
 * @param tn evioDictEntry
 * @return Count of evioDictEntry in map.
 */
int evioBankIndex::tagNumCount(const evioDictEntry & tn) const {
  return((int)tagNumMap.count(tn));
}


//-----------------------------------------------------------------------


/**
 * Returns pair of iterators defining range of equal keys in tagNumMap.
 * @param tn evioDictEntry
 * @return Pair of iterators defining range
 */
bankIndexRange evioBankIndex::getRange(const evioDictEntry & tn) const {
  return(tagNumMap.equal_range(tn));
}


//-----------------------------------------------------------------------


/**
 * Returns bankIndex given evioDictEntry, throws exception if no entry found
 * @param tn evioDictEntry
 * @return bankIndex for evioDictEntry
 */
bankIndex evioBankIndex::getBankIndex(const evioDictEntry &tn) const  {

  auto iter = tagNumMap.find(tn);
  if(iter!=tagNumMap.end()) {
    return((*iter).second);
  } else {
    throw(evioException(0,"?evioBankIndex::getBankIndex...evioDictEntry not found",__FILE__,__FUNCTION__,__LINE__));
  }
}


//-----------------------------------------------------------------------
//-----------------------------------------------------------------------
