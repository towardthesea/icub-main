// -*- mode:C++; tab-width:4; c-basic-offset:4; indent-tabs-mode:nil -*-      
#include "RunnableControlLaw.h"
#include <yarp/os/Network.h>
#include <yarp/math/Math.h>
#include <yarp/sig/Matrix.h>
#include <yarp/sig/Vector.h>

using namespace std;
using namespace yarp::os;
using namespace yarp::sig;
using namespace yarp::math;

CB::RunnableControlLaw::RunnableControlLaw() {
  numControllers = 0;
  controllers.clear();
  initiallized = false;  
  connectedToDevices = false;
  numOutputs = 1;
  outputName.push_back("u");
}


CB::RunnableControlLaw::~RunnableControlLaw() {
  controllers.clear();
}

void CB::RunnableControlLaw::init() {

  string tmpName;
  string tmpSpace;
  int tmpSize;
  bool foundResource;
  bool useTranspose;

  cout << "RunnableControlLaw::init()" << endl;

  controllerDeviceNames.clear();
  controllerOutputSpaces.clear();
  Vout.clear();
  VoutConfig.clear();
  VoutDeviceNames.clear();
  VoutSpaces.clear();
  deviceMap.clear();
  jacobianMap.clear();
  jacobianInverseStore.clear();

  for(int i=0; i<controllers.size(); i++) {

      // store the device names, the spaces, and the sizes of each controller
      tmpName = controllers[i]->getOutputDeviceName();
      tmpSpace = controllers[i]->getOutputSpace();
      tmpSize = controllers[i]->getOutputSize();
      controllerDeviceNames.push_back(tmpName);
      controllerOutputSpaces.push_back(tmpSpace);
      useTranspose = controllers[i]->usingJacobianTranspose();

      cout << "adding control output " << i << " for device=" << tmpName.c_str() << ", in space=" << tmpSpace.c_str() << ", with size=" << tmpSize << endl;

      // for each device, create a single Vout vector.
      // because later we will evaluate the composite control signal
      // by goign from lower to higher priority control actions,
      // store the lowest priority size and space for each device.
      foundResource = false;
      for(int k=0; k<VoutDeviceNames.size(); k++) {
          if(tmpName == VoutDeviceNames[k]) {
              foundResource = true;
              break;
          }
      }      
      if(!foundResource) {
          VoutDeviceNames.push_back(tmpName);
          VoutSpaces.push_back(tmpSpace);
          Vout.push_back(Vector(tmpSize)); 
          VoutConfig.push_back(Vector(1)); 
          deviceMap[tmpName] = Vout.size() - 1;
      } else {
          Vout[deviceMap[tmpName]].resize(tmpSize);
          VoutSpaces[deviceMap[tmpName]] = tmpSpace;
      }

  }
  
  cout << "RunnableControlLaw::init() finished" << endl;
  initiallized = true;

}

void CB::RunnableControlLaw::addController(Controller *c) {
  controllers.push_back(c);
  numControllers++;
  cout << "RunnableControlLaw::addController() added new controller " << numControllers << endl;
}

void CB::RunnableControlLaw::addController(string sen, string ref, string pf, string eff, double gain=1) {
  Controller *c = new Controller(sen,ref,pf,eff);
  c->setGain(gain);
  controllers.push_back(c);
  numControllers++;
  cout << "RunnableControlLaw::addController() added new controller " << numControllers << endl;
}

void CB::RunnableControlLaw::addController(string sen, string pf, string eff, double gain=1) {
  Controller *c = new Controller(sen,pf,eff);
  c->setGain(gain);
  controllers.push_back(c);
  numControllers++;
  cout << "RunnableControlLaw::addController() added new controller " << numControllers << endl;
}

