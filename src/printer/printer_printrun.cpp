/*
    This file is a part of the RepSnapper project.
    Copyright (C) 2012 martin.dieringer@gmx.de

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/


#include "printer_printrun.h"
#include "printcore_py.h"


// passing const char to Python ...
#define CHARS(var,str) char var[sizeof(str)]; strncpy(var, str, sizeof(str)); var[sizeof(var) - 1] = '\0'



Printer::Printer(View *view)
 :   printing (false),
     lastdonelines(0),
     lasttimeshown(0),
     debug_output(false),
     m_model(NULL),
     inhibit_print (false)
{
  gcode_iter = NULL;
  m_view = view;
  //device = NULL;

  Py_Initialize();

#define FROMFILE 0
#if FROMFILE
  cerr << "running file printcore.py ..." << endl;
  PyRun_SimpleString("import sys\nsys.path.append('.')\n");
  pyName = PyString_FromString("printcore");
  pyModule = PyImport_Import(pyName);
#else
  //cerr << printcore_py << endl;
  if (PyRun_SimpleString(printcore_py) != 0)  {
    cerr << "Could not run printcore python code" << endl;
    return;
  }
  pyModule = PyImport_AddModule("__main__");
#endif

  if (!pyModule)
    cerr << "error loading module" << endl;

  PyRun_SimpleString("print 'Python Version',sys.version\n");

  //cerr << "mo " << pyModule << endl;
  pyDict = PyModule_GetDict(pyModule);

  // Create an instance
  CHARS (cl, "printcore");
  PyObject *pyClass = PyDict_GetItemString(pyDict, cl);
  if (PyCallable_Check(pyClass)) {
    pyPrintCore = PyObject_CallObject(pyClass, NULL);
  }
  if (!pyPrintCore)
    cerr << "error loading core module" << endl;
}

Printer::~Printer()
{
  Stop();
  serial_try_connect(false);
  if (Py_IsInitialized())
    Py_Finalize();
}


bool Printer::do_connect (bool connect)
{
  if (connect) {

    if (pyPrintCore) {
      const string port = m_model->settings.Hardware.PortName;
      CHARS (meth, "connect");
      CHARS (format, "(si)");
      const int speed = m_model->settings.Hardware.SerialSpeed;
      PyObject *pValue =
	PyObject_CallMethod(pyPrintCore, meth, format,
			    port.c_str(), speed);
      if(!pValue) {
	PyErr_Print();
	return false;
      }
    }
    return true;

  } else { // disconnect

      if (pyPrintCore) {
	CHARS (meth,  "disconnect");
	PyObject *pValue =
	  PyObject_CallMethod(pyPrintCore, meth, NULL);
	if(!pValue) {
	  PyErr_Print();
	  return false;
	}
      }
      return true;
  }
}


void Printer::log(string s, RR_logtype type)
{
  switch (type) {
  case LOG_ERROR: error_buffer  +=s; break;
  case LOG_ECHO:  echo_buffer   +=s; break;
  default:
  case LOG_COMM:  commlog_buffer+=s; break;
  }
}


// void Printer::update_core_settings ()
// {
//   if (m_model) {
// #if IOCHANNEL
//     debug_output = m_model->settings.Display.CommsDebug;
// #else
//     if (device==NULL) return;
//     rr_dev_enable_debugging(device, m_model->settings.Display.CommsDebug);
// #endif

//   }
// }



void Printer::Pause()
{
  if (!IsConnected())
    return;
  if (pyPrintCore) {
    CHARS (meth, "pause");
    PyObject *pValue =
      PyObject_CallMethod(pyPrintCore, meth, NULL);
    if(!pValue) {
      PyErr_Print();
      return;
    }
  }
  set_printing (false);
}

void Printer::Continue()
{
  if (!IsConnected())
    return;
  if (pyPrintCore) {
    CHARS (meth, "resume");
    PyObject *pValue =
      PyObject_CallMethod(pyPrintCore, meth, NULL);
    if(!pValue) {
      PyErr_Print();
      return;
    }
  }
  set_printing (true);
}

PyObject * Printer::p_get_attr(const char * name) const
{
  if (!pyPrintCore) return NULL;
  PyObject *pObj = PyObject_GetAttrString(pyPrintCore, name);
  PyObject *pValue = NULL;
  if(pObj) {
    return pObj;
    pValue = PyEval_CallObject(pObj,NULL);
    //PyObject_GetAttrString(pyPrintCore, meth);
    if(!pValue) {
      cerr <<" value error: " ;
      PyErr_Print();
    }
  } else {
    cerr <<" object error: " ;
    PyErr_Print();
  }
  return pValue;
}

bool Printer::IsConnected()
{
  bool ret = false;
  if (pyPrintCore) {
    CHARS (meth, "is_online");
    PyObject *pValue =
      PyObject_CallMethod(pyPrintCore, meth, NULL);
    //PyObject *pValue = p_get_attr("online");
    if(!pValue) return false;
    long blong = PyInt_AsLong(pValue);
    ret = (blong == 1);
  }
  cerr << "online: "<< ret << endl;
  return ret;
}

void Printer::Reset()
{
  if (!IsConnected())
    return;
  // if (device==NULL) return;
  cerr << "reset?"<< endl;
  if (pyPrintCore) {
    CHARS (meth, "reset");
    PyObject *pValue =
      PyObject_CallMethod(pyPrintCore, meth, NULL);
    if(!pValue) {
      PyErr_Print();
      return;
    }
  }
  // rr_dev_reset_device(device);
}



void Printer::Print()
{
  if (!IsConnected())
    return;

  if (pyPrintCore) {
    CHARS (meth, "startprint");
    PyObject *pValue =
      PyObject_CallMethod(pyPrintCore, meth, NULL);

    if(!pValue) {
      PyErr_Print();
      return;
    }
  }

  // if (device==NULL) return;
  // if (!rr_dev_is_connected (device)) {
  //   alert (_("Not connected to printer.\nCannot start printing"));
  //   return;
  // }

  // assert(m_model != NULL);

  // delete (gcode_iter);
  // gcode_iter = m_model->gcode.get_iter();
  // set_printing (true);

  // rr_dev_reset (device);
  // rr_dev_set_paused (device, RR_PRIO_NORMAL, 0);
}


long Printer::get_next_line(string &line)
{
  return -1;
}



bool Printer::SendNow(string str, long lineno)
{
  if (!IsConnected()) {
    //   error (_("Can't send command"), _("You must first connect to a device!"));
    return false;
  }

  std::transform(str.begin(), str.end(), str.begin(), ::toupper);

  if (pyPrintCore) {
    CHARS (meth, "send_now");
    CHARS (format, "s");
    PyObject *pValue =
      PyObject_CallMethod(pyPrintCore, meth, format, str.c_str());
    if(!pValue) {
      PyErr_Print();
      return false;
    }
  }

  return true;
}

void Printer::Stop()
{
  if (!IsConnected()) {
    // alert (_("Not connected to printer.\nCannot stop printing"));
    return;
  }

  if (pyPrintCore) {
    CHARS (meth, "pause");
    PyObject *pValue =
      PyObject_CallMethod(pyPrintCore, meth, NULL);
    if(!pValue) {
      PyErr_Print();
      return;
    }
  }

  set_printing (false);
}




// never called in libreprap version
void Printer::parse_response (string line)
{
  cerr << "response: " << line << endl;
  cerr << "NOT PARSING" << endl;
}

