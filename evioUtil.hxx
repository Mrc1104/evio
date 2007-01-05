// evioUtil.hxx

//  Author:  Elliott Wolin, JLab, 3-jan-2007

// must do:
//   solaris
//   check bufsize in toEVIOBuffer(), parseBank, etc.
//   update doc

// should do:
//   Doxygen comments

//  would like to do:
//   shared pointer
//   cMsg channel
//   ET channel
//   exception stack trace

// not sure:
//   mark in tree, container node?
//   evioDOMNode == defined on tag?
//   scheme for exception type codes?
//   namespaces?
//   AIDA interface?


#ifndef _evioUtil_hxx
#define _evioUtil_hxx


using namespace std;
#include <evio.h>
#include <list>
#include <vector>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <functional>


// class prototypes
class evioDOMTree;
class evioDOMNode;
class evioDOMContainerNode;
template<typename T> class evioDOMLeafNode;
class evioSerializable;




//-----------------------------------------------------------------------------
//----------------------------- Typedefs --------------------------------------
//-----------------------------------------------------------------------------


typedef evioDOMTree* evioDOMTreeP;
typedef evioDOMNode* evioDOMNodeP;
typedef list<evioDOMNodeP>  evioDOMNodeList;
typedef auto_ptr<evioDOMNodeList> evioDOMNodeListP;
typedef enum {
  BANK       = 0xe,
  SEGMENT    = 0xd,
  TAGSEGMENT = 0xc
} ContainerType;


//-----------------------------------------------------------------------------
//------------------------ Misc global functions ------------------------------
//-----------------------------------------------------------------------------


string evioGetIndent(int depth);


//-----------------------------------------------------------------------------
//--------------------------- evio Classes ------------------------------------
//-----------------------------------------------------------------------------


// basic evio exception class
class evioException {

public:
  evioException(int typ = 0, const string &txt = "", const string &aux = "");
  evioException(int typ, const string &txt, const string &file, int line);
  virtual string toString(void) const;


public:
  int type;
  string text;
  string auxText;

};


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------


// evio channel interface
class evioChannel {

public:
  virtual void open(void) throw(evioException) = 0;
  virtual bool read(void) throw(evioException) = 0;
  virtual void write(void) throw(evioException) = 0;
  virtual void write(const unsigned long *myBuf) throw(evioException) = 0;
  virtual void write(const unsigned int* myBuf) throw(evioException) = 0;
  virtual void write(const evioChannel &channel) throw(evioException) = 0;
  virtual void write(const evioChannel *channel) throw(evioException) = 0;
  virtual void write(const evioDOMTree &tree) throw(evioException) = 0;
  virtual void write(const evioDOMTree *tree) throw(evioException) = 0;
  virtual void close(void) throw(evioException) = 0;

  // not sure if these should be part of the interface
  virtual const unsigned int *getBuffer(void) const throw(evioException) = 0;
  virtual int getBufSize(void) const = 0;

};


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------


//  implements evioChannel for a file
class evioFileChannel : public evioChannel {

public:
  evioFileChannel(const string &fileName, const string &mode = "r", int size = 8192) throw(evioException);
  virtual ~evioFileChannel();

  void open(void) throw(evioException);
  bool read(void) throw(evioException);
  void write(void) throw(evioException);
  void write(const unsigned long *myBuf) throw(evioException);
  void write(const unsigned int *myBuf) throw(evioException);
  void write(const evioChannel &channel) throw(evioException);
  void write(const evioChannel *channel) throw(evioException);
  void write(const evioDOMTree &tree) throw(evioException);
  void write(const evioDOMTree *tree) throw(evioException);
  void close(void) throw(evioException);

  const unsigned int *getBuffer(void) const throw(evioException);
  int getBufSize(void) const;

  void ioctl(const string &request, void *argp) throw(evioException);
  string getFileName(void) const;
  string getMode(void) const;


private:
  string filename;
  string mode;
  int handle;
  unsigned int *buf;
  int bufSize;
};


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------


// interface defines node and leaf handlers for stream parsing of evio buffers
class evioStreamParserHandler {

public:
  virtual void *containerNodeHandler(int length, int tag, int contentType, int num, 
                            int depth, void *userArg) = 0;
  virtual void leafNodeHandler(int length, int tag, int contentType, int num, 
                           int depth, const void *data, void *userArg) = 0;
};


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------


// stream parser dispatches to handlers when node or leaf reached
class evioStreamParser {

public:
  void *parse(const unsigned int *buf, evioStreamParserHandler &handler, void *userArg)
    throw(evioException);
  
private:
  void *parseBank(const unsigned int *buf, int bankType, int depth, 
                  evioStreamParserHandler &handler, void *userArg) throw(evioException);

};