bool CB::RunnableControlLaw::updateAction() {
  
  bool ok = true;
  Vector Vc;
  Matrix Jc;
  Matrix JcInv;
  Matrix Nc;
  Matrix I;
  int id;
  string tmpName;
  string tmpSpace;
  int tmpSize;
  bool useTranspose;

  double scalar = 10.0;
  double JcMag = 0;
  double VoutMag = 0;
  double VprojMag = 0;
  double VcMag = 0;

  Matrix Mout;
  Matrix MoutInitial;
  Vector Vproj;
  Matrix JcT;
  Matrix JcTinv;
  Matrix Mproj;
  Matrix Jint;
  Matrix MoutTmp;

  //  cout << "\n\nRunnableControlLaw::update()\n-----------------------" << endl;

  if(!initiallized) {
    init();
  }

  // clear out last times vals
  for(int i=0; i<Vout.size(); i++) {
    Vout[i].zero();
  }

  // compute prioritized combination of controllers. 
  // we do this by going from lowest priority to highest priority,
  // projecting the new subordinate control signal into the nullspace
  // of the task jacobian on the superior controller.  This
  // may require a space transformation of the lower priority action
  // into the space of the higher priority action.
  for(int i=(numControllers-1); i>=0; i--) {

    tmpName = controllers[i]->getOutputDeviceName();
    tmpSpace = controllers[i]->getOutputSpace();
    tmpSize = controllers[i]->getOutputSize();   
    useTranspose = controllers[i]->usingJacobianTranspose();

    Vc = controllers[i]->getControlOutput();
    Jc = controllers[i]->getTaskJacobian();
    id = deviceMap[tmpName];

    Nc.resize(tmpSize,tmpSize);
    I.resize(tmpSize,tmpSize);
    I.eye();
    
    Mout.resize(tmpSize,1);
    Vproj.resize(tmpSize);

    //    cout << "RunnableControlLaw::update() -- Vout id: %d, name=%s, space=%s, size=%d\n", id, tmpName.c_str(), tmpSpace.c_str(), tmpSize);
    
    //    cout << "RunnableControlLaw::update() -- Vc[" << i << "] beginning :\n";
    //for(int k=0; k<tmpSize; k++) cout << Vc[k] << endl;

    //    cout << "RunnableControlLaw::update() -- Jc[" << i << "]=(" << Jc.rows() << "x" << Jc.cols() << "):\n";
    //for(int k=0; k<tmpSize; k++) cout << Jc[0][k] << endl;

    // compute the nullspace of the higher priority task
    
    JcT.resize(Jc.cols(),Jc.rows());
    JcT = Jc.transposed();
    //cout << "RunnableControlLaw::update() -- JcT[%d] (%dx%d):\n", i, JcT.rows(), JcT.cols());
    //for(int k=0; k<tmpSize; k++) cout << "%.5f\n", JcT[k][0]);
    
    JcTinv.resize(JcT.cols(),JcT.rows());
    JcTinv = pinv(JcT,0.0);
    //    cout << "RunnableControlLaw::update() -- JcTinv[" << i << "]=(" << JcTinv.rows() << "x" << JcTinv.cols() << "):\n";
    //for(int k=0; k<tmpSize; k++) cout << JcTinv[0][k] << endl;
    
    JcInv.resize(Jc.cols(),Jc.rows());
    JcInv = JcTinv.transposed();
    //    cout << "RunnableControlLaw::update() -- JcInv[%d] (%dx%d):\n", i, JcInv.rows(), JcInv.cols());
    //for(int k=0; k<tmpSize; k++) cout << "%.5f\n", JcInv[k][0]);

    Nc = (I - JcInv*Jc);  // (NxN) - (Nx1)*(1xN) = (NxN) 
    /*
    cout << "RunnableControlLaw::update() -- Nullspace[%d] (%dx%d):\n", i, Nc.rows(), Nc.cols());
    for(int k=0; k<Nc.rows(); k++) {
        for(int n=0; n<Nc.cols(); n++) {
            cout << "%.5f ", Nc[k][n]);
        }
        cout << "" << endl;
    }
    */

    //    cout << "RunnableControlLaw::update() -- controller space:%s, existing space for device: %s\n",
    //             tmpSpace.c_str(), VoutSpaces[id].c_str());

    if(tmpSpace != VoutSpaces[id]) {
        
        // the (new) higher priority space is not the same as the 
        // previous space for the current device.  We need to create
        // a jacobian that transforms the lower priority (possibly 
        // composite) control signal into this new space...
        
        MoutInitial.resize(Vout[id].size(),1);
        for(int k=0; k<Vout[id].size(); k++) MoutInitial[k][0] = Vout[id][k];
        
        //  cout << "RunnableControlLaw::update() -- MoutInitial[%d]:\n", i);
        //for(int k=0; k<Vout[id].size(); k++) cout << "%.5f\n", MoutInitial[k][0]);

        //get jacobian
        Jint = getJacobian(tmpName,tmpSpace,VoutSpaces[id],useTranspose);
        Mout = Jint*MoutInitial; //(N*M)*(Mx1) = (Nx1)

        // project any existing Vout[id] into nullspace of current Vout[id]
        // V = V_1 - (I-J_1^# J_1)*J_21*V_2

        // update the size and space info for this device
        Vout[id].resize(tmpSize);
        VoutSpaces[id] = tmpSpace;

    } else {
        // project any existing Vout[id] into nullspace of current Vout[id]
        // V = V_1 - (I-J_1^# J_1)*V_2
    }
        
    for(int k=0; k<tmpSize; k++) Mout[k][0] = Vout[id][k];
    
    //    cout << "RunnableControlLaw::update() -- Mout[" << i << "]:\n";
    //for(int k=0; k<tmpSize; k++) cout << Mout[k][0] << endl;
    
    Mproj = Nc*Mout; // (NxN)*(Nx1)=(Nx1)
    for(int k=0; k<tmpSize; k++) Vproj[k] = Mproj[k][0];
    
    // make sure magnitude of lower priority objective does not overweight superior1
    VcMag = 0;
    for(int k=0; k<Vc.size(); k++) VcMag += (Vc[k]*Vc[k]);
    VcMag = sqrt(VcMag);

    VprojMag = 0;
    for(int k=0; k<Vproj.size(); k++) VprojMag += (Vproj[k]*Vproj[k]);
    VprojMag = sqrt(VprojMag);

    //    cout << "VprojMag: %f\nVcMag:    %f\n", VprojMag, VcMag);
    
    if(VprojMag > (2*VcMag)) {
        cout << "Scaling down lower priority objective..." << endl;
        if(VcMag < 1E-5) {
            for(int k=0; k<Vproj.size(); k++) Vproj[k] = 0;
        } else {
            for(int k=0; k<Vproj.size(); k++) Vproj[k] = Vproj[k]*(2.0*VcMag/VprojMag);
        }
    }
    

    /*
    if(i==0) {
        cout << "RunnableControlLaw::update() -- Vproj[%d] :\n", i);
        for(int k=0; k<tmpSize; k++) cout << "%.5f\n", Vproj[k]);
    }
    */

    // determine composite signal in the new output space for this device        
    Vout[id] = Vc + 0.5*Vproj;

    /*
    if(i==0) {    
        cout << "RunnableControlLaw::update() -- Vc[%d]:\n", i);
        for(int k=0; k<tmpSize; k++) cout << "%.7f\n", Vc[k]);    
    //    cout << "RunnableControlLaw::update() -- Vproj[%d]\n", i);
    //for(int k=0; k<tmpSize; k++) cout << "%.7f\n", Vproj[k]);
    
        cout << "RunnableControlLaw::update() -- Vout[%d]:\n", i);
        for(int k=0; k<tmpSize; k++) cout << "%.7f\n", Vout[id][k]);    
    }
    */
  }

  // now go through each device output, and make sure that the space is
  // a configuration (the only _actually_ settable type of device).
  // if it isn't, get the appropriate jacobian, and transform it to it.
  for(int i=0; i<VoutSpaces.size(); i++) {
      //      cout << "RunnableControlLaw::update() for Vout[" << i << "] = " << VoutDeviceNames[i].c_str() << endl;
      
      //tmpName = controllers[i]->getOutputDeviceName();
      //      tmpSpace = controllers[i]->getOutputSpace();
      //tmpSize = controllers[i]->getOutputSize();   
      //id = deviceMap[tmpName];

      if(VoutSpaces[i] != "configuration") {
          //          cout << "RunnableControlLaw::update() for controller " << i << ", not a configuration resource" << endl;
          Jint = getJacobian(VoutDeviceNames[i], "configuration", VoutSpaces[i], useTranspose);
          MoutTmp.resize(Vout[i].size(),1);
          for(int k=0; k<Vout[i].size(); k++) MoutTmp[k][0] = Vout[i][k];
          Mout = Jint*MoutTmp;
          //for(int k=0; k<Vout[i].size(); k++) Vout[i][k] = Mout[k][0];
          //     VoutSpaces[i] = "configuration";

            VoutConfig[i].resize(Mout.rows());
            //            cout << "Vout size=" << Vout[i].size() << ", VoutConfig size=" << VoutConfig[i].size() << endl;
            for(int k=0; k<Mout.rows(); k++) VoutConfig[i][k] = Mout[k][0];
      } else {
          VoutConfig[i].resize(Vout[i].size());
          for(int k=0; k<Vout[i].size(); k++) VoutConfig[i][k] = Vout[i][k];
      }
      //   cout << "VoutConfig[" << i << "]\n";
      //for(int k=0; k<VoutConfig[i].size(); k++) cout << VoutConfig[i][k] << endl;

  }

  if(!connectedToDevices) { 
      cout << "RunnableControlLaw::update() not connected to devices. doing it now..." << endl;
      ok &= connectToDevicePorts();
      if(!ok) {
          cout << "RunnableControlLaw::update() couldnt connect to device ports" << endl;
      }
  }

  if(!sendOutputsToDevices()) {
      cout << "RunnableControlLaw::update() problem sending outputs to device ports" << endl;
  }

  //  cout << "RunnableControlLaw::update() done" << endl;
  return ok;
}

