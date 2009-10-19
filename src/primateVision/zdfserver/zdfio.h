/**
 * \ingroup icub_primatevision_zdfserver
 */

/*
 * Copyright (C) 2003-2008 Andrew Dankers. All rights reserved.
 * 
 */

#ifndef ZDFIO_H
#define ZDFIO_H

//YARP2 INCLUDES
#include <yarp/os/all.h>
#include <yarp/sig/all.h>
#include <yarp/dev/all.h>

using namespace yarp::sig;
using namespace yarp::os;
using namespace yarp::dev;
using namespace std;


namespace iCub {
  namespace contrib {
    namespace primateVision {


      //STATIC SERVER CONFIG OUTPUT:
#include <yarp/os/begin_pack_for_net.h>
      /** A container class for handling ZDFServer parameters
       *  sent over port in response to paramProbe.
       */
      class ZDFServerParams {
      public:
	/** Constructor. */
	ZDFServerParams() {
	  listTag = BOTTLE_TAG_LIST + BOTTLE_TAG_INT;
	  lenTag = 3;
	}
	/** Converstion to string of parameters for printing. */
	string toString(){
	  char buffer[50];
	  sprintf(buffer, "%d %d %d",width,height,psb);
	  return buffer;
	}
	int listTag;
	int lenTag;
	
	//this Server's Response Params:
	int width; /**< Server image output width. */
	int height; /**< Server image output height. */
	int psb; /**< Server step in bytes through output image. */
	//
	
      } PACKED_FOR_NET;
#include <yarp/os/end_pack_for_net.h>



      //ONLINE PROC OUTPUT:
      /** A container class for handling online calculated 
       *  zdf output sent over ports.
       */
      class ZDFServerData : public Portable{
      public:
	/** Constructor. */
	ZDFServerData() {
	}
	void resize(int w,int h){
	  dog.resize(w,h);
	  tex.resize(w,h);
	}
	/** Converstion to string of parameters for printing. */
	string toString(){
	  char buffer[50];
	  sprintf(buffer, "%f %f %f %d %d %d",
		  x,y,z,mos_x,mos_y,(int)update);
	  return buffer;
	}
	
	bool write(ConnectionWriter& con){
	  con.appendInt(BOTTLE_TAG_LIST);
	  con.appendInt(8);
	  dog.write(con);
	  tex.write(con);
	  con.appendDouble(x);
	  con.appendDouble(y);
	  con.appendDouble(z);
	  con.appendInt(mos_x);
	  con.appendInt(mos_y);
	  con.appendInt((int) update);
	  return true;
	}
	bool read(ConnectionReader& con){
	  if (!con.isValid()) {
	    return false;
	  }

	  con.convertTextMode();
	  int header=con.expectInt();
	  int len = con.expectInt();
	  if (header != BOTTLE_TAG_LIST || len!=8){
	    return false;
	  }
	  dog.read(con);
	  tex.read(con);
	  x = con.expectDouble();
	  y = con.expectDouble();
	  z = con.expectDouble();
	  mos_x = con.expectInt();
	  mos_y = con.expectInt();
	  update = (bool) con.expectInt();
	  return true;
	}

	//ZDFServer Results:
	double x;
	double y;
	double z;
	int mos_x;
	int mos_y;
	bool update;
	ImageOf<PixelMono> dog;
	ImageOf<PixelMono> tex;
      };


      //ONLINE TUNING OUTPUT:
      /** A container class for handling images 
       *  sent to zdfclient for tuning.
       */
      class ZDFServerTuneData : public Portable {
      public:
	/** Constructor. */
	ZDFServerTuneData()  {

	}
	void resize(int w,int h){
	  prob.resize(w,h);
	  tex.resize(w,h);
	  left.resize(w,h);
	  right.resize(w,h);
	}
	bool write(ConnectionWriter& con){
	  con.appendInt(BOTTLE_TAG_LIST);
	  con.appendInt(4);
	  prob.write(con);
	  tex.write(con);
	  left.write(con);
	  right.write(con);
	  return true;
	}
	bool read(ConnectionReader& con){
	  if (!con.isValid()) {
	    return false;
	  }

	  con.convertTextMode();
	  int header = con.expectInt();
	  int len = con.expectInt();
	  if (header != BOTTLE_TAG_LIST || len!=4){
	    return false;
	  }
	  prob.read(con);
	  tex.read(con);
	  left.read(con);
	  right.read(con);
	  return true;
	}
	
	//ZDFServer tuning stuff:
	ImageOf<PixelMono> prob;
	ImageOf<PixelMono> tex;
	ImageOf<PixelMono> left;
	ImageOf<PixelMono> right;
      };

    }
  }
}
#endif
