//  evioBankIndex.hxx
//
// creates bank index for serialized event, indexes all banks including container banks
// eventually need to switch to std::tuple instead of custom struct
//
//
//  Author:  Elliott Wolin, JLab, 15-aug-2012



#ifndef _evioBankIndex_hxx
#define _evioBankIndex_hxx


#include "evioException.hxx"
#include "evioUtil.hxx"


namespace evio {

using namespace std;
using namespace evio;


// holds bank index info
typedef struct {
  int containerType;            // bank container type
  int contentType;              // bank content type
  int depth;                    // depth in hierarchy
  const uint32_t *bankPointer;  // pointer to first word of bank
  int bankLength;               // length of bank in 32-bit words
  const void *data;             // pointer to first word of data of bank
  int dataLength;               // length of data item array units of item
} bankIndex;
  

// compares tagNums to each other, first by tag, then by num
struct tagNumComp {
  bool operator() (const evioDictEntry &lhs, const evioDictEntry &rhs) const {
    if(lhs.getTag()<rhs.getTag()) {
      return(true);
    } else if(lhs.getTag()>rhs.getTag()) {
      return(false);
    } else {
      return(lhs.getNum()<rhs.getNum());
    }    
  }
};


// bank index map and range
typedef multimap<evioDictEntry,bankIndex,tagNumComp> bankIndexMap;
typedef pair< bankIndexMap::const_iterator, bankIndexMap::const_iterator > bankIndexRange;




//-----------------------------------------------------------------------
//-----------------------------------------------------------------------


/**
 * Creates bank index for serialized event.
 *
 * Note that a given tag/num may appear more than once in event and map.
 */
class evioBankIndex {

public:
  evioBankIndex(int maxDepth=0);
  evioBankIndex(const uint32_t *buffer, int maxDepth=0);
  virtual ~evioBankIndex() = default;


public:
  bool parseBuffer(const uint32_t *buffer, int maxDepth);
  bool tagNumExists(const evioDictEntry & tn) const;
  int tagNumCount(const evioDictEntry & tn) const;
  bankIndexRange getRange(const evioDictEntry & tn) const;
  bankIndex getBankIndex(const evioDictEntry &tn) const ;
  int getMaxDepth();


public:
  bankIndexMap tagNumMap;     /**<Holds index to one or more banks having tag/num.*/

private:
  int maxDepth;


public:
  /**
   * Returns length and pointer to data, NULL if container bank, bad evioDictEntry or wrong data type.
   *
   * @param tn evioDictEntry
   * @param pLen Pointer to int to receive data length, set to 0 upon error
   * @return Pointer to data, NULL on error
   */
  template <typename T> const T* getData(const evioDictEntry &tn, int *pLen) {

    bankIndexMap::const_iterator iter = tagNumMap.find(tn);

    if((iter!=tagNumMap.end()) && ((((*iter).second).contentType)==evioUtil<T>::evioContentType())) {
      *pLen=((*iter).second).dataLength;
      return(static_cast<const T*>(((*iter).second).data));
    } else {
      *pLen=0;
      return(nullptr);
    }
  }


  /**
   * Returns length and pointer to data, assumes valid bankIndex
   *
   * @param bi bankIndex
   * @param pLen Pointer to int to receive data length, set to 0 for bad type
   * @return Pointer to data, NULL on bad type
   */
  template <typename T> const T* getData(const bankIndex &bi, int *pLen)  {

    if(bi.contentType==evioUtil<T>::evioContentType()) {
      *pLen=(bi.dataLength);
      return(static_cast<const T*>(bi.data));
    } else {
      *pLen=0;
      return(nullptr);
    }
  }

};

//-----------------------------------------------------------------------
//-----------------------------------------------------------------------

} // namespace evio


#endif
  