void CB::RunnableControlLaw::startAction() {
  for(int i=0; i<controllers.size(); i++) {
    controllers[i]->startAction();
  }
  Time::delay(0.5);
  running = true;
  start();     // mandatory start function
}

void CB::RunnableControlLaw::stopAction() {
  stop();     // mandatory stop function
  for(int i=0; i<controllers.size(); i++) {
    controllers[i]->stopAction();
  }
  for(int i=0; i<helperJacobians.size(); i++) {
      helperJacobians[i]->stopJacobian();
  }

  // make sure the devices are locked
  for(int i=0; i<devicePorts.size(); i++) {
      Bottle &b = deviceLockPorts[i]->prepare();
      b.clear();
      b.addInt(1);        
      deviceLockPorts[i]->write();
  }

  //controllers.clear();
  //helperJacobians.clear();
}

void CB::RunnableControlLaw::postData() {
  
    /*    
    Bottle &b = outputPort[0]->prepare();
    b.clear();

    printf ("RunnableControlLaw %s posting data\n", actionName.c_str());
  
    // what to do here with multiple Vout's?
    b.addInt(Vout.size());
    
    printf ("out: (");
    for(int i = 0; i < Vout.size(); i++)
        {
            printf ("%.4f ", Vout[i]);
            
            // add position to output port
            b.addDouble(Vout[i]);
            
        }
    printf (")" << endl;
    outputPort[0]->write();      
    */
}

