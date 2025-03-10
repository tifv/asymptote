/* C++ interface to the XDR External Data Representation I/O routines
   Version 1.46
   Copyright (C) 1999-2007 John C. Bowman

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU Lesser General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA. */

#ifndef __xstream_h__
#define __xstream_h__ 1

#ifndef _ALL_SOURCE
#define _ALL_SOURCE 1
#endif

#include <cstdio>
#include <vector>
#include <algorithm>

#include <sys/types.h>
#include <rpc/types.h>

#define quad_t long long
#define u_quad_t unsigned long long

#if defined(__CYGWIN__) || defined(__FreeBSD__)
#include <sys/select.h>
#define u_char unsigned char
#define u_int unsigned int
#define u_short unsigned short
#define u_long unsigned long
extern "C" int fseeko(FILE *, off_t, int);
extern "C" off_t ftello(FILE *);
extern "C" FILE *open_memstream(char **, size_t *);
#endif

#ifdef __APPLE__
#include <rpc/xdr.h>

inline bool_t xdr_long(XDR *__xdrs, long *__lp) {
  return xdr_longlong_t(__xdrs,(long long *) __lp);
}

inline bool_t xdr_u_long(XDR *__xdrs, unsigned long *__lp) {
  return xdr_u_longlong_t(__xdrs,(unsigned long long *) __lp);
}

#endif

#ifdef _POSIX_SOURCE
#undef _POSIX_SOURCE
#include <rpc/rpc.h>
#define _POSIX_SOURCE
#else
#include <rpc/rpc.h>
#endif

namespace xdr {

class xbyte {
  unsigned char c;
public:
  xbyte() {}
  xbyte(unsigned char c0) : c(c0) {}
  xbyte(int c0) : c((unsigned char) c0) {}
  xbyte(unsigned int c0) : c((unsigned char) c0) {}
  int byte() const {return c;}
  operator unsigned char () const {return c;}
};

class xios {
public:
  enum io_state {goodbit=0, eofbit=1, failbit=2, badbit=4};
  enum open_mode {in=1, out=2, app=8, trunc=16};
  enum seekdir {beg=SEEK_SET, cur=SEEK_CUR, end=SEEK_END};
private:
  int _state;
public:
  int good() const { return _state == 0; }
  int eof() const { return _state & eofbit; }
  int fail() const { return !good();}
  int bad() const { return _state & badbit; }
  void clear(int state = 0) {_state=state;}
  void set(int flag) {_state |= flag;}
  operator void*() const { return fail() ? (void*)0 : (void*)(-1); }
  int operator!() const { return fail(); }
};

class xstream : public xios {
protected:
  FILE *buf;
public:
  virtual ~xstream() {}
  xstream() : xios(), buf(nullptr) {}

  void precision(int) {}

