// evioException.hxx

//  Author:  Elliott Wolin, JLab, 18-Feb-2010


#ifndef _evioException_hxx
#define _evioException_hxx


#include <cstdlib>
#include <cstring>
#include <exception>
#include <string>


#ifndef sun
 #ifdef linux
   #include <execinfo.h>
   #include <cxxabi.h>
 #endif
 #include <sstream>
#else
 #include <sstream>
#endif



namespace evio {


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------


/**
 * Basic evio exception class.  Includes integer type and two text fields.
 */
class evioException : public std::exception {

public:
  evioException(int typ = 0, const std::string &txt = "", const std::string &aux = "");
  evioException(int typ, const std::string &txt, const std::string &file, const std::string &func, int line);
  virtual ~evioException() noexcept {};
  virtual std::string toString() const noexcept;
  virtual const char *what() const noexcept;


public:
  int type;             /**<Exception type.*/
  std::string text;     /**<Primary text.*/
  std::string auxText;  /**<Auxiliary text.*/
  std::string trace;    /**<Stack trace, not available on all platforms.*/
};


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------


}  // namespace evio



#endif