//--------------------------------------------------------------
//--------------------------------------------------------------


// virtual class represents an evio node in memory, concrete classes are evioDOMContainerNode and evioDOMLeafNode<T>
// user can only create nodes via factory method createEvioDOMNode()
class evioDOMNode {

  friend class evioDOMContainerNode;   // allows container node to delete child nodes in destructor


protected:
  evioDOMNode(evioDOMNodeP parent, int tag, int num, int contentType) throw(evioException);
  virtual ~evioDOMNode(void);


private:
  evioDOMNode(const evioDOMNode &node) throw(evioException);
  bool operator=(const evioDOMNode &node) const {};


public:
  // public factory methods for node creation
  static evioDOMNodeP createEvioDOMNode(int tag, int num, ContainerType cType=BANK) throw(evioException);
  template <typename T> static evioDOMNodeP createEvioDOMNode(int tag, int num, const vector<T> tVec) throw(evioException);
  template <typename T> static evioDOMNodeP createEvioDOMNode(int tag, int num, const T* t, int len) throw(evioException);
  static evioDOMNodeP createEvioDOMNode(int tag, int num, const evioSerializable &o, ContainerType cType=BANK) throw(evioException);
  static evioDOMNodeP createEvioDOMNode(int tag, int num, void (*f)(evioDOMNodeP c, void *userArg), void *userArg, ContainerType cType=BANK) 
    throw(evioException);
  template <typename T> static evioDOMNodeP createEvioDOMNode(int tag, int num, T *t, void* T::*mfp(evioDOMNode *c, void *userArg),
                                                              void *userArg, ContainerType cType=BANK) throw(evioException);


protected:
  // node creation factory methods used internally
  static evioDOMNodeP createEvioDOMNode(evioDOMNodeP parent, int tag, int num, ContainerType cType=BANK) throw(evioException);
  template <typename T> static evioDOMNodeP createEvioDOMNode(evioDOMNodeP parent, int tag, int num, const vector<T> tVec)
    throw(evioException);
  template <typename T> static evioDOMNodeP createEvioDOMNode(evioDOMNodeP parent, int tag, int num, const T* t, int len)
    throw(evioException);
  static evioDOMNodeP createEvioDOMNode(evioDOMNodeP parent, int tag, int num, const evioSerializable &o, ContainerType cType=BANK)
    throw(evioException);
  static evioDOMNodeP createEvioDOMNode(evioDOMNodeP parent, int tag, int num, void (*f)(evioDOMNodeP c, void *userArg),
                                        void *userArg, ContainerType cType=BANK) throw(evioException);
  template <typename T> static evioDOMNodeP createEvioDOMNode(evioDOMNodeP parent, int tag, int num, T *t,
                                                              void* T::*mfp(evioDOMNodeP c, void *userArg),
                                                              void *userArg, ContainerType cType=BANK) throw(evioException);


public:
  virtual void addNode(evioDOMNodeP node) throw(evioException);
  template <typename T> void append(const vector<T> &tVec) throw(evioException);
  template <typename T> void append(const T* tBuf, int len) throw(evioException);
  template <typename T> void replace(const vector<T> &tVec) throw(evioException);
  template <typename T> void replace(const T* tBuf, int len) throw(evioException);


public:
  virtual evioDOMNodeP cut(void) throw(evioException);
  virtual void drop(void) throw(evioException);
  virtual evioDOMNodeP move(evioDOMNodeP newParent) throw(evioException);
  virtual evioDOMNodeP move(evioDOMTree *newTree) throw(evioException);


public:
  virtual bool operator==(int tag) const;
  virtual bool operator!=(int tag) const;
  virtual bool operator==(const evioDOMNodeP node) const;
  virtual bool operator!=(const evioDOMNodeP node) const;


public:
  evioDOMNode& operator<<(evioDOMNodeP node) throw(evioException);
  evioDOMNode& operator<<(evioDOMTree &tree) throw(evioException);
  evioDOMNode& operator<<(evioDOMTree *tree) throw(evioException);
  template <typename T> evioDOMNode& operator<<(const vector<T> &tVec) throw(evioException);
  template <typename T> evioDOMNode& operator<<(T tVal) throw(evioException);


public:
  evioDOMNodeList *getChildList(void) throw(evioException);
  template <typename T> vector<T> *getVector(void) throw(evioException);


public:
  virtual string toString(void) const = 0;
  virtual string getHeader(int depth) const = 0;
  virtual string getFooter(int depth) const = 0;
  bool isContainer(void) const;
  bool isLeaf(void) const;


public:
  evioDOMNodeP parent;
  evioDOMTreeP parentTree;
  int tag;
  int num;
  int contentType;
};


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------