Matrix CB::RunnableControlLaw::getJacobian(string deviceName, string outSpace, string inSpace, bool useTranspose=false) {

    Matrix J;
    bool needsJacobianInverse;
    int id, id2;
    string jStr = deviceName + "/" + outSpace + "/" + inSpace;
    string jStr2 = deviceName + "/" + inSpace + "/" + outSpace;


    cout << "RunnableControlLaw::getJacobian(" << inSpace.c_str() << ":" << outSpace.c_str() << "), mapSize=" << jacobianMap.size() << endl;

    // check to see if the jacobian has already been created
    if(jacobianMap.find(jStr) == jacobianMap.end()) {

        cout << "RunnableControlLaw::getJacobian() -- did not find a forward jacobian already created" << endl;

        if(jacobianMap.find(jStr2) != jacobianMap.end()) {

            cout << "RunnableControlLaw::getJacobian() -- found backward jacobian" << endl;

            // this jacobian has already been created to do the inverse mapping
            id2 = jacobianMap[jStr2];
            cout << "Inverse of the necessary Jacobian found at positon " << id2 << endl;

            // create a position to copy the pointer and store it in the map
            id = helperJacobians.size();
            jacobianMap[jStr] = id;

            // just copy the pointer to the other one, and set the inverse flag
            // as the opposite of whatever it was
            helperJacobians.push_back(helperJacobians[id2]);
            jacobianInverseStore.push_back(!jacobianInverseStore[id2]);
            
        } else {
            
            cout << "RunnableControlLaw::getJacobian() -- did not find a backward jacobian already created, either. must create one" << endl;

            id = helperJacobians.size();
            jacobianMap[jStr] = id;
            
            helperJacobians.push_back(jacMap.getJacobian(inSpace,outSpace,deviceName));
            needsJacobianInverse = jacMap.needsInverse(inSpace,outSpace);
            jacobianInverseStore.push_back(needsJacobianInverse);

            if(helperJacobians[helperJacobians.size()-1]==NULL) {
                cout << "Controller needs an unknown Jacobian..." << endl;
                exit(0);
            }
            
            //            cout << "starting helper Jacobian[%d], (%s:%s), device=%s, inv=%d\n", 
            //               id, inSpace.c_str(), outSpace.c_str(), deviceName.c_str(), (int)needsJacobianInverse);

            //            exit(0);
            helperJacobians[id]->startJacobian();

        }


        Time::delay(0.25);

    }

    //    cout << "RunnableControlLaw::getJacobian() -- found forward jacobian" << endl;
    //    cout << "getting Jacobian" << endl;
    id = jacobianMap[jStr];
    if(jacobianInverseStore[id]) {
        if(!useTranspose) {
            J = helperJacobians[id]->getJacobianInverse();
        } else {
            J = helperJacobians[id]->getJacobianTranspose();
        }
    } else {
        J = helperJacobians[id]->getJacobian();
    }
    //cout << "Jacobian found, at id:%d, inverse=%d, size=%dx%d\n", id, (int)jacobianInverseStore[id], J.rows(), J.cols());

    /*
    if(outSpace=="configuration") {
        exit(0);
    }
    */
    return J;
}