  xstream& seek(off_t pos, seekdir dir=beg) {
    if(buf) {
      clear();
      if(fseeko(buf,pos,dir) != 0) set(failbit);
    }
    return *this;
  }
  off_t tell() {
    return ftello(buf);
  }
};

#define IXSTREAM(T,N) ixstream& operator >> (T& x)      \
  {if(!xdr_##N(&xdri, &x)) set(eofbit); return *this;}

#define OXSTREAM(T,N) oxstream& operator << (T x)       \
  {if(!xdr_##N(&xdro, &x)) set(badbit); return *this;}

class ixstream : virtual public xstream {
private:
  bool singleprecision;
protected:
  XDR xdri;
public:
  ixstream(bool singleprecision=false): singleprecision(singleprecision) {}

  virtual void open(const char *filename, open_mode=in) {
    clear();
    buf=fopen(filename,"r");
    if(buf) xdrstdio_create(&xdri,buf,XDR_DECODE);
    else set(badbit);
  }

  virtual void close() {
    closeFile();
  }

  void closeFile() {
    if(buf) {
#ifndef _CRAY
      xdr_destroy(&xdri);
#endif
      fclose(buf);
      buf=nullptr;
    }
  }
  ixstream(const char *filename) {open(filename);}
  ixstream(const char *filename, open_mode mode) {open(filename,mode);}
  virtual ~ixstream() {close();}

  typedef ixstream& (*imanip)(ixstream&);
  ixstream& operator >> (imanip func) { return (*func)(*this); }

  IXSTREAM(int,int);
  IXSTREAM(unsigned int,u_int);
  IXSTREAM(long,long);
  IXSTREAM(unsigned long,u_long);
  IXSTREAM(long long,longlong_t);
  IXSTREAM(unsigned long long,u_longlong_t);
  IXSTREAM(short,short);
  IXSTREAM(unsigned short,u_short);
  IXSTREAM(char,char);
#ifndef _CRAY
  IXSTREAM(unsigned char,u_char);
#endif
  IXSTREAM(float,float);

  ixstream& operator >> (double& x)
  {
    if(singleprecision) {
      float f;
      *this >> f;
      x=(double) f;
    } else
      if(!xdr_double(&xdri, &x)) set(eofbit);
    return *this;
  }

  ixstream& operator >> (xbyte& x) {
    int c=fgetc(buf);
    if(c != EOF) x=c;
    else set(eofbit);
    return *this;
  }
};

class oxstream : virtual public xstream {
private:
  bool singleprecision;
protected:
  XDR xdro;
public:
  oxstream(bool singleprecision=false) : singleprecision(singleprecision) {}

  virtual void open(const char *filename, open_mode mode=trunc) {
    clear();
    buf=fopen(filename,(mode & app) ? "a" : "w");
    if(buf) xdrstdio_create(&xdro,buf,XDR_ENCODE);
    else set(badbit);
  }

  virtual void close()
  {
    closefile();
  }

  void closefile() {
    if(buf) {
#ifndef _CRAY
      xdr_destroy(&xdro);
#endif
      fclose(buf);
      buf=NULL;
    }
  }

  oxstream(const char *filename, bool singleprecision=false): singleprecision(singleprecision) {open(filename);}
  oxstream(const char *filename, open_mode mode, bool singleprecision=false): singleprecision(singleprecision)
  {
    open(filename,mode);
  }
  virtual ~oxstream() {closefile();}

  oxstream& flush() {if(buf) fflush(buf); return *this;}

  typedef oxstream& (*omanip)(oxstream&);
  oxstream& operator << (omanip func) { return (*func)(*this); }

  OXSTREAM(int,int);
  OXSTREAM(unsigned int,u_int);
  OXSTREAM(long,long);
  OXSTREAM(unsigned long,u_long);
  OXSTREAM(long long,longlong_t);
  OXSTREAM(unsigned long long,u_longlong_t);
  OXSTREAM(short,short);
  OXSTREAM(unsigned short,u_short);
  OXSTREAM(char,char);
#ifndef _CRAY
  OXSTREAM(unsigned char,u_char);
#endif
  OXSTREAM(float,float);

  oxstream& operator << (double x) {
    if(singleprecision)
      *this << (float) x;
    else
      if(!xdr_double(&xdro, &x)) set(badbit);
    return *this;
  }

  oxstream& operator << (xbyte x) {
    if(fputc(x.byte(),buf) == EOF) set(badbit);
    return *this;
  }
};

class memoxstream : public oxstream
{
private:
  char* baseBuf;
  size_t len;

public:
  memoxstream(bool singleprecision=false) :
    oxstream(singleprecision), baseBuf(nullptr), len(0)
  {
    clear();
    buf=open_memstream(&baseBuf,&len);
    if(buf)
      xdrstdio_create(&xdro,buf,XDR_ENCODE);
    else
      set(badbit);
  }

  ~memoxstream() override
  {
    closefile();
    free(baseBuf);
  }

  [[nodiscard]]
  char const* stream() const
  {
    return baseBuf;
  }

  [[nodiscard]]
  size_t const& getLength() const
  {
    return len;
  }
};

class memixstream: public ixstream
{
public:
  memixstream(char* data, size_t length, bool singleprecision=false) : ixstream(singleprecision)
  {
    xdrmem_create(&xdri,data,length,XDR_DECODE);
  }

  explicit memixstream(std::vector<char>& data, bool singleprecision=false) :
    memixstream(data.data(), data.size(), singleprecision)
  {
  }

  ~memixstream() override
  {
    xdr_destroy(&xdri);
  }

  void close() override
  {
    xdr_destroy(&xdri);
  }

  void open(const char *filename, open_mode = in) override
  {
  }
};

class ioxstream : public ixstream, public oxstream {
public:
  void open(const char *filename, open_mode mode=out) override {
    clear();
    if(mode & app)
      buf=fopen(filename,"a+");
    else if(mode & trunc)
      buf=fopen(filename,"w+");
    else if(mode & out) {
      buf=fopen(filename,"r+");
      if(!buf)
        buf=fopen(filename,"w+");
    } else
      buf=fopen(filename,"r");
    if(buf) {
      xdrstdio_create(&xdri,buf,XDR_DECODE);
      xdrstdio_create(&xdro,buf,XDR_ENCODE);
    } else set(badbit);
  }

  void close() override {
    if(buf) {
#ifndef _CRAY
      xdr_destroy(&xdri);
      xdr_destroy(&xdro);
#endif
      fclose(buf);
      buf=NULL;
    }
  }

  ioxstream() {}
  ioxstream(const char *filename) {open(filename);}
  ioxstream(const char *filename, open_mode mode) {open(filename,mode);}
  virtual ~ioxstream() {close();}
};

inline oxstream& endl(oxstream& s) {s.flush(); return s;}
inline oxstream& flush(oxstream& s) {s.flush(); return s;}

#undef IXSTREAM
#undef OXSTREAM

}

#undef quad_t

#endif