//  represents an evio container node in memory
class evioDOMContainerNode : public evioDOMNode {

  friend class evioDOMNode;  // allows evioDOMNode to use private subclass methods


private:
  evioDOMContainerNode(evioDOMNodeP parent, int tag, int num, ContainerType cType) throw(evioException);
  evioDOMContainerNode(const evioDOMContainerNode &cNode) throw(evioException);
  ~evioDOMContainerNode(void);
  bool operator=(const evioDOMContainerNode &node) const {};


public:
  void addNode(evioDOMNodeP node) throw(evioException);


public:
  template <typename T> evioDOMNode& operator<<(const vector<T> &tVec) throw(evioException);


public:
  string toString(void) const;
  string getHeader(int depth) const;
  string getFooter(int depth) const;


public:
  evioDOMNodeList childList;

};


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------


//  represents an evio leaf node in memory
template <typename T> class evioDOMLeafNode : public evioDOMNode {

  friend class evioDOMNode;           // allows evioDOMNode to use private subclass methods


private:
  evioDOMLeafNode(evioDOMNodeP par, int tg, int num, const vector<T> &v) throw(evioException);
  evioDOMLeafNode(evioDOMNodeP par, int tg, int num, const T *p, int ndata) throw(evioException);
  evioDOMLeafNode(const evioDOMLeafNode<T> &lNode) throw(evioException);
  ~evioDOMLeafNode(void);
  bool operator=(const evioDOMLeafNode<T> &lNode) const {};


public:
  evioDOMNode& operator<<(const vector<T> &tVec) throw(evioException);


public:
  string toString(void) const;
  string getHeader(int depth) const;
  string getFooter(int depth) const;


public:
  vector<T> data;
};


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------


// represents an evio tree/event in memory
class evioDOMTree : public evioStreamParserHandler {

public:
  evioDOMTree(const evioChannel &channel, const string &name = "evio") throw(evioException);
  evioDOMTree(const evioChannel *channel, const string &name = "evio") throw(evioException);
  evioDOMTree(const unsigned long *buf, const string &name = "evio") throw(evioException);
  evioDOMTree(const unsigned int *buf, const string &name = "evio") throw(evioException);
  evioDOMTree(evioDOMNodeP node, const string &name = "evio") throw(evioException);
  evioDOMTree(int tag, int num, ContainerType cType=BANK, const string &name = "evio")
    throw(evioException);
  virtual ~evioDOMTree(void);


private:
  evioDOMTree(const evioDOMTree &tree) throw(evioException);
  bool operator=(const evioDOMTree &tree) const {};


public:
  void clear(void) throw(evioException);
  void addBank(evioDOMNodeP node) throw(evioException);
  template <typename T> void addBank(int tag, int num, const vector<T> dataVec) throw(evioException);
  template <typename T> void addBank(int tag, int num, const T* dataBuf, int dataLen) throw(evioException);


public:
  evioDOMTree& operator<<(evioDOMNodeP node) throw(evioException);


public:
  void toEVIOBuffer(unsigned long *buf, int size) const throw(evioException);
  void toEVIOBuffer(unsigned int *buf, int size) const throw(evioException);


public:
  evioDOMNodeListP getNodeList(void) throw(evioException);
  template <class Predicate> evioDOMNodeListP getNodeList(Predicate pred) throw(evioException);


public:
  string toString(void) const;


private:
  evioDOMNodeP parse(const unsigned int *buf) throw(evioException);
  int toEVIOBuffer(unsigned int *buf, const evioDOMNodeP pNode, int size) const throw(evioException);
  template <class Predicate> evioDOMNodeList *addToNodeList(evioDOMNodeP pNode, evioDOMNodeList *pList, Predicate pred)
    throw(evioException);
  
  void toOstream(ostream &os, const evioDOMNodeP node, int depth) const throw(evioException);
  void *containerNodeHandler(int length, int tag, int contentType, int num, int depth, void *userArg);
  void leafNodeHandler(int length, int tag, int contentType, int num, int depth, const void *data, void *userArg);


public:
  evioDOMNodeP root;
  string name;

};


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------


// interface for object serialization
class evioSerializable {

public:
  virtual void serialize(evioDOMNodeP node) const throw(evioException) = 0;
};


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------


// include templates
#include <evioUtilTemplates.hxx>

#endif