bool CB::RunnableControlLaw::connectToDevicePorts() {

    bool ok = true;

    string devicePortName;
    string outputPortName;
    string deviceLockPortName;
    string outputLockPortName;

    if(connectedToDevices) return ok;
    devicePorts.clear();
    deviceLockPorts.clear();

    for(int i=0; i<Vout.size(); i++) {

        devicePortName = "/cb/configuration" + VoutDeviceNames[i] + "/data:i";
        outputPortName = "/cb/control_law" + VoutDeviceNames[i] + ":o";
        deviceLockPortName = "/cb/configuration" + VoutDeviceNames[i] + "/lock:i";
        outputLockPortName = "/cb/control_law" + VoutDeviceNames[i] + "/lock:o";

        devicePorts.push_back(new BufferedPort<Bottle>);
        cout << "RunnableControlLaw::connect() -- opening port to: " << outputPortName.c_str() << endl;
        ok &= devicePorts[i]->open(outputPortName.c_str());
        if(!ok) {
            cout << "RunnableControlLaw::connect() -- could not open port: " << outputPortName.c_str() << endl;
            return ok;
        }
        ok &= Network::connect(outputPortName.c_str(),devicePortName.c_str(), "tcp");
        if(!ok) {
            cout << "RunnableControlLaw::connect() -- could not connect to port: " << outputPortName.c_str() << endl;
            return ok;
        }
        cout << "RunnableControlLaw::connect() -- connected to port: " << outputPortName.c_str() << endl;
        
        deviceLockPorts.push_back(new BufferedPort<Bottle>);
        cout << "RunnableControlLaw::connect() -- opening port to: " << outputLockPortName.c_str() << endl;
        ok &= deviceLockPorts[i]->open(outputLockPortName.c_str());
        if(!ok) {
            cout << "RunnableControlLaw::connect() -- could not open port: " << outputLockPortName.c_str() << endl;
            return ok;
        }
        ok &= Network::connect(outputLockPortName.c_str(),deviceLockPortName.c_str(), "tcp");
        if(!ok) {
            cout << "RunnableControlLaw::connect() -- could not connect to port: " << outputLockPortName.c_str() << endl;
            return ok;
        }
        cout << "RunnableControlLaw::connect() -- connected to port: " << outputLockPortName.c_str() << endl;

    }

    connectedToDevices = true;
    return ok;
}

