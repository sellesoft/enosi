/*
 *  URI parsing and generating api
 *
 *  TODO(sushi) move into iro
 */

#ifndef _lpp_URI_h
#define _lpp_URI_h

#include "iro/Common.h"
#include "iro/io/IO.h"

using namespace iro;

/* ================================================================================================ URI
 *  A URI and its parts. This always copies whatever is given to it and owns the memory it views.
 */
struct URI
{
    io::Memory scheme;
    io::Memory authority;
    io::Memory body;

  b8   init();
  void deinit();

  void reset();

  // parses the given string into a URI 
  //
  static b8 parse(URI* uri, String s);

  b8 encode();
  b8 decode();
};

#endif // _lpp_uri_h