bool CB::RunnableControlLaw::sendOutputsToDevices() {

    //    cout << "RunnableControlLaw::sendOutputsToDevices(), n=" << devicePorts.size() << endl;
    for(int i=0; i<devicePorts.size(); i++) {
        
        Bottle &b0 = deviceLockPorts[i]->prepare();
        b0.clear();
        b0.addInt(0);        
        deviceLockPorts[i]->write();
        
        Bottle &b1 = devicePorts[i]->prepare();
        b1.clear();
        for(int k=0; k<VoutConfig[i].size(); k++) {
            b1.addDouble(VoutConfig[i][k]);
        }
        devicePorts[i]->write();

        Bottle &b2 = deviceLockPorts[i]->prepare();
        b2.clear();
        b2.addInt(1);        
        deviceLockPorts[i]->write();

    }

}

void CB::RunnableControlLaw::resetControlLaw() {

    string devicePortName;
    string outputPortName;
    string deviceLockPortName;
    string outputLockPortName;

    cout << "RunnableControlLaw() -- reset()" << endl;

    // make sure teh devices are locked
    for(int i=0; i<devicePorts.size(); i++) {
        Bottle &b = deviceLockPorts[i]->prepare();
        b.clear();
        b.addInt(1);        
        deviceLockPorts[i]->write();
    }

    cout << "RunnableControlLaw() -- reset() deleting controllers" << endl;
    for(int i=0; i<controllers.size(); i++) 
        controllers[i]->stopAction();
    controllers.clear();

    cout << "RunnableControlLaw() -- reset() deleting jacobians" << endl;
    for(int i=0; i<helperJacobians.size(); i++) 
        helperJacobians[i]->stopJacobian();
    helperJacobians.clear();

    cout << "RunnableControlLaw() -- reset() disconnecting ports" << endl;
    for(int i=0; i<Vout.size(); i++) {
        devicePortName = "/cb/configuration" + VoutDeviceNames[i] + "/data:i";
        outputPortName = "/cb/control_law" + VoutDeviceNames[i] + ":o";
        deviceLockPortName = "/cb/configuration" + VoutDeviceNames[i] + "/lock:i";
        outputLockPortName = "/cb/control_law" + VoutDeviceNames[i] + "/lock:o";
        Network::disconnect(outputPortName.c_str(),devicePortName.c_str(), "tcp");
        Network::disconnect(outputLockPortName.c_str(),deviceLockPortName.c_str(), "tcp");
        devicePorts[i]->close();
        deviceLockPorts[i]->close();
    }

    cout << "RunnableControlLaw() -- reset() clearing data structures" << endl;
    controllers.clear();
    Vout.clear();
    VoutSpaces.clear();
    VoutDeviceNames.clear();
    VoutConfig.clear();
    controllerDeviceNames.clear();
    controllerOutputSpaces.clear();
    deviceMap.clear();
    jacobianMap.clear();
    jacobianInverseStore.clear();
    devicePorts.clear();
    deviceLockPorts.clear();

    connectedToDevices = false;
    running = false;
    initiallized = false;
    numControllers = 0;

    cout << "RunnableControlLaw() -- reset() done..." << endl;
}
